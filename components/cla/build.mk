ifdef PLATFORM

ifndef CLA
  $(error CLA not defined)
endif

ifeq ($(wildcard components/cla/src/$(ARCHITECTURE)/$(CLA)/.*),)
  $(error Specified CLA is not existing)
endif

cla_SOURCES := $(wildcard components/cla/src/$(ARCHITECTURE)/$(CLA)/*.c)

cla_OBJECTS := $(patsubst components/cla/src/$(ARCHITECTURE)/$(CLA)/%.c,\
	components/cla/src/build/$(PLATFORM)/$(CLA)/%.o, $(cla_SOURCES))

cla_DEPS := $(patsubst components/cla/src/$(ARCHITECTURE)/$(CLA)/%.c,\
	components/cla/src/build/$(PLATFORM)/$(CLA)/%.d, $(cla_SOURCES))

$(eval $(call toolchain,components/cla/src/build/$(PLATFORM)/$(CLA)/cla.a))

-include $(cla_DEPS)

components/cla/src/build/$(PLATFORM)/$(CLA)/%.o: components/cla/src/$(ARCHITECTURE)/$(CLA)/%.c
	$(call cmd,cc_o_c)

$(cla_OBJECTS): components/cla/src/build/$(PLATFORM)/$(CLA)/%.o: components/cla/src/$(ARCHITECTURE)/$(CLA)/%.c
	$(call cmd,cc_o_c)

components/cla/src/build/$(PLATFORM)/$(CLA)/cla.a: OBJECTS := $(cla_OBJECTS)
components/cla/src/build/$(PLATFORM)/$(CLA)/cla.a: CFLAGS  += -Icomponents/cla/include \
						 -Icomponents/cla/include/$(ARCHITECTURE)/$(CLA) \
						 -Icomponents/hal/include/generic \
						 -Icomponents/hal/include/$(ARCHITECTURE) \
						 -Iinclude \
						 -Iexternal/tinycbor/src \
						 -Icomponents/system/$(ARCHITECTURE)/drv/include \
					 	 -Icomponents/system/$(ARCHITECTURE)/include \
						 -Icomponents/system/$(PLATFORM)/drv/include \
						 $(FREERTOS_INCLUDES)
components/cla/src/build/$(PLATFORM)/$(CLA)/cla.a: $(cla_OBJECTS)
components/cla/src/build/$(PLATFORM)/$(CLA)/cla.a: | components/cla/src/build/$(PLATFORM)/$(CLA)
	$(call cmd,ar)

components/cla/src/build/$(CLA)/:
	$(call cmd,mkdir)

components/cla/src/build/$(PLATFORM)/$(CLA): components/cla/src/build/$(CLA)/
	$(call cmd,mkdir)

endif #PLATFORM

clean::
	$(call cmd,rm,components/cla/src/build/$(CLA))
