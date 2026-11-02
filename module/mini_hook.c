#include "mini_hook.h"
#include "hde64.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <ps5/kernel.h>
#include <ps5/klog.h>

int sceKernelDebugOutText(int channel, const char *text);

static uint64_t g_kernel_base = 0;
static char g_log_path[192] = {0};
static int g_target_writable = 0;

void mh_set_kernel_base(uint64_t kernel_base) { g_kernel_base = kernel_base; }
void mh_set_target_writable(int writable) { g_target_writable = writable; }
void mh_set_log_file(const char *path) {
  if (!path) return;
  strncpy(g_log_path, path, sizeof(g_log_path) - 1);
  g_log_path[sizeof(g_log_path) - 1] = 0;
}

void mh_log(const char *fmt, ...) {
  char buf[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  sceKernelDebugOutText(0, buf);

  const char *paths[2] = {g_log_path, "/download0/ytps5.log"};
  for (int i = 0; i < 2; ++i) {
    if (!paths[i][0]) continue;
    FILE *fp = fopen(paths[i], "a");
    if (fp) {
      fputs(buf, fp);
      fputc('\n', fp);
      fclose(fp);
      break;
    }
  }
}

size_t mh_calc_prologue_len(uintptr_t addr, size_t min_size) {
  size_t total = 0;
  while (total < min_size && total < MH_MAX_PROLOGUE) {
    hde64s hs;
    uint32_t len = hde64_disasm((void *)(addr + total), &hs);
    if ((hs.flags & F_ERROR) || len == 0) {
      mh_log("[hook] HDE64 decode error at %p", (void *)(addr + total));
      return 0;
    }
    total += len;
  }
  return (total >= min_size) ? total : 0;
}

static bool make_rwx(void *addr, size_t len) {
  const long page = sysconf(_SC_PAGESIZE);
  if (page <= 0) return false;

  uintptr_t start = (uintptr_t)addr & ~((uintptr_t)page - 1);
  uintptr_t end = ((uintptr_t)addr + len + (uintptr_t)page - 1) &
                  ~((uintptr_t)page - 1);

  if (mprotect((void *)start, end - start,
               PROT_READ | PROT_WRITE | PROT_EXEC) == 0) {
    return true;
  }

  if (g_kernel_base) {
    if (kernel_mprotect(getpid(), (intptr_t)start, end - start,
                        PROT_READ | PROT_WRITE | PROT_EXEC) == 0) {
      return true;
    }
  }

  mh_log("[hook] make_rwx failed for %p len 0x%zx", (void *)start,
         end - start);
  return false;
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

int mh_install(mh_hook_t *h) {
  if (!h || !h->target_addr || !h->user_impl) {
    mh_log("[hook] bad args");
    return -1;
  }
  if (h->installed) return 0;

  const size_t stolen = mh_calc_prologue_len(h->target_addr, 14);
  if (!stolen || stolen > MH_MAX_PROLOGUE) {
    mh_log("[hook] invalid prologue size %zu", stolen);
    return -2;
  }
  h->stolen_len = stolen;
  memcpy(h->original, (void *)h->target_addr, stolen);

  long page = sysconf(_SC_PAGESIZE);
  if (page <= 0) page = 0x4000;
  void *tramp = mmap(NULL, (size_t)page, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (tramp == MAP_FAILED) {
    mh_log("[hook] mmap trampoline failed");
    return -3;
  }
  h->tramp_mem = tramp;

  uint8_t tramp_buf[MH_MAX_PROLOGUE + 14] = {0};
  memcpy(tramp_buf, h->original, stolen);
  emit_abs_jmp(tramp_buf + stolen, h->target_addr + stolen);
  memcpy(tramp, tramp_buf, stolen + 14);
  h->tramp_size = stolen + 14;
  h->orig_fn = tramp;

  if (!g_target_writable &&
      !make_rwx((void *)h->target_addr, stolen + 14)) {
    mh_log("[hook] cannot make target writable");
    munmap(tramp, (size_t)page);
    h->tramp_mem = NULL;
    return -4;
  }

  uint8_t patch[MH_MAX_PROLOGUE] = {0};
  emit_abs_jmp(patch, (uint64_t)h->user_impl);
  if (stolen > 14) memset(patch + 14, 0x90, stolen - 14);
  memcpy((void *)h->target_addr, patch, stolen);
  __builtin___clear_cache((char *)h->target_addr,
                          (char *)h->target_addr + stolen);

  h->installed = 1;
  mh_log("[hook] installed at %p (stolen=%zu tramp=%p)",
         (void *)h->target_addr, stolen, tramp);
  return 0;
}

int mh_remove(mh_hook_t *h) {
  if (!h || !h->installed) return 0;
  if (make_rwx((void *)h->target_addr, h->stolen_len)) {
    memcpy((void *)h->target_addr, h->original, h->stolen_len);
    __builtin___clear_cache((char *)h->target_addr,
                            (char *)h->target_addr + h->stolen_len);
  }
  if (h->tramp_mem) {
    munmap(h->tramp_mem, (size_t)sysconf(_SC_PAGESIZE));
    h->tramp_mem = NULL;
  }
  h->installed = 0;
  return 0;
}
