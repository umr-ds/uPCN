
FREERTOS_INCLUDES := -I$(FREERTOS_PREFIX)/FreeRTOS/Source/include \
                     -I$(FREERTOS_PREFIX)/FreeRTOS/Source/portable/GCC/ARM_CM4F

FREERTOS_SOURCES := $(addprefix $(FREERTOS_PREFIX)/FreeRTOS/Source/,\
	list.c                       \
	queue.c                      \
	tasks.c                      \
	portable/GCC/ARM_CM4F/port.c \
	portable/MemMang/heap_3.c    \
)

FREERTOS_DEPS := $(patsubst $(FREERTOS_PREFIX)/FreeRTOS/Source/%.c,\
	components/system/stm32/build/%.d,$(FREERTOS_SOURCES))

FREERTOS_OBJECTS := $(patsubst $(FREERTOS_PREFIX)/FreeRTOS/Source/%.c,\
	components/system/stm32/build/%.o,$(FREERTOS_SOURCES))

$(FREERTOS_OBJECTS): components/system/stm32/build/%.o: \
	$(FREERTOS_PREFIX)/FreeRTOS/Source/%.c
	$(call cmd,cc_o_c)

SYSTEM_SOURCES := $(wildcard components/system/stm32/src/*.c)

SYSTEM_DEPS := $(patsubst components/system/stm32/src/%.c,\
	components/system/stm32/build/%.d,$(SYSTEM_SOURCES))

SYSTEM_OBJECTS := $(patsubst components/system/stm32/src/%.c,\
	components/system/stm32/build/%.o,$(SYSTEM_SOURCES))

-include $(FREERTOS_DEPS)
-include $(SYSTEM_DEPS)

$(SYSTEM_OBJECTS): components/system/stm32/build/%.o: components/system/stm32/src/%.c
	$(call cmd,cc_o_c)

components/system/stm32/build/startup_stm32f40xx.o: \
	components/system/stm32/src/startup_stm32f40xx.s
	$(call cmd,cc_o_s)

libSystem.a_OBJECTS := $(SYSTEM_OBJECTS) $(FREERTOS_OBJECTS) \
	components/system/stm32/build/startup_stm32f40xx.o

components/system/stm32/build/libSystem.a: OBJECTS := $(libSystem.a_OBJECTS)
components/system/stm32/build/libSystem.a: CFLAGS  += -I$(FREERTOS_INCLUDES)
components/system/stm32/build/libSystem.a: $(libSystem.a_OBJECTS)
components/system/stm32/build/libSystem.a: \
	| components/system/stm32/build/portable/GCC/ARM_CM4F \
	  components/system/stm32/build/portable/MemMang
	$(call cmd,ar)

components/system/stm32/build/portable/GCC/ARM_CM4F: \
	| components/system/stm32/build/portable/GCC
	$(call cmd,mkdir)

components/system/stm32/build/portable/GCC components/system/stm32/build/portable/MemMang: \
	| components/system/stm32/build/portable
	$(call cmd,mkdir)

components/system/stm32/build/portable: | components/system/stm32/build
	$(call cmd,mkdir)

components/system/stm32/build:
	$(call cmd,mkdir)

clean::
	$(call cmd,rm,components/system/stm32/build)
