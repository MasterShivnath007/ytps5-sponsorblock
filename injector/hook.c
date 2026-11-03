/*
 * Raw x86-64 hook installed into the supported app by the injector.
 *
 * This function is compiled to a position-independent blob and copied into
 * the target process. It must not reference any global data or external
 * functions. Everything it needs is in Ytps5Pad.
 *
 * It is called as:
 *     int ytps5_pad_hook(int handle, void *data, Ytps5Pad *stuff, int num)
 * by a small stub in the injector that moves `num` to RCX, puts `stuff` in
 * RDX and jumps here. The stub preserves the caller's return address.
 *
 * On the first call it patches HTMLScriptExecute. The injector has already
 * prepared the trampoline/hook and made the target page RWX, so this write
 * happens from the input loop instead of racing a live script call.
 */
#include "../common/ytps5_pad.h"

int ytps5_pad_hook(int handle, void *data, Ytps5Pad *s, int num) {
  if (!s->loaded) {
    s->loaded = 1;
    s->status = 1;

    if (s->target_addr && s->target_patch_len <= sizeof(s->target_patch)) {
      volatile uint8_t *dst = (volatile uint8_t *)s->target_addr;
      for (uint32_t i = 0; i < s->target_patch_len; ++i) {
        dst[i] = s->target_patch[i];
      }
      __builtin___clear_cache((char *)s->target_addr,
                              (char *)s->target_addr + s->target_patch_len);
      s->target_patched = 1;
      s->status = 5;
    } else {
      s->status = 2;
    }
  }

  return s->orig_pad(handle, data, num);
}
