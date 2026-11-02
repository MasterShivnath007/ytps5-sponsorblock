#include "util.h"
#include "mini_hook.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

static int g_mem_fd = -1;

void util_init() {
  if (g_mem_fd < 0) {
    g_mem_fd = open("/proc/curproc/mem", O_RDONLY);
    mh_log("[util] /proc/curproc/mem fd=%d", g_mem_fd);
  }
}

bool is_canonical_address(uint64_t addr) {
  return addr != 0 && (addr & 0xFFFF800000000000ull) == 0;
}

static bool read_mem(uint64_t addr, void *out, size_t len,
                     bool allow_short = false) {
  if (!is_canonical_address(addr) || !out || !len) return false;

  if (g_mem_fd >= 0) {
    ssize_t n = pread(g_mem_fd, out, len, (off_t)addr);
    if (n <= 0) return false;
    if ((size_t)n < len) {
      if (!allow_short) return false;
      memset((char *)out + n, 0, len - (size_t)n);
    }
    return true;
  }

  /* Fallback: the address came from the app's own string object. */
  memcpy(out, (const void *)addr, len);
  return true;
}

static bool looks_printable_text(const uint8_t *data, size_t len,
                                 double min_ratio) {
  if (!data || !len) return false;
  size_t printable = 0;
  for (size_t i = 0; i < len; ++i) {
    uint8_t c = data[i];
    if ((c >= 0x20 && c < 0x7F) || c == '\t' || c == '\r' || c == '\n') {
      ++printable;
    }
  }
  return (double)printable / (double)len >= min_ratio;
}

static bool read_remote_ascii(uint64_t addr, size_t max_bytes,
                              std::string &out) {
  if (!is_canonical_address(addr) || !max_bytes) return false;

  size_t fetch = std::min<size_t>(max_bytes, 2048);
  std::vector<char> buffer(fetch + 1, 0);
  if (!read_mem(addr, buffer.data(), fetch, true)) return false;
  buffer[fetch] = 0;

  size_t actual = strnlen(buffer.data(), fetch);
  if (!looks_printable_text((const uint8_t *)buffer.data(), actual, 0.55)) {
    return false;
  }
  out.assign(buffer.data(), actual);
  return true;
}

bool try_extract_script_from_candidate(uint64_t candidate, std::string &out,
                                       StringLikeLayout *out_layout) {
  out.clear();
  if (out_layout) out_layout->valid = false;
  if (!is_canonical_address(candidate)) return false;

  constexpr size_t kProbe = 0x100;
  std::array<uint8_t, kProbe> header{};
  if (!read_mem(candidate, header.data(), header.size(), true)) return false;

  auto read_u64 = [&](size_t offset) -> uint64_t {
    if (offset + sizeof(uint64_t) > header.size()) return 0;
    uint64_t value = 0;
    memcpy(&value, header.data() + offset, sizeof(value));
    return value;
  };

  auto try_pointer_and_length = [&](size_t d_off, size_t n_off,
                                    size_t c_off) -> bool {
    uint64_t data_ptr = read_u64(d_off);
    uint64_t len = read_u64(n_off);
    uint64_t cap = read_u64(c_off);
    if (!is_canonical_address(data_ptr)) return false;
    if (!len || len > (1ULL << 20)) return false;
    if (cap < len || cap > (1ULL << 20)) return false;

    std::vector<uint8_t> temp(len, 0);
    if (!read_mem(data_ptr, temp.data(), temp.size())) return false;
    size_t sample = std::min<size_t>(temp.size(), 512);
    if (!looks_printable_text(temp.data(), sample, 0.55)) return false;

    out.assign((const char *)temp.data(), temp.size());
    if (out_layout) {
      out_layout->valid = true;
      out_layout->data_off = d_off;
      out_layout->size_off = n_off;
      out_layout->cap_off = c_off;
      out_layout->header_span =
          std::max<size_t>(0x40, std::max({d_off, n_off, c_off}) + 8);
    }
    mh_log("[util] script string data@0x%zx size@0x%zx cap@0x%zx len=%llu",
           d_off, n_off, c_off, (unsigned long long)len);
    return true;
  };

  for (size_t base = 0; base + 24 <= 0x80; base += 8) {
    if (try_pointer_and_length(base, base + 8, base + 16)) return true;
  }

  const size_t offsets[] = {0x18, 0x20, 0x28, 0x30, 0x38, 0x40, 0x48,
                            0x50, 0x58, 0x60, 0x68, 0x70, 0x78, 0x80,
                            0x88, 0x90, 0x98, 0xa0, 0xa8, 0xb0, 0xb8,
                            0xc0, 0xc8, 0xd0, 0xd8, 0xe0, 0xe8, 0xf0};
  for (size_t off : offsets) {
    if (try_pointer_and_length(off, off + 8, off + 16)) return true;
  }

  for (size_t off = 0; off + sizeof(uint64_t) <= header.size();
       off += sizeof(uint64_t)) {
    uint64_t maybe_ptr = read_u64(off);
    if (read_remote_ascii(maybe_ptr, 2048, out)) {
      mh_log("[util] script string fallback at 0x%zx", off);
      if (out_layout && !out.empty()) {
        if (off + 24 <= header.size()) {
          uint64_t maybe_size = read_u64(off + 8);
          uint64_t maybe_cap = read_u64(off + 16);
          if (maybe_cap > 0 && maybe_cap <= (1ULL << 20) &&
              (maybe_size == 0 ||
               (maybe_size <= (1ULL << 20) && maybe_cap >= maybe_size))) {
            out_layout->valid = true;
            out_layout->data_off = off;
            out_layout->size_off = off + 8;
            out_layout->cap_off = off + 16;
            out_layout->header_span = std::max<size_t>(0x40, off + 24);
          }
        }
      }
      return true;
    }
  }

  return false;
}

static uint32_t pattern_to_byte(const char *pattern, uint8_t *bytes) {
  uint32_t count = 0;
  const char *current = pattern;
  const char *end = pattern + strlen(pattern);

  while (current < end) {
    if (*current == '?') {
      ++current;
      if (*current == '?') ++current;
      bytes[count++] = 0xff;
    } else {
      char *next = nullptr;
      bytes[count++] = (uint8_t)strtoul(current, &next, 16);
      current = next;
    }
  }
  return count;
}

uint8_t *pattern_scan(uint64_t module_base, uint32_t module_size,
                      const char *signature) {
  if (!module_base || !module_size || !signature) return nullptr;

  constexpr uint32_t MAX_PATTERN_LENGTH = 256;
  uint8_t patternBytes[MAX_PATTERN_LENGTH] = {0};
  int32_t patternLength = pattern_to_byte(signature, patternBytes);
  if (!patternLength || patternLength >= MAX_PATTERN_LENGTH) return nullptr;

  uint8_t *scanBytes = (uint8_t *)module_base;
  for (uint64_t i = 0; i + patternLength < module_size; ++i) {
    bool found = true;
    for (int32_t j = 0; j < patternLength; ++j) {
      if (scanBytes[i + j] != patternBytes[j] && patternBytes[j] != 0xff) {
        found = false;
        break;
      }
    }
    if (found) {
      mh_log("[util] pattern match at %p", (void *)(module_base + i));
      return &scanBytes[i];
    }
  }
  return nullptr;
}
