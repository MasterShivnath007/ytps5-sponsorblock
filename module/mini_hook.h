#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MH_MAX_PROLOGUE
#define MH_MAX_PROLOGUE 32
#endif

typedef struct mh_hook_t {
  uint64_t target_addr;
  void *user_impl;
  void *orig_fn;
  void *tramp_mem;
  size_t tramp_size;
  size_t stolen_len;
  uint8_t original[MH_MAX_PROLOGUE];
  int installed;
} mh_hook_t;

size_t mh_calc_prologue_len(uintptr_t addr, size_t min_size);
int mh_install(mh_hook_t *hook);
int mh_remove(mh_hook_t *hook);
void mh_log(const char *fmt, ...);

/* Set by the module before installing hooks. */
void mh_set_kernel_base(uint64_t kernel_base);
void mh_set_log_file(const char *path);
void mh_set_target_writable(int writable);

#ifdef __cplusplus
}
#endif
