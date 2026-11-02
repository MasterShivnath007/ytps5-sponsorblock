#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string>

struct StringLikeLayout {
  bool valid = false;
  size_t data_off = 0;
  size_t size_off = 0;
  size_t cap_off = 0;
  size_t header_span = 0x40;
};

void util_init();
bool is_canonical_address(uint64_t addr);

bool try_extract_script_from_candidate(uint64_t candidate, std::string &out,
                                       StringLikeLayout *out_layout = nullptr);

uint8_t *pattern_scan(uint64_t module_base, uint32_t module_size,
                      const char *signature);
