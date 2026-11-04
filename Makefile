# ytps5-sponsorblock build
#
# Requirements:
#   - PS5 payload SDK (John Tornblom)
#   - LLVM 18 (for llvm-config; the SDK's clang wrapper needs it)
#   - etaHEN-Plugins checkout for libhijacker + headers
#
# Example:
#   make PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk \
#        LLVM_CONFIG=/opt/homebrew/opt/llvm@18/bin/llvm-config \
#        ETAHEN_DIR=/path/to/etaHEN-Plugins

SHELL := /bin/bash

PS5_PAYLOAD_SDK ?= /opt/ps5-payload-sdk
ETAHEN_DIR      ?= third_party/etahen
LLVM_CONFIG     ?= llvm-config-18

CC      := $(PS5_PAYLOAD_SDK)/bin/prospero-clang
CXX     := $(PS5_PAYLOAD_SDK)/bin/prospero-clang++
OBJCOPY := $(PS5_PAYLOAD_SDK)/bin/prospero-objcopy

BUILD := build

COMMON_INC := -Icommon -Imodule
ETAHEN_INC := -I$(ETAHEN_DIR)/include -I$(ETAHEN_DIR)/libNidResolver/include

C_WARN  := -Wall -Wextra -Wno-unused-parameter
CXX_WARN := -Wall -Wextra -Wno-unused-parameter -Wno-format

MODULE_CFLAGS := -O2 -fPIC -fno-stack-protector -fno-asynchronous-unwind-tables -fno-builtin $(C_WARN)
MODULE_CXXFLAGS := -O2 -fPIC -fplt -fno-stack-protector -fno-asynchronous-unwind-tables \
                   -fno-exceptions -fno-rtti $(CXX_WARN)
INJECTOR_CXXFLAGS := -O2 -fPIC -fno-exceptions -fno-rtti -std=c++20 $(CXX_WARN)

MODULE_LIBS := -lkernel
INJECTOR_LIBS := -lkernel_sys -lSceLibcInternal -lSceSystemService -lSceNet \
                 -lSceSysmodule -lSceUserService -lSceNetCtl -lSceSysCore \
                 -lScePad -lSceVideoOut -lSceAppInstUtil

.PHONY: all clean injector payload-header hook-blob direct-blob installer check sdk

all: installer

sdk:
	@test -x "$(CC)" || (echo "PS5_PAYLOAD_SDK is not set correctly: $(PS5_PAYLOAD_SDK)"; exit 1)
	@test -d "$(ETAHEN_DIR)/include" || (echo "ETAHEN_DIR is not set correctly: $(ETAHEN_DIR)"; exit 1)
	@command -v "$(LLVM_CONFIG)" >/dev/null || (echo "LLVM_CONFIG not found: $(LLVM_CONFIG)"; exit 1)

minify:
	npx --yes terser payload/inject.src.js -c -m -o payload/inject.js

payload-header: payload/inject.js
	@python3 scripts/gen_payload_header.py

hook-blob: $(BUILD)/hook.bin
	@python3 scripts/gen_hook_blob.py

$(BUILD):
	@mkdir -p $(BUILD)/module $(BUILD)/injector

# ---------------------------------------------------------------------------
# Raw hook blob (loaded into the app)
# ---------------------------------------------------------------------------
$(BUILD)/hook.o: injector/hook.c common/ytps5_pad.h | $(BUILD)
	$(CC) -O2 -fPIC -fno-stack-protector -fno-asynchronous-unwind-tables \
		-fno-builtin -fno-function-sections -c -o $@ $<

$(BUILD)/hook.bin: $(BUILD)/hook.o
	$(OBJCOPY) -O binary --only-section=.text $< $@

# ---------------------------------------------------------------------------
# Direct HTMLScriptExecute hook blob (runs in the app, no dlopen)
# ---------------------------------------------------------------------------
direct-blob: $(BUILD)/direct_hook.bin
	@python3 scripts/gen_hook_blob.py build/direct_hook.bin \
		injector/direct_hook_blob.h ytps5_direct_blob ytps5_direct_blob_len

$(BUILD)/direct_hook.o: injector/direct_hook.c common/ytps5_direct.h | $(BUILD)
	$(CC) -Os -fPIC -fno-stack-protector -fno-asynchronous-unwind-tables \
		-fno-builtin -fno-function-sections -c -o $@ $<

$(BUILD)/direct_hook.bin: $(BUILD)/direct_hook.o
	$(OBJCOPY) -O binary --only-section=.text $< $@

# ---------------------------------------------------------------------------
# Standalone injector
# ---------------------------------------------------------------------------
INJECTOR_SRC := injector/main.cpp module/hde64.cpp
INJECTOR_OBJ := $(patsubst injector/%.cpp,$(BUILD)/injector/%.o,$(patsubst module/%.cpp,$(BUILD)/injector/%.o,$(INJECTOR_SRC)))

$(BUILD)/injector/%.o: injector/%.cpp common/ytps5_direct.h \
		$(ETAHEN_DIR)/include/hijacker/hijacker.hpp \
		module/payload_js.h injector/hook_blob.h injector/direct_hook_blob.h \
		| hook-blob direct-blob payload-header $(BUILD)
	@mkdir -p $(dir $@)
	$(CXX) $(INJECTOR_CXXFLAGS) $(COMMON_INC) $(ETAHEN_INC) -c -o $@ $<

$(BUILD)/injector/%.o: module/%.cpp module/hde64.h | $(BUILD)
	@mkdir -p $(dir $@)
	$(CXX) $(INJECTOR_CXXFLAGS) $(COMMON_INC) -c -o $@ $<

$(BUILD)/ytps5-injector.elf: hook-blob direct-blob $(INJECTOR_OBJ)
	$(CXX) -o $@ $(INJECTOR_OBJ) \
		$(ETAHEN_DIR)/libhijacker/libhijacker.a \
		$(INJECTOR_LIBS)

injector: $(BUILD)/ytps5-injector.elf

# ---------------------------------------------------------------------------
# Single-file installer: embeds the proxy and injector, sends both to elfldr
# ---------------------------------------------------------------------------
$(BUILD)/proxy_blob.o: $(BUILD)/ytps5-dump.elf | $(BUILD)
	cd $(BUILD) && $(OBJCOPY) -I binary -O elf64-x86-64 -B i386:x86-64 \
		ytps5-dump.elf proxy_blob.o

$(BUILD)/injector_blob.o: $(BUILD)/ytps5-injector.elf | $(BUILD)
	cd $(BUILD) && $(OBJCOPY) -I binary -O elf64-x86-64 -B i386:x86-64 \
		ytps5-injector.elf injector_blob.o

$(BUILD)/ytps5-installer.elf: injector/relay_embedded.c \
		$(BUILD)/proxy_blob.o $(BUILD)/injector_blob.o | $(BUILD)
	$(CC) -O2 -o $@ injector/relay_embedded.c \
		$(BUILD)/proxy_blob.o $(BUILD)/injector_blob.o -lkernel_sys

installer: $(BUILD)/ytps5-installer.elf

# Local SponsorBlock proxy and feed-JSON capture listener.
CURL_INC := $(PS5_PAYLOAD_SDK)/target/user/homebrew/include
CURL_LIB := $(PS5_PAYLOAD_SDK)/target/user/homebrew/lib
CURL_LIBS := -L$(CURL_LIB) -lcurl -lpsl -lssl -lcrypto -lzstd -lz -pthread

$(BUILD)/ytps5-dump.elf: injector/dump_server.c | $(BUILD)
	$(CC) -O2 -I$(CURL_INC) -o $@ $< $(CURL_LIBS) -lkernel_sys

dump: $(BUILD)/ytps5-dump.elf

dist: all
	@mkdir -p dist/tools dist/docs
	@rm -f dist/ytps5-*.zip ../ytps5-sponsorblock.zip ytps5-sponsorblock.zip
	@cp $(BUILD)/ytps5-installer.elf dist/ytps5.elf
	@cp README.md LICENSE THIRD_PARTY_NOTICES.md CREDITS.md DISCLAIMER.md TRADEMARKS.md SUPPORT.md DONATIONS.md RESEARCH.md version.json config.example.txt dist/
	@cp -R LICENSES dist//
	@cp tools/ytps5.py tools/run_test.sh dist/tools/
	@cp README.md LICENSE THIRD_PARTY_NOTICES.md dist/
	@cp docs/TESTING.md docs/HOW_IT_WORKS.md dist/docs/
	@(cd dist && zip -qr ../ytps5-sponsorblock.zip .)
	@ls -lh ytps5-sponsorblock.zip

clean:
	rm -rf $(BUILD) dist ytps5-sponsorblock.zip injector/hook_blob.h module/payload_js.h
