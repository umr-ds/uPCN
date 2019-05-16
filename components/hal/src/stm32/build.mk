
hal_SOURCES := $(wildcard components/hal/src/stm32/*.c)

hal_DEPS := $(patsubst components/hal/src/stm32/%.c,\
	components/hal/src/stm32/build/%.d, $(hal_SOURCES))

hal_OBJECTS := $(patsubst components/hal/src/stm32/%.c,\
	components/hal/src/stm32/build/%.o, $(hal_SOURCES))

$(eval $(call toolchain,components/hal/src/stm32/build/hal.a))

-include $(hal_DEPS)

components/hal/src/stm32/build/%.o: components/hal/src/stm32/%.c
	$(call cmd,cc_o_c)

$(hal_OBJECTS): components/hal/src/stm32/build/%.o: components/hal/src/stm32/%.c
	$(call cmd,cc_o_c)

components/hal/src/stm32/build/hal.a: OBJECTS := $(hal_OBJECTS)
components/hal/src/stm32/build/hal.a: CFLAGS  += -Icomponents/hal/include/generic \
						 -Icomponents/hal/include/stm32 \
						 -Icomponents/system/stm32/drv/include \
						 -Icomponents/system/stm32/include \
						 -Iinclude \
						 -Icomponents/cla/include \
						 -Icomponents/cla/include/$(ARCHITECTURE)/$(CLA) \
						 $(FREERTOS_INCLUDES)
components/hal/src/stm32/build/hal.a: $(hal_OBJECTS)
components/hal/src/stm32/build/hal.a: | components/hal/src/stm32/build/
	$(call cmd,ar)

components/hal/src/stm32/build/:
	$(call cmd,mkdir)

clean::
	$(call cmd,rm,components/hal/src/stm32/build/)
