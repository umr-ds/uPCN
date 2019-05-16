ifdef PLATFORM

drv_SOURCES := $(wildcard components/drv/src/*.c)

drv_DEPS := $(patsubst components/drv/src/%.c,\
	components/drv/build/$(PLATFORM)/%.d, $(drv_SOURCES))

drv_OBJECTS := $(patsubst components/drv/src/%.c,\
	components/drv/build/$(PLATFORM)/%.o, $(drv_SOURCES))

$(eval $(call toolchain,components/drv/build/$(PLATFORM)/drv.a))

-include $(drv_DEPS)

components/drv/build/$(PLATFORM)/%.o: components/drv/src/%.c
	$(call cmd,cc_o_c)

components/drv/build/$(PLATFORM)/drv.a: OBJECTS := $(drv_OBJECTS)
components/drv/build/$(PLATFORM)/drv.a: CFLAGS  +=  -Icomponents/system/$(ARCHITECTURE)/drv/include \
                                                    -Icomponents/hal/include/generic \
                                                    -Icomponents/hal/include/$(ARCHITECTURE) \
                                                    -Iinclude

components/drv/build/$(PLATFORM)/drv.a: $(drv_OBJECTS)
components/drv/build/$(PLATFORM)/drv.a: | components/drv/build/$(PLATFORM)
	$(call cmd,ar)

components/drv/build/:
	$(call cmd,mkdir)

components/drv/build/$(PLATFORM): components/drv/build/
	$(call cmd,mkdir)

endif #PLATFORM

clean::
	$(call cmd,rm,components/drv/build)
