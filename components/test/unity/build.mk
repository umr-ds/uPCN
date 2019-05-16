
unity_SOURCES := $(wildcard components/test/unity/src/*.c)

unity_OBJECTS := $(patsubst components/test/unity/src/%.c,components/test/unity/build/$(PLATFORM)/%.o, \
		 $(unity_SOURCES))

unity_DEPS := $(patsubst components/test/unity/src/%.c,components/test/unity/build/$(PLATFORM)/%.d, \
		 $(unity_SOURCES))

$(eval $(call toolchain,components/test/unity/build/$(PLATFORM)/libunity.a))

-include $(unity_DEPS)

components/test/unity/build/$(PLATFORM)/%.o: components/test/unity/src/%.c
	$(call cmd,cc_o_c)

components/test/unity/build/$(PLATFORM)/libunity.a: OBJECTS := $(unity_OBJECTS)
components/test/unity/build/$(PLATFORM)/libunity.a: CFLAGS  += -Icomponents/test/unity/include
components/test/unity/build/$(PLATFORM)/libunity.a: $(unity_OBJECTS)
components/test/unity/build/$(PLATFORM)/libunity.a: | components/test/unity/build/$(PLATFORM)
	$(call cmd,ar)

components/test/unity/build:
	$(call cmd,mkdir)


ifdef PLATFORM

components/test/unity/build/$(PLATFORM): components/test/unity/build
	$(call cmd,mkdir)

endif #PLATFORM


clean::
	$(call cmd,rm,components/test/unity/build)
