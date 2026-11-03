/*
 * ytps5-sponsorblock injector
 *
 * Standalone PS5 ELF. It waits for the official PS5 video app, attaches to it,
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
#include <dlfcn.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

#include <ps5/kernel.h>
#include <ps5/payload.h>

#include "dbg/dbg.hpp"
#include "hijacker/hijacker.hpp"
#include "kernel/proc.hpp"
#include "notify.hpp"
#include "offsets.hpp"
#include "nid.hpp"

#include "../common/ytps5_pad.h"
#include "ytps5_direct.h"
#include "direct_hook_blob.h"
#include "payload_js.h"
#include "hde64.h"
#include "hook_blob.h"

/* libhijacker expects this global. */
uintptr_t kernel_base = 0;

/* The app imports scePadRead, not scePadReadState. */
static constexpr Nid kPadReadNid{"q1cHNfGycLI"};

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
    /* Make the last line survive a kernel panic or hard power-off. */
    fsync(fileno(g_log));
  }
}

/* Show the same progress on the TV as in the log file. */
static void notifyf(const char *fmt, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  printf_notification("%s", buf);
  logf("[notify] %s", buf);
}

static bool g_probe = false;
static bool g_no_jailbreak = false;
static bool g_no_suspend = false;
static bool g_no_attach = false;
static bool g_proctest = false;
static bool g_listtest = false;
static bool g_memtest = false;
static bool g_sotest = false;
static bool g_signedtest = false;
static bool g_legacy = false;
static bool g_resume_only = false;
static bool g_monitor = false;
static uint64_t g_state_addr = 0;
static int g_attach_delay = 0;

static void load_options() {
  FILE *fp = fopen(LOG_DIR "/options.txt", "r");
  if (!fp) return;
  char line[128];
  while (fgets(line, sizeof(line), fp)) {
    const char *eq = strchr(line, '=');
    if (strncmp(line, "state", 5) == 0) {
      g_state_addr = eq ? strtoull(eq + 1, nullptr, 0) : 0;
      continue;
    }
    int value = eq ? atoi(eq + 1) : 0;
    if (strncmp(line, "probe", 5) == 0) g_probe = value != 0;
    else if (strncmp(line, "no_jailbreak", 12) == 0) g_no_jailbreak = value != 0;
    else if (strncmp(line, "no_suspend", 10) == 0) g_no_suspend = value != 0;
    else if (strncmp(line, "delay", 5) == 0) g_attach_delay = value;
    else if (strncmp(line, "no_attach", 9) == 0) g_no_attach = value != 0;
    else if (strncmp(line, "proctest", 8) == 0) g_proctest = value != 0;
    else if (strncmp(line, "listtest", 8) == 0) g_listtest = value != 0;
    else if (strncmp(line, "memtest", 7) == 0) g_memtest = value != 0;
    else if (strncmp(line, "sotest", 6) == 0) g_sotest = value != 0;
    else if (strncmp(line, "signedtest", 10) == 0) g_signedtest = value != 0;
    else if (strncmp(line, "legacy", 6) == 0) g_legacy = value != 0;
    else if (strncmp(line, "resume_only", 11) == 0) g_resume_only = value != 0;
    else if (strncmp(line, "monitor", 7) == 0) g_monitor = value != 0;
  }
  fclose(fp);
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

static bool supported_title(const char *tid) {
  return tid && strncmp(tid, "PPSA0165", 8) == 0;
}

static bool find_supported_app(int &pid, int &appid, char *title, size_t title_size) {
  /* Fast path: the foreground big app. */
  int big = sceSystemServiceGetAppIdOfRunningBigApp();
  if (big >= 0) {
    memset(title, 0, title_size);
    if (sceSystemServiceGetAppTitleId(big, title) == 0 &&
        supported_title(title)) {
      for (int i = 0; i < 10000; ++i) {
        int candidate = 0;
        if (_sceApplicationGetAppId(i, &candidate) < 0) continue;
        if (candidate == big) {
          pid = i;
          appid = big;
          return true;
        }
      }
    }
  }

  /* The app is a media app on some firmware and may not be reported as the
   * running big app. Walk the application table instead. */
  for (int i = 0; i < 10000; ++i) {
    int candidate = 0;
    if (_sceApplicationGetAppId(i, &candidate) < 0) continue;
    char candidate_title[256] = {0};
    if (sceSystemServiceGetAppTitleId(candidate, candidate_title) != 0) {
      continue;
    }
    if (!supported_title(candidate_title)) continue;

    pid = i;
    appid = candidate;
    strncpy(title, candidate_title, title_size - 1);
    title[title_size - 1] = 0;
    return true;
  }

  return false;
}

static uint8_t *scan_buffer(uint8_t *buf, size_t size, const char *signature) {
  uint8_t pattern[256] = {0};
  uint32_t pattern_len = 0;

  const char *cur = signature;
  while (*cur) {
    if (*cur == ' ' || *cur == '\t' || *cur == '\n' || *cur == '\r') {
      ++cur;
      continue;
    }
    if (*cur == '?') {
      ++cur;
      if (*cur == '?') ++cur;
      pattern[pattern_len++] = 0xff;
    } else {
      char *next = nullptr;
      unsigned long value = strtoul(cur, &next, 16);
      if (next == cur) return nullptr;
      pattern[pattern_len++] = (uint8_t)value;
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
      "55 48 89 E5 41 57 41 56 41 55 41 54 53 48 83 EC",
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
   * Cobalt/app code. This keeps the common case fast. */
  uint64_t addr = try_lib(h->getEboot(), true);
  if (addr) return addr;

  for (auto lib : h->getLibs()) {
    std::string path = lib->getPath().c_str();
    if (path.find("cobalt") == std::string::npos &&
        path.find("Cobalt") == std::string::npos &&
        path.find("NpSns") == std::string::npos &&
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

static void emit_abs_jmp(uint8_t *dst, uint64_t target) {
  /* FF 25 00 00 00 00 [qword target] */
  dst[0] = 0xFF;
  dst[1] = 0x25;
  dst[2] = 0;
  dst[3] = 0;
  dst[4] = 0;
  dst[5] = 0;
  memcpy(dst + 6, &target, sizeof(target));
}

static size_t prologue_len(const uint8_t *code, size_t code_size,
                           size_t min_size) {
  size_t total = 0;
  while (total < min_size && total + 16 <= code_size) {
    hde64s hs;
    uint32_t len = hde64_disasm(code + total, &hs);
    if ((hs.flags & F_ERROR) || len == 0) return 0;
    total += len;
  }
  return total >= min_size ? total : 0;
}

static uintptr_t resolve_in_libs(Hijacker *h, const char *needle,
                                 const Nid &nid) {
  for (auto lib : h->getLibs()) {
    std::string path = lib->getPath().c_str();
    if (path.find(needle) == std::string::npos) continue;
    uintptr_t addr = h->getFunctionAddress(lib.get(), nid);
    if (addr) return addr;
  }
  return 0;
}

static bool find_pad_got(Hijacker *h, uintptr_t &got) {
  for (auto lib : h->getLibs()) {
    auto meta = lib->getMetaData();
    if (!meta) continue;
    ssize_t index = meta->getSymbolTable().getSymbolIndex(kPadReadNid);
    if (index < 0) continue;
    for (const auto &plt : meta->getPltTable()) {
      if ((ssize_t)ELF64_R_SYM(plt.r_info) == index) {
        got = lib->imagebase() + plt.r_offset;
        return true;
      }
    }
  }
  return false;
}

/*
 * Patch HTMLScriptExecute directly. No code is loaded through dlopen; the
 * injector writes a tiny position-independent hook and a trampoline into
 * executable memory that already belongs to the app.
 */
static int run_direct_hook(Hijacker *h, int pid, const char *title) {
  (void)title;
  bool suspended = false;
  auto resume_app = [&]() {
    if (suspended) {
      h->resume();
      suspended = false;
      logf("direct: resumed");
    }
  };

  if (!g_no_suspend) {
    h->suspend();
    suspended = true;
    logf("direct: app suspended for patching");
  }

  notifyf("ytps5: scanning for hook target");

  std::string where;
  uint64_t target = find_html_execute(h, where);
  if (!target) {
    logf("direct: HTMLScriptExecute not found");
    notifyf("ytps5: target not found");
    resume_app();
    return 2;
  }
  logf("direct: target=%p (%s)", (void *)target,
       where.empty() ? "unknown" : where.c_str());
  notifyf("ytps5: target 0x%llx", (unsigned long long)target);

  uint8_t head[32] = {0};
  if (!h->read(target, head, sizeof(head))) {
    logf("direct: cannot read target prologue");
    notifyf("ytps5: read failed");
    resume_app();
    return 3;
  }

  size_t stolen = prologue_len(head, sizeof(head), 14);
  if (!stolen || stolen > 32) {
    logf("direct: invalid prologue length %zu", stolen);
    notifyf("ytps5: bad prologue");
    resume_app();
    return 3;
  }

  uint8_t original[32] = {0};
  memcpy(original, head, stolen);

  const uintptr_t page = 0x1000;
  uintptr_t page_start = (uintptr_t)target & ~(page - 1);
  uintptr_t page_end = ((uintptr_t)target + stolen + page - 1) & ~(page - 1);
  int rc = kernel_mprotect(pid, (intptr_t)page_start, page_end - page_start,
                           PROT_READ | PROT_WRITE | PROT_EXEC);
  logf("direct: kernel_mprotect(%p, 0x%zx) -> %d", (void *)page_start,
       (size_t)(page_end - page_start), rc);
  if (rc != 0) {
    notifyf("ytps5: mprotect failed");
    resume_app();
    return 4;
  }

  uintptr_t malloc_addr =
      resolve_in_libs(h, "libSceLibcInternal", Nid("gQX+4GDQjpM"));
  if (!malloc_addr) {
    malloc_addr = resolve_in_libs(h, "libc.prx", Nid("gQX+4GDQjpM"));
  }
  uintptr_t free_addr =
      resolve_in_libs(h, "libSceLibcInternal", Nid("tIhsqj0qsFE"));
  if (!free_addr) {
    free_addr = resolve_in_libs(h, "libc.prx", Nid("tIhsqj0qsFE"));
  }
  logf("direct: malloc=%p free=%p", (void *)malloc_addr, (void *)free_addr);
  if (!malloc_addr) {
    notifyf("ytps5: malloc not found");
    resume_app();
    return 5;
  }

  std::string payload = "try{\n";
  payload.append((const char *)ytps5_payload_js, ytps5_payload_js_len);
  payload.append("\n}catch(e){}\n");
  size_t payload_len = payload.size();

  auto state_addr = h->getDataAllocator().allocate(
      sizeof(Ytps5Direct) + payload_len + 1);
  auto tramp_addr = h->getTextAllocator().allocate(stolen + 32);
  auto hook_addr =
      h->getTextAllocator().allocate(15 + ytps5_direct_blob_len + 32);
  if (!state_addr || !tramp_addr || !hook_addr) {
    logf("direct: allocation failed state=%p tramp=%p hook=%p",
         (void *)state_addr, (void *)tramp_addr, (void *)hook_addr);
    notifyf("ytps5: allocation failed");
    resume_app();
    return 6;
  }

  Ytps5Direct st{};
  st.malloc_fn = (void *(*)(size_t))malloc_addr;
  st.free_fn = (void (*)(void *))free_addr;
  st.orig_execute = tramp_addr;
  st.payload = (char *)(state_addr + sizeof(Ytps5Direct));
  st.payload_len = payload_len;

  uint8_t tramp[64] = {0};
  memcpy(tramp, original, stolen);
  emit_abs_jmp(tramp + stolen, target + stolen);
  size_t tramp_len = stolen + 14;

  uint8_t stub[15] = {0};
  stub[0] = 0x49; /* movabs r8, imm64 */
  stub[1] = 0xB8;
  memcpy(stub + 2, &state_addr, sizeof(state_addr));
  uintptr_t blob_addr = hook_addr + sizeof(stub);
  stub[10] = 0xE9; /* jmp rel32 */
  int32_t rel = (int32_t)(blob_addr - (hook_addr + sizeof(stub)));
  memcpy(stub + 11, &rel, sizeof(rel));

  uint8_t patch[32];
  memset(patch, 0x90, sizeof(patch));
  emit_abs_jmp(patch, hook_addr);

  bool html_wrote =
      h->write(state_addr, &st, sizeof(st)) &&
      h->write(state_addr + sizeof(Ytps5Direct), payload.data(), payload_len) &&
      h->write(tramp_addr, tramp, tramp_len) &&
      h->write(hook_addr, stub, sizeof(stub)) &&
      h->write(blob_addr, ytps5_direct_blob, ytps5_direct_blob_len);
  if (!html_wrote) {
    logf("direct: html state/blob write failed (state=%p tramp=%p hook=%p)",
         (void *)state_addr, (void *)tramp_addr, (void *)hook_addr);
    notifyf("ytps5: write failed");
    return 8;
  }

  if (!h->write(target, patch, stolen)) {
    logf("direct: target write failed at %p", (void *)target);
    notifyf("ytps5: target write failed");
    return 8;
  }

  logf("direct: installed target=%p stolen=%zu tramp=%p hook=%p state=%p",
       (void *)target, stolen, (void *)tramp_addr, (void *)hook_addr,
       (void *)state_addr);
  notifyf("ytps5: hook installed");
  resume_app();

  int last_calls = -1;
  int last_injected = -1;
  for (int i = 0; i < 30; ++i) {
    sleep(1);
    Ytps5Direct now{};
    if (h->read(state_addr, &now, sizeof(now))) {
      if ((int)now.calls != last_calls ||
          (int)now.injected != last_injected) {
        logf("direct: calls=%llu injected=%llu skipped=%llu errors=%llu "
             "last_size=%llu last_total=%llu",
             (unsigned long long)now.calls,
             (unsigned long long)now.injected,
             (unsigned long long)now.skipped,
             (unsigned long long)now.errors,
             (unsigned long long)now.last_size,
             (unsigned long long)now.last_total);
        last_calls = (int)now.calls;
        last_injected = (int)now.injected;
      }
      if (now.injected > 0) break;
    }
  }

  if (last_injected > 0) {
    notifyf("ytps5: %d script(s) injected", last_injected);
  } else {
    notifyf("ytps5: hook installed, waiting for scripts");
  }
  return 0;
}

static bool is_y2jb_title(const char *tid) {
  return tid && (strcmp(tid, "PPSA01650") == 0 ||
                 strcmp(tid, "PPSA01651") == 0);
}

int main() {
  log_open();
  load_options();

  payload_args_t *args = payload_get_args();
  kernel_base = args->kdata_base_addr;

  logf("ytps5 injector starting (pid=%d kernel_base=%p)", getpid(),
       (void *)kernel_base);
  logf("options probe=%d no_jailbreak=%d no_suspend=%d delay=%d", g_probe,
       g_no_jailbreak, g_no_suspend, g_attach_delay);

  logf("kernel addresses: arg=%p sdk_data=%p sdk_text=%p sdk_allproc=%p",
       (void *)args->kdata_base_addr, (void *)KERNEL_ADDRESS_DATA_BASE,
       (void *)KERNEL_ADDRESS_TEXT_BASE, (void *)KERNEL_ADDRESS_ALLPROC);
  logf("payload pipes: rwpipe=%p rwpair=%p kpipe=%p",
       (void *)args->rwpipe, (void *)args->rwpair, (void *)args->kpipe_addr);
  logf("libhijacker offsets: allproc=0x%zx security_flags=0x%zx "
       "root_vnode=0x%zx qa_flags=0x%zx",
       offsets::allproc(), offsets::security_flags(), offsets::root_vnode(),
       offsets::qa_flags());
  logf("sdk rootvnode=%p libhijacker_rootvnode=%p", (void *)KERNEL_ADDRESS_ROOTVNODE,
       (void *)(kernel_base + offsets::root_vnode()));

  {
    uint64_t allproc_value = 0;
    int rc = kernel_copyout((uintptr_t)KERNEL_ADDRESS_ALLPROC, &allproc_value,
                            sizeof(allproc_value));
    logf("[stage] kernel_copyout(allproc) rc=%d value=%p", rc,
         (void *)allproc_value);
  }

  if (g_sotest) {
    logf("[sotest] loading " LOG_DIR "/sponsorblock.so in the payload process");
    dlerror();
    void *so = dlopen(LOG_DIR "/sponsorblock.so", RTLD_LAZY);
    const char *err = dlerror();
    logf("[sotest] handle=%p error=%s", so, err ? err : "none");
    if (so) {
      void *sym = dlsym(so, "ytps5_install");
      logf("[sotest] ytps5_install=%p", sym);
    }
  }

  if (g_proctest) {
    logf("[stage] proctest: getProc(self)");
    auto self = getProc(getpid());
    logf("[stage] getProc(self)=%p", self.get());
    if (self) {
      logf("[stage] self pid=%d title=%s", self->pid(),
           self->titleId().c_str());
    }
    logf("[stage] proctest done");
  }

  if (g_listtest) {
    static uint8_t procbuf[0xc90];
    uintptr_t head = (uintptr_t)KERNEL_ADDRESS_ALLPROC;
    logf("[listtest] head=%p", (void *)head);
    int rc = kernel_copyout(head, procbuf, sizeof(procbuf));
    logf("[listtest] head copy rc=%d first=%p", rc,
         (void *)*(uintptr_t *)procbuf);

    uintptr_t addr = *(uintptr_t *)procbuf;
    for (int i = 0; i < 256 && addr; ++i) {
      if ((addr >> 48) != 0xffff) {
        logf("[listtest] node %d non-canonical %p, stopping", i, (void *)addr);
        break;
      }
      logf("[listtest] node %d addr=%p (about to read)", i, (void *)addr);
      rc = kernel_copyout(addr, procbuf, sizeof(procbuf));
      int pid = *(int *)(procbuf + 0xbc);
      uintptr_t next = *(uintptr_t *)procbuf;
      logf("[listtest] node %d rc=%d pid=%d next=%p", i, rc, pid,
           (void *)next);
      if (pid == getpid()) {
        logf("[listtest] found self at node %d", i);
        break;
      }
      addr = next;
    }
    logf("[listtest] done");
  }

  if (g_memtest) {
    uintptr_t first = 0;
    kernel_copyout((uintptr_t)KERNEL_ADDRESS_ALLPROC, &first, sizeof(first));
    logf("[memtest] first proc=%p", (void *)first);

    logf("[memtest] heap copy starting");
    uint8_t *heap = (uint8_t *)malloc(0xc90);
    int rc = kernel_copyout(first, heap, 0xc90);
    logf("[memtest] heap rc=%d pid=%d next=%p", rc,
         *(int *)(heap + 0xbc), (void *)*(uintptr_t *)heap);
    free(heap);

    logf("[memtest] stack KProc starting");
    KProc stack_proc{first};
    logf("[memtest] stack pid=%d", stack_proc.pid());

    logf("[memtest] new KProc starting");
    auto heap_proc = new KProc{first};
    logf("[memtest] new pid=%d", heap_proc->pid());
    delete heap_proc;

    logf("[memtest] done");
  }

  if (g_no_attach) {
    logf("[stage] no_attach set, exiting without attaching");
    return 0;
  }

  uint32_t sdk_version = 0;
  size_t sdk_size = sizeof(sdk_version);
  if (sysctlbyname("kern.sdk_version", &sdk_version, &sdk_size, nullptr, 0) == 0) {
    logf("firmware sdk_version=0x%x", sdk_version);
  logf("payload bytes=%u direct_blob=%u", ytps5_payload_js_len,
       ytps5_direct_blob_len);
  } else {
    logf("firmware sdk_version unavailable");
  }

  int last_pid = -1;
  for (;;) {
    int pid = -1;
    int appid = -1;
    char title[256] = {0};
    notifyf("ytps5: waiting for the supported app");

    for (int attempt = 0; ; ++attempt) {
      if (find_supported_app(pid, appid, title, sizeof(title))) break;
      if (attempt == 0 || (attempt % 30) == 0) {
        logf("waiting for app (appid=%d title=%s)", appid,
             title[0] ? title : "none");
      }
      sleep(2);
      pid = -1;
    }

    logf("found title=%s appid=%d pid=%d", title, appid, pid);
    if (is_y2jb_title(title)) {
      logf("refusing Y2JB title %s", title);
      notifyf("ytps5: %s is a Y2JB app - NOT patching", title);
      while (find_supported_app(pid, appid, title, sizeof(title)) &&
             is_y2jb_title(title)) {
        sleep(3);
      }
      continue;
    }
    if (pid == last_pid) {
      sleep(2);
      continue;
    }
    last_pid = pid;
    notifyf("ytps5: %s is clean - patching", title);

    if (g_attach_delay > 0) {
      logf("waiting %ds for the app to finish loading", g_attach_delay);
      sleep(g_attach_delay);
      int pid2 = -1;
      int appid2 = -1;
      char title2[256] = {0};
      if (!find_supported_app(pid2, appid2, title2, sizeof(title2))) {
        last_pid = -1;
        continue;
      }
      if (pid2 != pid) pid = pid2;
    }

    notifyf("ytps5: attaching");
    UniquePtr<Hijacker> hijacker = nullptr;
    for (int attempt = 0; attempt < 50 && !hijacker; ++attempt) {
      hijacker = Hijacker::getHijacker(pid);
      if (!hijacker) usleep(200000);
    }
    if (!hijacker) {
      logf("Hijacker::getHijacker failed after retries");
      notifyf("ytps5: attach failed");
      sleep(2);
      last_pid = -1;
      continue;
    }
    notifyf("ytps5: attached");

    if (g_resume_only) {
      dbg::resume(pid);
      logf("resume_only: dbg::resume(%d) sent", pid);
      notifyf("ytps5: app resumed");
      return 0;
    }

    if (g_monitor) {
      if (!g_state_addr) {
        logf("monitor: no state address in options");
        notifyf("ytps5: monitor needs state=");
        return 0;
      }
      Ytps5Direct now{};
      if (hijacker->read(g_state_addr, &now, sizeof(now))) {
        logf("monitor: calls=%llu injected=%llu skipped=%llu errors=%llu "
             "last_self=%p last_size=%llu last_total=%llu",
             (unsigned long long)now.calls,
             (unsigned long long)now.injected,
             (unsigned long long)now.skipped,
             (unsigned long long)now.errors, (void *)now.last_self,
             (unsigned long long)now.last_size,
             (unsigned long long)now.last_total);
        notifyf("ytps5: calls=%llu injected=%llu",
                (unsigned long long)now.calls,
                (unsigned long long)now.injected);
      } else {
        logf("monitor: cannot read state at %p", (void *)g_state_addr);
        notifyf("ytps5: monitor read failed");
      }
      return 0;
    }

    int result = run_direct_hook(hijacker.get(), pid, title);
    logf("direct hook result=%d", result);

    /* Stay resident: wait for this app to close, then patch the next launch. */
    for (;;) {
      int p2 = -1;
      int a2 = -1;
      char t2[256] = {0};
      if (!find_supported_app(p2, a2, t2, sizeof(t2))) break;
      if (p2 != pid) break;
      sleep(2);
    }
    last_pid = -1;
    sleep(1);
  }
}
