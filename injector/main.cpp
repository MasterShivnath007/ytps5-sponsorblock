/*
 * ytps5-sponsorblock injector
 *
 * Standalone PS5 ELF. It waits for the official YouTube app, attaches to it,
 * hooks scePadReadState with a tiny raw hook, and lets that hook dlopen()
 * sponsorblock.so inside the app process.
 */
#include <cstddef>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <ps5/kernel.h>
#include <ps5/payload.h>

#include "hijacker/hijacker.hpp"
#include "nid.hpp"

#include "../common/ytps5_stuff.h"
#include "hook_blob.h"

/* libhijacker expects this global. */
uintptr_t kernel_base = 0;

#define LOG_DIR "/data/ytps5-sponsorblock"
#define LOG_FILE LOG_DIR "/injector.log"

static FILE *g_log = nullptr;

extern "C" int sceKernelDebugOutText(int channel, const char *text);

static void log_open() {
  mkdir(LOG_DIR, 0777);
  g_log = fopen(LOG_FILE, "a");
}

static void logf(const char *fmt, ...) {
  char buf[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  sceKernelDebugOutText(0, buf);
  if (g_log) {
    fprintf(g_log, "%s\n", buf);
    fflush(g_log);
  }
}

extern "C" {
int sceSystemServiceGetAppIdOfRunningBigApp(void);
int sceSystemServiceGetAppTitleId(int app_id, char *title_id);
int _sceApplicationGetAppId(int pid, int *app_id);
int sceKernelPrepareToSuspendProcess(pid_t pid);
int sceKernelSuspendProcess(pid_t pid);
int sceKernelPrepareToResumeProcess(pid_t pid);
int sceKernelResumeProcess(pid_t pid);
int sceKernelDebugOutText(int channel, const char *text);
}

static bool youtube_title(const char *tid) {
  return tid && strncmp(tid, "PPSA0165", 8) == 0;
}

static bool find_youtube(int &pid, int &appid, char *title, size_t title_size) {
  appid = sceSystemServiceGetAppIdOfRunningBigApp();
  if (appid < 0) return false;

  memset(title, 0, title_size);
  if (sceSystemServiceGetAppTitleId(appid, title) != 0) return false;
  if (!youtube_title(title)) return false;

  pid = -1;
  for (int i = 0; i < 10000; ++i) {
    int candidate = 0;
    if (_sceApplicationGetAppId(i, &candidate) < 0) continue;
    if (candidate == appid) {
      pid = i;
      break;
    }
  }
  return pid >= 0;
}

static uint8_t *scan_buffer(uint8_t *buf, size_t size, const char *signature) {
  uint8_t pattern[256] = {0};
  uint32_t pattern_len = 0;

  const char *cur = signature;
  while (*cur) {
    if (*cur == '?') {
      ++cur;
      if (*cur == '?') ++cur;
      pattern[pattern_len++] = 0xff;
    } else {
      char *next = nullptr;
      pattern[pattern_len++] = (uint8_t)strtoul(cur, &next, 16);
      cur = next;
    }
    if (pattern_len >= sizeof(pattern)) return nullptr;
  }

  for (size_t i = 0; i + pattern_len < size; ++i) {
    bool ok = true;
    for (uint32_t j = 0; j < pattern_len; ++j) {
      if (pattern[j] != 0xff && buf[i + j] != pattern[j]) {
        ok = false;
        break;
      }
    }
    if (ok) return buf + i;
  }
  return nullptr;
}

static uint64_t scan_library(Hijacker *h, SharedLib *lib, const char *signature,
                              size_t *scanned_len) {
  const auto *text = lib->getTextSection();
  if (!text) return 0;

  uintptr_t start = text->start();
  size_t len = text->sectionLength();
  if (!start || !len) return 0;

  /* Do not let a corrupt rtld entry send us over a huge range. */
  if (len > 0x8000000ULL) len = 0x8000000ULL;

  constexpr size_t kChunk = 2 * 1024 * 1024;
  constexpr size_t kOverlap = 256;
  size_t scanned = 0;

  for (size_t off = 0; off < len; off += kChunk) {
    size_t take = kChunk + kOverlap;
    if (off + take > len) take = len - off;
    if (take < 32) break;

    auto data = h->read(start + off, take);
    if (!data) {
      logf("read failed at %p+0x%zx", (void *)(start + off), take);
      break;
    }
    uint8_t *hit = scan_buffer(data.get(), take, signature);
    scanned += take;
    if (hit) {
      if (scanned_len) *scanned_len = scanned;
      return (uint64_t)(start + off + (uintptr_t)(hit - data.get()));
    }
  }

  if (scanned_len) *scanned_len = scanned;
  return 0;
}

static uint64_t find_html_execute(Hijacker *h, std::string &where) {
  static const char *sigs[] = {
      "55 48 89 E5 41 57 41 56 41 55 41 54 53 48 81 EC D8 00 00 00 "
      "49 89 D5 48 8B 15 ?? ?? ?? ?? 48 8B 02 48 89 45 D0 "
      "80 BF 10 05 00 00 00",
      "55 48 89 E5 41 57 41 56 41 55 41 54 53 48 81 EC ?? ?? ?? ?? "
      "49 89 D5 48 8B 15 ?? ?? ?? ?? 48 8B 02 48 89 45 D0",
      "48 89 5C 24 ?? 48 89 6C 24 ?? 48 89 74 24 ?? 57 41 54 41 55 "
      "41 56 41 57 48 81 EC",
      "F3 0F 1E FA 55 48 89 E5 41 57 41 56 41 55 41 54 53 48 81 EC "
      "?? ?? ?? ?? 49 89 D5",
  };

  auto try_lib = [&](SharedLib *lib, bool verbose) -> uint64_t {
    if (!lib) return 0;
    std::string path = lib->getPath().c_str();
    if (path.empty()) path = "<eboot>";

    for (const char *sig : sigs) {
      size_t scanned = 0;
      uint64_t addr = scan_library(h, lib, sig, &scanned);
      if (addr) {
        logf("HTMLScriptExecute candidate at %p in %s (scanned 0x%zx)",
             (void *)addr, path.c_str(), scanned);
        where = path;
        return addr;
      }
      if (verbose) {
        logf("scanned %s without match (0x%zx bytes)", path.c_str(), scanned);
      }
    }
    return 0;
  };

  /* First pass: the main executable and the modules that normally carry
   * Cobalt/YouTube code. This keeps the common case fast. */
  uint64_t addr = try_lib(h->getEboot(), true);
  if (addr) return addr;

  for (auto lib : h->getLibs()) {
    std::string path = lib->getPath().c_str();
    if (path.find("cobalt") == std::string::npos &&
        path.find("Cobalt") == std::string::npos &&
        path.find("YouTube") == std::string::npos &&
        path.find("youtube") == std::string::npos &&
        path.find("Abstract") == std::string::npos) {
      continue;
    }
    addr = try_lib(lib.get(), true);
    if (addr) return addr;
  }

  /* Second pass: everything else. */
  logf("no match in primary modules; scanning all modules");
  for (auto lib : h->getLibs()) {
    addr = try_lib(lib.get(), false);
    if (addr) {
      logf("matched in secondary scan: %s", where.c_str());
      return addr;
    }
  }

  where.clear();
  return 0;
}

int main() {
  log_open();

  payload_args_t *args = payload_get_args();
  kernel_base = args->kdata_base_addr;

  logf("ytps5 injector starting (pid=%d kernel_base=%p)", getpid(),
       (void *)kernel_base);

  int pid = -1;
  int appid = -1;
  char title[256] = {0};

  for (int attempt = 0; attempt < 600; ++attempt) {
    if (find_youtube(pid, appid, title, sizeof(title))) break;
    if (attempt == 0 || (attempt % 10) == 0) {
      logf("waiting for YouTube (appid=%d title=%s)", appid,
           title[0] ? title : "none");
    }
    sleep(2);
    pid = -1;
  }

  if (pid < 0) {
    logf("YouTube was not detected, exiting");
    return 1;
  }
  logf("found YouTube title=%s appid=%d pid=%d", title, appid, pid);

  auto hijacker = Hijacker::getHijacker(pid);
  if (!hijacker) {
    logf("Hijacker::getHijacker failed");
    return 2;
  }

  hijacker->jailbreak(true);
  logf("attached; suspending app");
  sceKernelPrepareToSuspendProcess(pid);
  sceKernelSuspendProcess(pid);

  auto pad = hijacker->getLib("libScePad.sprx");
  if (!pad) {
    logf("libScePad.sprx not found");
    sceKernelPrepareToResumeProcess(pid);
    sceKernelResumeProcess(pid);
    return 3;
  }

  uintptr_t pad_addr =
      hijacker->getFunctionAddress(pad.get(), nid::scePadReadState);
  if (!pad_addr) {
    logf("scePadReadState not found");
    sceKernelPrepareToResumeProcess(pid);
    sceKernelResumeProcess(pid);
    return 4;
  }

  auto libkernel = hijacker->getLibKernel();
  if (!libkernel) {
    logf("libkernel not found");
    sceKernelPrepareToResumeProcess(pid);
    sceKernelResumeProcess(pid);
    return 5;
  }
  uintptr_t dlopen_addr =
      hijacker->getFunctionAddress(libkernel, Nid("UteVS6B1ZrU"));
  uintptr_t dlsym_addr =
      hijacker->getFunctionAddress(libkernel, Nid("biY+kcVB5D4"));
  uintptr_t debug_out_addr =
      hijacker->getFunctionAddress(libkernel, nid::sceKernelDebugOutText);
  logf("pad=%p dlopen=%p dlsym=%p debug_out=%p", (void *)pad_addr,
       (void *)dlopen_addr, (void *)dlsym_addr, (void *)debug_out_addr);
  if (!dlopen_addr || !dlsym_addr) {
    logf("dlopen/dlsym not resolved");
    sceKernelPrepareToResumeProcess(pid);
    sceKernelResumeProcess(pid);
    return 5;
  }

  logf("loaded modules:");
  for (auto lib : hijacker->getLibs()) {
    const auto *text = lib->getTextSection();
    logf("  %s text=%p size=0x%zx", lib->getPath().c_str(),
         text ? (void *)text->start() : nullptr,
         text ? text->sectionLength() : 0);
  }

  std::string where;
  uint64_t html_execute = find_html_execute(hijacker.get(), where);
  logf("HTMLScriptExecute=%p (%s)", (void *)html_execute,
       where.empty() ? "not found" : where.c_str());

  int target_writable = 0;
  if (html_execute) {
    const uintptr_t page = 0x1000;
    uintptr_t start = html_execute & ~(page - 1);
    int rc = kernel_mprotect(pid, (intptr_t)start, 0x8000,
                             PROT_READ | PROT_WRITE | PROT_EXEC);
    target_writable = (rc == 0);
    logf("kernel_mprotect(%p) -> %d (writable=%d)", (void *)start, rc,
         target_writable);
  }

  auto eboot = hijacker->getEboot();
  if (!eboot) {
    logf("eboot not found");
    sceKernelPrepareToResumeProcess(pid);
    sceKernelResumeProcess(pid);
    return 6;
  }

  auto find_pad_plt = [&](SharedLib *lib, uintptr_t &got) -> bool {
    if (!lib) return false;
    auto meta = lib->getMetaData();
    if (!meta) return false;
    ssize_t index = meta->getSymbolTable().getSymbolIndex(nid::scePadReadState);
    if (index < 0) return false;
    for (const auto &plt : meta->getPltTable()) {
      if ((ssize_t)ELF64_R_SYM(plt.r_info) == index) {
        got = lib->imagebase() + plt.r_offset;
        return true;
      }
    }
    return false;
  };

  uintptr_t got_addr = 0;
  if (find_pad_plt(eboot, got_addr)) {
    logf("scePadReadState PLT found in eboot");
  } else {
    for (auto lib : hijacker->getLibs()) {
      if (lib.get() == eboot) continue;
      if (find_pad_plt(lib.get(), got_addr)) {
        logf("scePadReadState PLT found in %s", lib->getPath().c_str());
        break;
      }
    }
  }
  if (!got_addr) {
    logf("scePadReadState PLT entry not found");
    sceKernelPrepareToResumeProcess(pid);
    sceKernelResumeProcess(pid);
    return 8;
  }
  logf("scePadReadState=%p GOT=%p", (void *)pad_addr, (void *)got_addr);

  auto data_addr = hijacker->getDataAllocator().allocate(sizeof(Ytps5Stuff));
  auto hook_addr =
      hijacker->getTextAllocator().allocate(ytps5_hook_blob_len + 16);
  if (!data_addr || !hook_addr) {
    logf("target allocation failed");
    sceKernelPrepareToResumeProcess(pid);
    sceKernelResumeProcess(pid);
    return 9;
  }

  Ytps5Stuff stuff{};
  stuff.dlopen_fn = (void *(*)(const char *, int))dlopen_addr;
  stuff.dlsym_fn = (void *(*)(void *, const char *))dlsym_addr;
  stuff.orig_pad = (int (*)(int, void *))pad_addr;
  stuff.loaded = 0;
  stuff.kernel_base = (uint64_t)args->kdata_base_addr;
  stuff.html_execute_addr = html_execute;
  stuff.target_writable = (uint32_t)target_writable;
  strncpy(stuff.so_path, LOG_DIR "/sponsorblock.so", sizeof(stuff.so_path) - 1);
  strncpy(stuff.sym_name, "ytps5_install", sizeof(stuff.sym_name) - 1);
  strncpy(stuff.log_path, LOG_DIR "/ytps5.log", sizeof(stuff.log_path) - 1);
  strncpy(stuff.js_path, LOG_DIR "/inject.js", sizeof(stuff.js_path) - 1);
  stuff.magic = YTPS5_MAGIC;
  stuff.version = YTPS5_VERSION;
  stuff.debug_out = (void (*)(int, const char *))debug_out_addr;
  stuff.status = 0;

  hijacker->write(data_addr, &stuff, sizeof(stuff));
  hijacker->write(hook_addr, ytps5_hook_blob, ytps5_hook_blob_len);
  hijacker->write<uintptr_t>(got_addr, hook_addr);

  logf("hooked scePadReadState GOT %p -> %p (stuff=%p)",
       (void *)got_addr, (void *)hook_addr, (void *)data_addr);

  usleep(1000);
  sceKernelPrepareToResumeProcess(pid);
  sceKernelResumeProcess(pid);
  logf("app resumed; watching module status");

  int last_status = -1;
  for (int i = 0; i < 15; ++i) {
    sleep(1);
    int status = 0;
    if (hijacker->read(data_addr + offsetof(Ytps5Stuff, status), &status,
                       sizeof(status))) {
      if (status != last_status) {
        logf("module status = %d", status);
        last_status = status;
      }
      if (status >= 2) break;
    }
  }

  if (last_status < 2) {
    logf("scePadReadState hook did not run (status=%d)", last_status);
  } else if (last_status == 2) {
    logf("dlopen failed inside YouTube (check /data visibility)");
  } else if (last_status == 3) {
    logf("dlsym(ytps5_install) failed");
  } else {
    logf("module loaded; see /data/ytps5-sponsorblock/ytps5.log");
  }

  logf("injector done");
  return 0;
}
