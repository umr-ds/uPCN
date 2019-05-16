ifdef PLATFORM

hal_SOURCES := $(wildcard components/hal/src/posix/*.c)

hal_OBJECTS := $(patsubst components/hal/src/posix/%.c,\
	components/hal/src/posix/build/$(PLATFORM)/%.o, $(hal_SOURCES))

hal_DEPS := $(patsubst components/hal/src/posix/%.c,\
	components/hal/src/posix/build/$(PLATFORM)/%.d, $(hal_SOURCES))

$(eval $(call toolchain,components/hal/src/posix/build/$(PLATFORM)/hal.a))

-include $(hal_DEPS)

components/hal/src/posix/build/$(PLATFORM)/%.o: components/hal/src/posix/%.c
	$(call cmd,cc_o_c)

$(hal_OBJECTS): components/hal/src/posix/build/$(PLATFORM)/%.o: components/hal/src/posix/%.c
	$(call cmd,cc_o_c)

components/hal/src/posix/build/$(PLATFORM)/hal.a: OBJECTS := $(hal_OBJECTS)
components/hal/src/posix/build/$(PLATFORM)/hal.a: CFLAGS  += -Icomponents/hal/include/generic \
						 -Icomponents/hal/include/posix \
						 -Iinclude \
						 -Icomponents/cla/include \
						 -Icomponents/cla/include/$(ARCHITECTURE)/$(CLA) \
						 -Icomponents/system/posix/drv/include
components/hal/src/posix/build/$(PLATFORM)/hal.a: $(hal_OBJECTS)
components/hal/src/posix/build/$(PLATFORM)/hal.a: | components/hal/src/posix/build/$(PLATFORM)
	$(call cmd,ar)

components/hal/src/posix/build/:
	$(call cmd,mkdir)

components/hal/src/posix/build/$(PLATFORM): components/hal/src/posix/build/
	$(call cmd,mkdir)

endif #PLATFORM

clean::
	$(call cmd,rm,components/hal/src/posix/build/)
