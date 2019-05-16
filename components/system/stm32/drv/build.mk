
libstm32f4.a_SOURCES := $(wildcard components/system/stm32/drv/src/*.c)
libstm32f4.a_SOURCES += $(wildcard components/system/stm32/drv/src/hwf4/*.c)

libstm32f4.a_DEPS := $(patsubst components/system/stm32/drv/src/%.c,\
	components/system/stm32/drv/build/%.d, $(libstm32f4.a_SOURCES))

libstm32f4.a_OBJECTS := $(patsubst components/system/stm32/drv/src/%.c,\
	components/system/stm32/drv/build/%.o, $(libstm32f4.a_SOURCES))

$(eval $(call toolchain,components/system/stm32/drv/build/libstm32f4.a))

-include $(libstm32f4.a_DEPS)

components/system/stm32/drv/build/%.o: components/system/stm32/drv/src/%.c
	$(call cmd,cc_o_c)

components/system/stm32/drv/build/libstm32f4.a: OBJECTS := $(libstm32f4.a_OBJECTS)
components/system/stm32/drv/build/libstm32f4.a: CFLAGS  += -Icomponents/system/stm32/include \
							   -Icomponents/system/stm32/drv/include \
							   -Icomponents/hal/include/stm32 \
							   -Icomponents/hal/include/generic \
							   -Iinclude \
							   $(FREERTOS_INCLUDES)
components/system/stm32/drv/build/libstm32f4.a: components/system/stm32/build/libSystem.a
components/system/stm32/drv/build/libstm32f4.a: $(libstm32f4.a_OBJECTS)
components/system/stm32/drv/build/libstm32f4.a: | components/system/stm32/drv/build/hwf4
	$(call cmd,ar)

components/system/stm32/drv/build:
	$(call cmd,mkdir)
components/system/stm32/drv/build/hwf4: | components/system/stm32/drv/build
	$(call cmd,mkdir)

clean::
	$(call cmd,rm,components/system/stm32/drv/build)
