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

MODULE_LIBS := -lkernel_sys
INJECTOR_LIBS := -lkernel_sys -lSceLibcInternal -lSceSystemService -lSceNet \
                 -lSceSysmodule -lSceUserService -lSceNetCtl -lSceSysCore \
                 -lScePad -lSceVideoOut -lSceAppInstUtil

.PHONY: all clean module injector payload-header hook-blob check sdk

all: $(BUILD)/ytps5-injector.elf $(BUILD)/sponsorblock.so

sdk:
	@test -x "$(CC)" || (echo "PS5_PAYLOAD_SDK is not set correctly: $(PS5_PAYLOAD_SDK)"; exit 1)
	@test -d "$(ETAHEN_DIR)/include" || (echo "ETAHEN_DIR is not set correctly: $(ETAHEN_DIR)"; exit 1)
	@command -v "$(LLVM_CONFIG)" >/dev/null || (echo "LLVM_CONFIG not found: $(LLVM_CONFIG)"; exit 1)

payload-header:
	@python3 scripts/gen_payload_header.py

hook-blob: $(BUILD)/hook.bin
	@python3 scripts/gen_hook_blob.py

$(BUILD):
	@mkdir -p $(BUILD)/module $(BUILD)/injector

# ---------------------------------------------------------------------------
# Raw hook blob (loaded into the app)
# ---------------------------------------------------------------------------
$(BUILD)/hook.o: injector/hook.c common/ytps5_stuff.h | $(BUILD)
	$(CC) -O2 -fPIC -fno-stack-protector -fno-asynchronous-unwind-tables \
		-fno-builtin -fno-function-sections -c -o $@ $<

$(BUILD)/hook.bin: $(BUILD)/hook.o
	$(OBJCOPY) -O binary --only-section=.text $< $@

# ---------------------------------------------------------------------------
# In-process module: sponsorblock.so
# ---------------------------------------------------------------------------
MODULE_SRC := module/ytps5_module.cpp module/mini_hook.c module/util.cpp module/hde64.cpp
MODULE_OBJ := $(patsubst module/%.cpp,$(BUILD)/module/%.o,$(patsubst module/%.c,$(BUILD)/module/%.o,$(MODULE_SRC)))

$(BUILD)/module/%.o: module/%.c module/mini_hook.h module/util.h module/hde64.h common/ytps5_stuff.h | payload-header $(BUILD)
	$(CC) $(MODULE_CFLAGS) $(COMMON_INC) -c -o $@ $<

$(BUILD)/module/%.o: module/%.cpp module/mini_hook.h module/util.h module/hde64.h common/ytps5_stuff.h | payload-header $(BUILD)
	$(CXX) $(MODULE_CXXFLAGS) $(COMMON_INC) -c -o $@ $<

$(BUILD)/sponsorblock.so: $(MODULE_OBJ)
	$(CXX) -shared -fPIC -o $@ $(MODULE_OBJ) $(MODULE_LIBS) -Wl,-soname=sponsorblock.so

module: payload-header $(BUILD)/sponsorblock.so

# ---------------------------------------------------------------------------
# Standalone injector
# ---------------------------------------------------------------------------
INJECTOR_SRC := injector/main.cpp module/hde64.cpp
INJECTOR_OBJ := $(patsubst injector/%.cpp,$(BUILD)/injector/%.o,$(patsubst module/%.cpp,$(BUILD)/injector/%.o,$(INJECTOR_SRC)))

$(BUILD)/injector/%.o: injector/%.cpp common/ytps5_stuff.h $(ETAHEN_DIR)/include/hijacker/hijacker.hpp | hook-blob $(BUILD)
	@mkdir -p $(dir $@)
	$(CXX) $(INJECTOR_CXXFLAGS) $(COMMON_INC) $(ETAHEN_INC) -c -o $@ $<

$(BUILD)/injector/%.o: module/%.cpp module/hde64.h | $(BUILD)
	@mkdir -p $(dir $@)
	$(CXX) $(INJECTOR_CXXFLAGS) $(COMMON_INC) -c -o $@ $<

$(BUILD)/ytps5-injector.elf: hook-blob $(INJECTOR_OBJ)
	$(CXX) -o $@ $(INJECTOR_OBJ) \
		$(ETAHEN_DIR)/libhijacker/libhijacker.a \
		$(ETAHEN_DIR)/lib/libNidResolver.a \
		$(INJECTOR_LIBS)

injector: $(BUILD)/ytps5-injector.elf

rack: all
	@ls -lh $(BUILD)/ytps5-injector.elf $(BUILD)/sponsorblock.so

clean:
	rm -rf $(BUILD) injector/hook_blob.h module/payload_js.h
