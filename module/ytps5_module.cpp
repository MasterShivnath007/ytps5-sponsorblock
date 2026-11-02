#include "../common/ytps5_stuff.h"
#include "mini_hook.h"
#include "util.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <ps5/kernel.h>
#include <unistd.h>

#include "payload_js.h"

typedef void (*html_execute_fn)(void *self, long arg2, uint64_t arg3,
                                char is_external);

static Ytps5Stuff *g_stuff = nullptr;
static mh_hook_t g_execute_hook{};
static html_execute_fn g_real_execute = nullptr;

static std::string g_payload;

/* The engine may keep the script object alive after HTMLScriptExecute
 * returns, so every injection owns its header and combined script. */
struct Injection {
  std::vector<uint8_t> header;
  std::string payload;
};
static std::vector<std::unique_ptr<Injection>> g_injections;

/* HTMLScriptExecute's first argument identifies the script context. Inject
 * once per context so the splash screen and the main page both get the
 * payload, while later scripts in the same context are passed through. */
static void *g_seen_self[16] = {0};
static int g_seen_count = 0;

static const char *kPrimarySig =
    "55 48 89 E5 41 57 41 56 41 55 41 54 53 48 81 EC D8 00 00 00 "
    "49 89 D5 48 8B 15 ?? ?? ?? ?? 48 8B 02 48 89 45 D0 "
    "80 BF 10 05 00 00 00";

static const char *kBroadSig =
    "55 48 89 E5 41 57 41 56 41 55 41 54 53 48 81 EC ?? ?? ?? ?? "
    "49 89 D5 48 8B 15 ?? ?? ?? ?? 48 8B 02 48 89 45 D0";

static const char *kThirdSig =
    "48 89 5C 24 ?? 48 89 6C 24 ?? 48 89 74 24 ?? 57 41 54 41 55 "
    "41 56 41 57 48 81 EC";

static const char *kEndbrSig =
    "F3 0F 1E FA 55 48 89 E5 41 57 41 56 41 55 41 54 53 48 81 EC "
    "?? ?? ?? ?? 49 89 D5";

static bool load_payload_file(const char *path, std::string &out) {
  if (!path || !path[0]) return false;
  FILE *fp = fopen(path, "rb");
  if (!fp) return false;

  if (fseek(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    return false;
  }
  long size = ftell(fp);
  if (size <= 0 || size > (2 * 1024 * 1024)) {
    fclose(fp);
    return false;
  }
  rewind(fp);
  out.resize((size_t)size);
  size_t got = fread(out.data(), 1, out.size(), fp);
  fclose(fp);
  if (got != out.size()) {
    out.clear();
    return false;
  }
  return true;
}

static uint64_t scan_maps_for_signature(const char *signature) {
  FILE *fp = fopen("/proc/curproc/map", "r");
  if (!fp) {
    mh_log("[scan] /proc/curproc/map unavailable");
    return 0;
  }

  char line[1024];
  int scanned = 0;
  while (fgets(line, sizeof(line), fp)) {
    unsigned long long start = 0, end = 0;
    char perms[8] = {0};
    if (sscanf(line, "%llx %llx %*u %*u %*llx %7s", &start, &end, perms) !=
        3) {
      continue;
    }
    if (!strstr(perms, "x")) continue;
    if (end <= start || end - start > 0x4000000ULL) continue;

    ++scanned;
    uint8_t *hit = pattern_scan(start, (uint32_t)(end - start), signature);
    if (hit) {
      fclose(fp);
      return (uint64_t)hit;
    }
  }
  fclose(fp);
  mh_log("[scan] scanned %d executable ranges", scanned);
  return 0;
}

static uint64_t find_html_execute() {
  uint64_t addr = scan_maps_for_signature(kPrimarySig);
  if (!addr) addr = scan_maps_for_signature(kBroadSig);
  if (!addr) addr = scan_maps_for_signature(kThirdSig);
  if (!addr) addr = scan_maps_for_signature(kEndbrSig);
  return addr;
}

extern "C" void ytps5_html_execute(void *self, long arg2, uint64_t arg3,
                                    char is_external) {
  html_execute_fn real = (html_execute_fn)g_execute_hook.orig_fn;
  if (!real) return;

  if (g_payload.empty()) {
    real(self, arg2, arg3, is_external);
    return;
  }

  void *context_key = self ? self : (void *)1;
  for (int i = 0; i < g_seen_count; ++i) {
    if (g_seen_self[i] == context_key) {
      real(self, arg2, arg3, is_external);
      return;
    }
  }

  StringLikeLayout layout{};
  std::string script;
  uint64_t header_addr = 0;

  if (is_canonical_address((uint64_t)arg2) &&
      try_extract_script_from_candidate((uint64_t)arg2, script, &layout)) {
    header_addr = (uint64_t)arg2;
  } else if (is_canonical_address(arg3) &&
             try_extract_script_from_candidate(arg3, script, &layout)) {
    header_addr = arg3;
  }

  if (!layout.valid || !header_addr) {
    real(self, arg2, arg3, is_external);
    return;
  }

  const size_t header_size = std::max<size_t>(0x80, layout.header_span);
  if (header_size > 0x400) {
    mh_log("[inject] header span too large: 0x%zx", header_size);
    real(self, arg2, arg3, is_external);
    return;
  }

  auto injection = std::make_unique<Injection>();
  injection->header.assign(header_size, 0);
  memcpy(injection->header.data(), (const void *)header_addr, header_size);

  injection->payload.reserve(g_payload.size() + script.size() + 2);
  injection->payload.append(g_payload);
  injection->payload.push_back('\n');
  injection->payload.append(script);

  auto write_u64 = [&](size_t off, uint64_t value) {
    if (off + sizeof(uint64_t) <= injection->header.size()) {
      memcpy(injection->header.data() + off, &value, sizeof(value));
    }
  };

  write_u64(layout.data_off, (uint64_t)injection->payload.data());
  write_u64(layout.size_off, (uint64_t)injection->payload.size());
  write_u64(layout.cap_off, (uint64_t)injection->payload.size());

  if (g_seen_count < (int)(sizeof(g_seen_self) / sizeof(g_seen_self[0]))) {
    g_seen_self[g_seen_count++] = context_key;
  }

  mh_log("[inject] context=%p script=%zu total=%zu preview=%.80s", self,
         script.size(), injection->payload.size(), script.c_str());

  void *header = injection->header.data();
  g_injections.push_back(std::move(injection));
  real(self, (long)(uintptr_t)header, arg3, 0);
}

extern "C" __attribute__((visibility("default"))) int
ytps5_install(Ytps5Stuff *stuff) {
  if (!stuff || stuff->magic != YTPS5_MAGIC) {
    return -1;
  }
  if (g_stuff) {
    mh_log("[module] already installed");
    return 0;
  }

  g_stuff = stuff;
  mh_set_kernel_base(stuff->kernel_base);
  mh_set_target_writable(stuff->target_writable);
  mh_set_log_file(stuff->log_path);
  util_init();

  mh_log("[module] install start (pid=%d, html_execute=%p, kernel_base=%p)",
         getpid(), (void *)stuff->html_execute_addr,
         (void *)stuff->kernel_base);

  if (!load_payload_file(stuff->js_path, g_payload)) {
    mh_log("[module] %s not readable, using embedded payload",
           stuff->js_path);
    g_payload.assign((const char *)ytps5_payload_js, ytps5_payload_js_len);
  }

  if (g_payload.empty()) {
    mh_log("[module] payload is empty, aborting");
    return -2;
  }
  mh_log("[module] payload size %zu", g_payload.size());

  uint64_t target = stuff->html_execute_addr;
  if (!target) {
    target = find_html_execute();
  }
  if (!target) {
    mh_log("[module] HTMLScriptExecute not found");
    return -3;
  }

  memset(&g_execute_hook, 0, sizeof(g_execute_hook));
  g_execute_hook.target_addr = target;
  g_execute_hook.user_impl = (void *)ytps5_html_execute;

  int rc = mh_install(&g_execute_hook);
  if (rc != 0) {
    mh_log("[module] mh_install failed: %d", rc);
    return rc;
  }

  g_real_execute = (html_execute_fn)g_execute_hook.orig_fn;
  mh_log("[module] ready: target=%p tramp=%p", (void *)target,
         g_execute_hook.orig_fn);
  return 0;
}

extern "C" __attribute__((visibility("default"))) int
module_start(size_t argc, const void *argv) {
  (void)argc;
  (void)argv;
  return 0;
}

extern "C" __attribute__((visibility("default"))) int
module_stop(size_t argc, const void *argv) {
  (void)argc;
  (void)argv;
  return 0;
}
