/*
 * Compact state for the scePadRead trigger hook. Kept small so it always fits
 * in the app's data-section tail alongside the JS payload.
 */
#pragma once

#include <stdint.h>

typedef struct Ytps5Pad {
  int (*orig_pad)(int handle, void *data, int num);
  uint64_t target_addr;
  uint8_t target_patch[32];
  uint32_t target_patch_len;
  uint32_t target_patched;
  volatile int loaded;
  volatile int status;
} Ytps5Pad;
