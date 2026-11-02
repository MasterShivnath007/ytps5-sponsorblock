/*
 * ytps5-sponsorblock
 *
 * Shared layout between the standalone injector and the in-process module.
 * Keep this file ABI-stable. Both sides are compiled with the same
 * PS5 payload SDK and must agree on every offset.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YTPS5_MAGIC 0x59545035u /* "YTP5" */
#define YTPS5_VERSION 1u

#define YTPS5_MAX_PATH 192
#define YTPS5_MAX_SYM 64

typedef struct Ytps5Stuff {
  /* Functions resolved from the target process' libkernel. */
  void *(*dlopen_fn)(const char *, int);
  void *(*dlsym_fn)(void *, const char *);

  /* Trampoline for the original function we hooked in the target. */
  int (*orig_pad)(int, void *);

  /* Set to 1 by the raw hook before it loads the module. */
  volatile int loaded;

  /* Kernel data base, copied from payload_get_args(). Used only as a
   * fallback when mprotect() is refused inside the app process. */
  uint64_t kernel_base;

  /* Address of Cobalt's HTMLScriptExecute, found by the injector. 0 means
   * the module should try to locate it itself. */
  uint64_t html_execute_addr;

  /* Set by the injector if it made the HTMLScriptExecute page RWX. */
  uint32_t target_writable;
  uint32_t reserved;

  char so_path[YTPS5_MAX_PATH];
  char sym_name[YTPS5_MAX_SYM];
  char log_path[YTPS5_MAX_PATH];
  char js_path[YTPS5_MAX_PATH];

  uint32_t magic;
  uint32_t version;

  /* Diagnostic-only fields used by the raw loader blob. */
  void (*debug_out)(int channel, const char *text);
  volatile int status;
  int reserved2;
} Ytps5Stuff;

#ifdef __cplusplus
}
#endif
