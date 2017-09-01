# This makefile is based on https://github.com/esp8266/source-code-examples/blob/master/example.Makefile

# Makefile for ESP8266 projects

# Output directors to store intermediate compiled files
# relative to the project directory
BUILD_BASE	= build
FW_BASE		= firmware

# base directory for the compiler
XTENSA_TOOLS_ROOT ?= $(ESP8266_BUILD)

# base directory of the ESP8266 SDK and the ESP-MESH-SDK packages
SDK_BASE	?= $(ESP8266_SDK_ROOT)
ESP_MESH_SDK_BASE ?= $(ESP8266_SDK_ROOT)/../../examples/ESP8266_MESH_DEMO

# esptool.py path and port
ESPTOOL		?= $(ESP8266_SDK_ROOT)/../esptool/esptool.py
ESPPORT		?= /dev/ttyUSB0

# name for the target project
TARGET		= app

# which modules (subdirectories) of the project to include in compiling
MODULES		= user
EXTRA_INCDIR    = include
EXTRA_LIBDIR    = lib

# libraries used in this project, mainly provided by the ESP-MESH-SDK
BASE_LIBS		= c gcc hal
EXTRA_LIBS		= crypto lwip main mesh net80211 phy pp smartconfig wpa

# compiler flags using during compilation of source files
CFLAGS		= -Os -g -O2 -Wpointer-arith -Wundef -Werror -Wl,-EL -fno-inline-functions -nostdlib -mlongcalls -mtext-section-literals  -D__ets__ -DICACHE_FLASH

# linker flags used to generate the main object file
LDFLAGS		= -nostdlib -Wl,--no-check-sections -u call_user_start -Wl,-static

# linker script used for the above linkier step
LD_SCRIPT	= eagle.app.v6.ld

# various paths from the SDK and the ESP-MESH-SDK used in this project
SDK_LDDIR	= ld
SDK_INCDIR	= include include/json driver_lib/include
SDK_LIBDIR	= lib
ESP_MESH_LIBDIR	= lib

# we create two different files for uploading into the flash
# these are the names and options to generate them
FW_FILE_1_ADDR	= 0x00000
FW_FILE_2_ADDR	= 0x10000

# select which tools to use as compiler, librarian and linker
CC		:= $(XTENSA_TOOLS_ROOT)/xtensa-lx106-elf-gcc
AR		:= $(XTENSA_TOOLS_ROOT)/xtensa-lx106-elf-ar
LD		:= $(XTENSA_TOOLS_ROOT)/xtensa-lx106-elf-gcc



####
#### no user configurable options below here
####
SRC_DIR		:= $(MODULES)
BUILD_DIR	:= $(addprefix $(BUILD_BASE)/,$(MODULES))

SRC		:= $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.c))
OBJ		:= $(patsubst %.c,$(BUILD_BASE)/%.o,$(SRC))

INCDIR	:= $(addprefix -I,$(SRC_DIR))
MODULE_INCDIR	:= $(addsuffix /include,$(INCDIR))
SDK_INCDIR	:= $(addprefix -I$(SDK_BASE)/,$(SDK_INCDIR))
EXTRA_INCDIR	:= $(addprefix -I,$(EXTRA_INCDIR))

BASE_LIBS		:= $(addprefix -l,$(BASE_LIBS))
EXTRA_LIBS		:= $(addprefix $(EXTRA_LIBDIR)/,$(addsuffix .a,$(addprefix lib,$(EXTRA_LIBS))))

ESP_MESH_SDK_LIBDIR_PATH		:= $(addprefix $(ESP_MESH_SDK_BASE)/,$(ESP_MESH_LIBDIR))
EXTRA_LIBDIR_PATH		:= $(EXTRA_LIBDIR)
SDK_LIBDIR	:= $(addprefix -L$(SDK_BASE)/,$(SDK_LIBDIR))
EXTRA_LIBDIR	:= $(addprefix -L,$(EXTRA_LIBDIR))

APP_AR		:= $(addprefix $(BUILD_BASE)/,$(TARGET)_app.a)
TARGET_OUT	:= $(addprefix $(BUILD_BASE)/,$(TARGET).out)

LD_SCRIPT	:= $(addprefix -T$(SDK_BASE)/$(SDK_LDDIR)/,$(LD_SCRIPT))

FW_FILE_1	:= $(addprefix $(FW_BASE)/,$(FW_FILE_1_ADDR).bin)
FW_FILE_2	:= $(addprefix $(FW_BASE)/,$(FW_FILE_2_ADDR).bin)

V ?= $(VERBOSE)
ifeq ("$(V)","1")
Q :=
vecho := @true
else
Q := @
vecho := @echo
endif

vpath %.c $(SRC_DIR)

define compile-objects
$1/%.o: %.c
	$(vecho) "CC $$<"
	$(Q) $(CC) $(INCDIR) $(MODULE_INCDIR) $(EXTRA_INCDIR) $(SDK_INCDIR) $(CFLAGS) -c $$< -o $$@
endef

.PHONY: all checkdirs update_libs flash device_init clean

all: update_libs checkdirs $(TARGET_OUT) $(FW_FILE_1) $(FW_FILE_2)

$(FW_BASE)/%.bin: $(TARGET_OUT) | $(FW_BASE)
	$(vecho) "FW $(FW_BASE)/"
	$(Q) $(ESPTOOL) elf2image -o $(FW_BASE)/ $(TARGET_OUT)

$(TARGET_OUT): $(APP_AR)
	$(vecho) "LD $@"
	$(Q) $(LD) $(EXTRA_LIBDIR) $(SDK_LIBDIR) $(LD_SCRIPT) $(LDFLAGS) -Wl,--start-group  $(APP_AR) $(EXTRA_LIBS) $(SDK_LIBS) $(BASE_LIBS) -Wl,--end-group -o $@

$(APP_AR): $(OBJ)
	$(vecho) "AR $@"
	$(Q) $(AR) cru $@ $^

checkdirs: $(BUILD_DIR) $(FW_BASE)

$(BUILD_DIR):
	$(Q) mkdir -p $@

$(FW_BASE):
	$(Q) mkdir -p $@

update_libs: $(EXTRA_LIBS)

$(EXTRA_LIBDIR_PATH):
	$(Q) mkdir -p $@

$(EXTRA_LIBDIR_PATH)/%.a: $(ESP_MESH_SDK_LIBDIR_PATH)/%.a
	$(Q) cp $? $@

vpath %.a $(ESP_MESH_SDK_LIBDIR_PATH)

flash: $(FW_FILE_1) $(FW_FILE_2)
	$(ESPTOOL) --port $(ESPPORT) --baud 115200 write_flash --flash_mode qio $(FW_FILE_1_ADDR) $(FW_FILE_1) $(FW_FILE_2_ADDR) $(FW_FILE_2)

device_init:
	$(ESPTOOL) --port $(ESPPORT) --baud 115200 write_flash --flash_mode qio 0x00000 $(SDK_BASE)/bin/boot_v1.6.bin 0xFC000 $(SDK_BASE)/bin/esp_init_data_default.bin 0xFE000 $(SDK_BASE)/bin/blank.bin 0xFB000 $(SDK_BASE)/bin/blank.bin

clean:
	$(Q) rm -rf $(FW_BASE) $(BUILD_BASE)

$(foreach bdir,$(BUILD_DIR),$(eval $(call compile-objects,$(bdir))))
