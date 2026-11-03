/*
 * ytps5-direct
 *
 * State shared between the standalone injector and the tiny hook that runs
 * inside the supported app process. The hook is copied into the app as a raw
 * position-independent blob; it has no libc or dynamic loader dependencies.
 * All functions it needs are resolved by the injector and stored here.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YTPS5_DIRECT_MAGIC 0x59544431u /* "YTD1" */
#define YTPS5_DIRECT_VERSION 1u

typedef struct Ytps5Direct {
  /* Resolved from the app's own libc. */
  void *(*malloc_fn)(size_t);
  void (*free_fn)(void *);

  /* Trampoline for the original HTMLScriptExecute prologue. */
  uint64_t orig_execute;

  /* Prefix prepended to every script. */
  char *payload;
  uint64_t payload_len;

  /* Diagnostics, read by the injector while it is still attached. */
  volatile uint64_t calls;
  volatile uint64_t injected;
  volatile uint64_t skipped;
  volatile uint64_t errors;
  volatile uint64_t last_self;
  volatile uint64_t last_size;
  volatile uint64_t last_total;
} Ytps5Direct;

#ifdef __cplusplus
}
#endif
