ifdef PLATFORM

posix-drv.a_SOURCES := $(wildcard components/system/posix/drv/src/*.c)

posix-drv.a_DEPS := $(patsubst components/system/posix/drv/src/%.c,\
	components/system/posix/drv/build/$(PLATFORM)/%.d, $(posix-drv.a_SOURCES))

posix-drv.a_OBJECTS := $(patsubst components/system/posix/drv/src/%.c,\
	components/system/posix/drv/build/$(PLATFORM)/%.o, $(posix-drv.a_SOURCES))

$(eval $(call toolchain,components/system/posix/drv/build/$(PLATFORM)/posix-drv.a))

-include $(posix-drv.a_DEPS)

components/system/posix/drv/build/$(PLATFORM)/%.o: components/system/posix/drv/src/%.c
	$(call cmd,cc_o_c)

components/system/posix/drv/build/$(PLATFORM)/posix-drv.a: OBJECTS := $(posix-drv.a_OBJECTS)
components/system/posix/drv/build/$(PLATFORM)/posix-drv.a: CFLAGS  += -Icomponents/system/posix/drv/include
components/system/posix/drv/build/$(PLATFORM)/posix-drv.a: $(posix-drv.a_OBJECTS)
components/system/posix/drv/build/$(PLATFORM)/posix-drv.a: | components/system/posix/drv/build/$(PLATFORM)/
	$(call cmd,ar)

components/system/posix/drv/build/$(PLATFORM)/: components/system/posix/drv/build/
	$(call cmd,mkdir)

components/system/posix/drv/build/:
	$(call cmd,mkdir)

endif #PLATFORM

clean::
	$(call cmd,rm,components/system/posix/drv/build)
