ifdef PLATFORM

agents_SOURCES := $(wildcard components/agents/src/*.c)

agents_DEPS := $(patsubst components/agents/src/%.c,\
	components/agents/build/$(PLATFORM)/%.d, $(agents_SOURCES))

agents_OBJECTS := $(patsubst components/agents/src/%.c,\
	components/agents/build/$(PLATFORM)/%.o, $(agents_SOURCES))

$(eval $(call toolchain,components/agents/build/$(PLATFORM)/agents.a))

-include $(agents_DEPS)

components/agents/build/$(PLATFORM)/%.o: components/agents/src/%.c
	$(call cmd,cc_o_c)

components/agents/build/$(PLATFORM)/agents.a: OBJECTS := $(agents_OBJECTS)
components/agents/build/$(PLATFORM)/agents.a: CFLAGS  += -Icomponents/agents/include \
		-Iinclude \
		-Icomponents/hal \
		-Icomponents/hal/include/generic \
		-Icomponents/hal/include/$(ARCHITECTURE) \
		-Icomponents/system/$(ARCHITECTURE)/drv/include \
		-Icomponents/system/$(ARCHITECTURE)/include \
		-Icomponents/cla/include \
		-Icomponents/cla/include/$(ARCHITECTURE)/$(CLA) \
		$(FREERTOS_INCLUDES)
components/agents/build/$(PLATFORM)/agents.a: $(agents_OBJECTS)
components/agents/build/$(PLATFORM)/agents.a: | components/agents/build/$(PLATFORM)
	$(call cmd,ar)

components/agents/build/:
	$(call cmd,mkdir)

components/agents/build/$(PLATFORM): components/agents/build/
	$(call cmd,mkdir)

endif #PLATFORM

clean::
	$(call cmd,rm,components/agents/build)
