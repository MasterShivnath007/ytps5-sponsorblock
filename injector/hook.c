/*
 * Raw x86-64 hook installed into the YouTube app by the injector.
 *
 * This function is compiled to a position-independent blob and copied into
 * the target process. It must not reference any global data or external
 * functions. Everything it needs is in Ytps5Stuff.
 *
 * It is called as:
 *     int ytps5_pad_hook(int handle, void *data, Ytps5Stuff *stuff)
 * by a small stub that puts `stuff` in RDX.
 */
#include "../common/ytps5_stuff.h"

int ytps5_pad_hook(int handle, void *data, Ytps5Stuff *s) {
  if (!s->loaded) {
    s->loaded = 1;
    s->status = 1;

    void *module = s->dlopen_fn(s->so_path, 1 /* RTLD_LAZY */);
    if (!module) {
      s->status = 2;
    } else {
      typedef void (*install_fn)(Ytps5Stuff *);
      install_fn install = (install_fn)s->dlsym_fn(module, s->sym_name);
      if (!install) {
        s->status = 3;
      } else {
        s->status = 4;
        install(s);
        s->status = 5;
      }
    }
  }

  return s->orig_pad(handle, data);
}
