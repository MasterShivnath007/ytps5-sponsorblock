/*
 * ytps5 direct HTMLScriptExecute hook.
 *
 * Compiled to a position-independent .text blob and copied into the supported app
 * process. The injector installs a small stub that puts the Ytps5Direct state
 * pointer in R8 and jumps here:
 *
 *     void ytps5_html_hook(void *self, void *content, void *location,
 *                          int is_external, Ytps5Direct *state);
 *
 * This file must not reference globals, string literals or external
 * functions. Everything comes from the state pointer.
 */
#include "../common/ytps5_direct.h"

typedef void (*execute_fn)(void *self, void *content, void *location,
                           int is_external);

static int user_pointer(uint64_t addr) {
  return addr >= 0x10000ULL && addr < 0x0000800000000000ULL;
}

static void copy_bytes(void *dst, const void *src, uint64_t len) {
  unsigned char *d = (unsigned char *)dst;
  const unsigned char *s = (const unsigned char *)src;
  while (len--) *d++ = *s++;
}

static int looks_like_text(const unsigned char *data, uint64_t len) {
  uint64_t sample = len < 64 ? len : 64;
  uint64_t printable = 0;
  for (uint64_t i = 0; i < sample; ++i) {
    unsigned char c = data[i];
    if ((c >= 0x20 && c < 0x7f) || c == '\t' || c == '\r' || c == '\n') {
      ++printable;
    }
  }
  return printable * 2 >= sample;
}

void ytps5_html_hook(void *self, void *content, void *location,
                     int is_external, Ytps5Direct *st) {
  execute_fn real = (execute_fn)st->orig_execute;

  st->calls++;

  if (!real || !content || !st->malloc_fn || !st->payload ||
      !st->payload_len) {
    if (real) real(self, content, location, is_external);
    return;
  }

  if (!user_pointer((uint64_t)content)) {
    st->skipped++;
    real(self, content, location, is_external);
    return;
  }

  /* Sony's std::string layout in this Cobalt build (0x28 bytes):
   *   +0x00 : allocator / flag (opaque, copied as-is)
   *   +0x08 : data pointer, or inline buffer when capacity <= 0xf
   *   +0x18 : size
   *   +0x20 : capacity
   */
  uint64_t size = *(const uint64_t *)((const char *)content + 0x18);
  uint64_t capacity = *(const uint64_t *)((const char *)content + 0x20);
  uint64_t data = capacity > 0xf
                      ? *(const uint64_t *)((const char *)content + 0x08)
                      : (uint64_t)content + 0x08;

  /* Large bundles (the ~1 MB main page) are already covered by the payload
   * installed from the earlier small bootstrap scripts. Rewriting them hits a
   * string/allocator path that corrupts memory, so leave them untouched. */
  if (!user_pointer(data) || size == 0 || size > (512ULL << 10)) {
    st->skipped++;
    real(self, content, location, is_external);
    return;
  }

  if (!looks_like_text((const unsigned char *)data, size)) {
    st->skipped++;
    real(self, content, location, is_external);
    return;
  }

  uint64_t total = st->payload_len + 1 + size;
  if (total > (8ULL << 20)) {
    st->skipped++;
    real(self, content, location, is_external);
    return;
  }

  char *buffer = (char *)st->malloc_fn((size_t)(total + 1));
  unsigned char *header = (unsigned char *)st->malloc_fn(0x28);
  if (!buffer || !header) {
    st->errors++;
    if (st->free_fn) {
      if (buffer) st->free_fn(buffer);
      if (header) st->free_fn(header);
    }
    real(self, content, location, is_external);
    return;
  }

  copy_bytes(buffer, st->payload, st->payload_len);
  buffer[st->payload_len] = '\n';
  copy_bytes(buffer + st->payload_len + 1, (const void *)data, size);
  buffer[total] = '\0';

  copy_bytes(header, content, 0x28);
  *(uint64_t *)(header + 0x08) = (uint64_t)buffer;
  *(uint64_t *)(header + 0x18) = total;
  *(uint64_t *)(header + 0x20) = total;

  st->injected++;
  st->last_self = (uint64_t)self;
  st->last_size = size;
  st->last_total = total;

  real(self, header, location, is_external);
}
