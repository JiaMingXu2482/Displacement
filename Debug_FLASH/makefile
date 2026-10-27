CC := arm-none-eabi-gcc

CPU_FLAGS := -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
CPP_FLAGS := -std=c99 -DCPU_S32K144LFT0MLLT -DDEV_ERROR_DETECT -DCPU_INIT_CONFIG $(CPU_FLAGS)
INCLUDES := \
	-I../Sources \
	-I../Generated_Code \
	-I../SDK/platform/devices \
	-I../SDK/platform/devices/common \
	-I../SDK/platform/devices/S32K144/include \
	-I../SDK/platform/devices/S32K144/startup \
	-I../SDK/platform/drivers/inc \
	-I../SDK/rtos/osif \
	-I../Project_Settings/Startup_Code

C_SOURCES := \
	$(wildcard ../Generated_Code/*.c) \
	$(wildcard ../SDK/platform/devices/*.c) \
	$(wildcard ../SDK/platform/devices/S32K144/startup/*.c) \
	$(wildcard ../SDK/platform/drivers/src/*/*.c) \
	$(wildcard ../SDK/platform/drivers/src/*/*/*.c) \
	$(wildcard ../SDK/rtos/osif/*.c) \
	$(wildcard ../Sources/*.c)

ASM_SOURCES := $(wildcard ../Project_Settings/Startup_Code/*.S)
C_OBJECTS := $(patsubst ../%.c,%.o,$(C_SOURCES))
ASM_OBJECTS := $(patsubst ../%.S,%.o,$(ASM_SOURCES))
OBJECTS := $(C_OBJECTS) $(ASM_OBJECTS)

LDFLAGS := $(CPU_FLAGS) -T../Project_Settings/Linker_Files/S32K144_64_flash.ld \
	-Wl,-Map,new_project.map -Wl,--gc-sections -specs=nano.specs -specs=nosys.specs \
	--sysroot="D:/Program Files/S32DS/build_tools/gcc_v6.3/gcc-6.3-arm32-eabi/arm-none-eabi/newlib"

.PHONY: all clean

all: new_project.elf
	$(CC) $(CPU_FLAGS) -Wl,--print-memory-usage -o /dev/null $(OBJECTS) $(LDFLAGS) 2>/dev/null || true
	arm-none-eabi-size --format=berkeley new_project.elf

new_project.elf: $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS)

$(C_OBJECTS): %.o: ../%.c
	@mkdir -p "$(@D)"
	$(CC) $(CPP_FLAGS) $(INCLUDES) -MMD -MP -MF "$(@:.o=.d)" -c "$<" -o "$@"

$(ASM_OBJECTS): %.o: ../%.S
	@mkdir -p "$(@D)"
	$(CC) $(CPU_FLAGS) $(INCLUDES) -c "$<" -o "$@"

-include $(OBJECTS:.o=.d)

clean:
	rm -rf Generated_Code Project_Settings SDK Sources *.elf *.map *.siz
