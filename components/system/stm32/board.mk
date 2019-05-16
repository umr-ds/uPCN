
# COMPILER CONFIG

BASE_CFLAGS_STM32 := -g -Wall -mlittle-endian -mcpu=cortex-m4 -mthumb \
               -mfloat-abi=hard -mfpu=fpv4-sp-d16 \
               -ffunction-sections -fdata-sections

CFLAGS_STM32   += $(BASE_CFLAGS_STM32) -std=gnu99
CXXFLAGS_STM32 += $(BASE_CFLAGS_STM32) -fno-exceptions -fno-rtti

LDSCRIPT_STM32 += components/system/stm32/support/stm32_flash.ld
LDFLAGS_STM32  += -mlittle-endian -mcpu=cortex-m4 -mthumb -mfloat-abi=hard \
	    -mfpu=fpv4-sp-d16 -Wl,--script=$(LDSCRIPT_STM32) \
            -Wl,--gc-sections

# BOARD CONFIG

BOARD_FREQ_STM32 := 8000000

CFLAGS_STM32 += -DHSE_VALUE=$(BOARD_FREQ_STM32)
