rwildcard = $(foreach d, $(wildcard $1*), $(filter $(subst *, %, $2), $d) $(call rwildcard, $d/, $2))

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/base_tools

name := memloader

dir_source := src
dir_build := build
dir_out := out

ARCH := -march=armv4t -mtune=arm7tdmi -mthumb -mthumb-interwork

ASFLAGS := -g $(ARCH)

# For debug builds, replace -O2 by -Og and comment -fomit-frame-pointer out
CFLAGS = \
	$(ARCH) \
	-g \
	-Os \
	-fomit-frame-pointer \
	-ffunction-sections \
	-fdata-sections \
	-fno-strict-aliasing \
	-fstrict-volatile-bitfields \
	-std=gnu11 \
	-I$(dir_source) \
	-DNDEBUG \
	-DDEBUG_UART_PORT=UART_A \
	-Wall 

LDFLAGS = -specs=linker.specs -g $(ARCH)

objects =	$(patsubst $(dir_source)/%.s, $(dir_build)/%.o, \
			$(patsubst $(dir_source)/%.c, $(dir_build)/%.o, \
			$(call rwildcard, $(dir_source), *.s *.c)))

mtc_sdram_bins = $(sort $(call rwildcard, $(dir_source)/minerva_tc/mtc_tables/nintendo_switch/, sdram*.bin))
mtc_sdram_lzma = $(dir_build)/mtc_sdram.lzma

objects := $(objects) $(mtc_sdram_lzma).o

define bin2o
	bin2s $< | $(AS) -o $(@)
endef

.PHONY: all
all: $(dir_out)/$(name).bin

.PHONY: clean
clean:
	@rm -rf $(dir_build)
	@rm -rf $(dir_out)

$(dir_out)/$(name).bin: $(dir_build)/$(name).elf
	@mkdir -p "$(@D)"
	$(OBJCOPY) -S -O binary $< $@
	@echo -n "Payload size of" $@ "is "
	@wc -c < $@
	@echo "Max size is 126296 Bytes."

$(dir_build)/$(name).elf: $(objects)
	$(LINK.o) $(OUTPUT_OPTION) $^

$(dir_build)/%.o: $(dir_source)/%.c
	@mkdir -p "$(@D)"
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(dir_build)/%.o: $(dir_source)/%.s
	@mkdir -p "$(@D)"
	$(COMPILE.c) -x assembler-with-cpp $(OUTPUT_OPTION) $<

$(mtc_sdram_lzma): $(mtc_sdram_bins)
	@mkdir -p "$(@D)"
	cat $^ > "$(@:.lzma=)"
	xz -z -e -k --single-stream --format=lzma --threads=1 "$(@:.lzma=)"
	@zip -0 "$(@:.lzma=.zip)" "$(@:.lzma=)" > /dev/null
	@dd conv=notrunc bs=1 skip=22 seek=5 count=4 "if=$(@:.lzma=.zip)" "of=$(@)" 2>/dev/null
	@dd conv=notrunc bs=1 seek=9 count=4 if=/dev/zero "of=$(@)" 2>/dev/null
	@rm "$(@:.lzma=.zip)" "$(@:.lzma=)"

$(dir_build)/%.lzma.o: $(dir_build)/%.lzma
	@$(bin2o)