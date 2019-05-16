ifdef PLATFORM

# --------
# RFC 5050
# --------

bundle6_SOURCES := $(wildcard components/bundle6/src/*.c)

bundle6_DEPS := $(patsubst components/bundle6/src/%.c,\
	components/upcn/build/$(PLATFORM)/bundle6/%.d, $(bundle6_SOURCES))

bundle6_OBJECTS := $(patsubst components/bundle6/src/%.c,\
	components/upcn/build/$(PLATFORM)/bundle6/%.o, $(bundle6_SOURCES))

-include $(bundle6_DEPS)

components/upcn/build/$(PLATFORM)/bundle6/%.o: components/bundle6/src/%.c
	$(call cmd,cc_o_c)

# --------
# BPv7-bis
# --------

bundle7_SOURCES := $(wildcard components/bundle7/src/*.c)

bundle7_DEPS := $(patsubst components/bundle7/src/%.c,\
	components/upcn/build/$(PLATFORM)/bundle7/%.d, $(bundle7_SOURCES))

bundle7_OBJECTS := $(patsubst components/bundle7/src/%.c,\
	components/upcn/build/$(PLATFORM)/bundle7/%.o, $(bundle7_SOURCES))

-include $(bundle7_DEPS)

components/upcn/build/$(PLATFORM)/bundle7/%.o: components/bundle7/src/%.c
	$(call cmd,cc_o_c)

# ---------------------
# Space Packet Protocol
# ---------------------

spp_SOURCES := $(wildcard components/spp/src/*.c)

spp_DEPS := $(patsubst components/spp/src/%.c,\
	components/upcn/build/$(PLATFORM)/spp/%.d, $(spp_SOURCES))

spp_OBJECTS := $(patsubst components/spp/src/%.c,\
	components/upcn/build/$(PLATFORM)/spp/%.o, $(spp_SOURCES))

-include $(spp_DEPS)

components/upcn/build/$(PLATFORM)/spp/%.o: components/spp/src/%.c
	$(call cmd,cc_o_c)

# ---------
# upcn core
# ---------

upcn_SOURCES := $(wildcard components/upcn/src/*.c)

upcn_DEPS := $(patsubst components/upcn/src/%.c,components/upcn/build/$(PLATFORM)/%.d, \
	$(upcn_SOURCES))

upcn_OBJECTS := $(patsubst components/upcn/src/%.c,components/upcn/build/$(PLATFORM)/%.o, \
	$(upcn_SOURCES))

# Add protocol implementations
upcn_DEPS    += $(bundle6_DEPS) $(bundle7_DEPS) $(spp_DEPS)
upcn_OBJECTS += $(bundle6_OBJECTS) $(bundle7_OBJECTS) $(spp_OBJECTS)

$(eval $(call toolchain,components/upcn/build/$(PLATFORM)/upcn.a))

-include $(upcn_DEPS)

components/upcn/build/$(PLATFORM)/%.o: components/upcn/src/%.c
	$(call cmd,cc_o_c)

components/upcn/build/$(PLATFORM)/upcn.a: OBJECTS := $(upcn_OBJECTS)
components/upcn/build/$(PLATFORM)/upcn.a: CFLAGS  += \
	-Iinclude \
	-Icomponents/hal/include/generic \
	-Icomponents/hal/include/$(ARCHITECTURE) \
	-Iexternal/tinycbor/src \
	-Icomponents/cla/include \
	-Icomponents/cla/include/$(ARCHITECTURE)/$(CLA) \
	-Icomponents/system/$(ARCHITECTURE)/drv/include \
	-Icomponents/system/$(ARCHITECTURE)/include \
	$(FREERTOS_INCLUDES)
components/upcn/build/$(PLATFORM)/upcn.a: $(upcn_OBJECTS)
components/upcn/build/$(PLATFORM)/upcn.a: | \
	components/upcn/build/$(PLATFORM)/bundle7 \
	components/upcn/build/$(PLATFORM)/bundle6 \
        components/upcn/build/$(PLATFORM)/spp
	$(call cmd,ar)

components/upcn/build/$(PLATFORM)/bundle6: components/upcn/build/$(PLATFORM)
	$(call cmd,mkdir)

components/upcn/build/$(PLATFORM)/bundle7: components/upcn/build/$(PLATFORM)
	$(call cmd,mkdir)

components/upcn/build/$(PLATFORM)/spp: components/upcn/build/$(PLATFORM)
	$(call cmd,mkdir)

components/upcn/build/$(PLATFORM): components/upcn/build
	$(call cmd,mkdir)

components/upcn/build:
	$(call cmd,mkdir)




endif #PLATFORM

clean::
	$(call cmd,rm,components/upcn/build)

uclean::
	$(call cmd,rm,components/upcn/build)
