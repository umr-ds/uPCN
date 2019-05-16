# --------
# RFC 5050
# --------

bundle6_SOURCES := $(wildcard components/bundle6/src/*.c)

bundle6_DEPS := $(patsubst components/bundle6/src/%.c,\
	components/test/build/$(PLATFORM)/bundle6/%.d, $(bundle6_SOURCES))

bundle6_OBJECTS := $(patsubst components/bundle6/src/%.c,\
	components/test/build/$(PLATFORM)/bundle6/%.o, $(bundle6_SOURCES))

-include $(bundle6_DEPS)

components/test/build/$(PLATFORM)/bundle6/%.o: components/bundle6/src/%.c
	$(call cmd,cc_o_c)


# --------
# BPv7-bis
# --------

bundle7_SOURCES := $(wildcard components/bundle7/src/*.c)

bundle7_DEPS := $(patsubst components/bundle7/src/%.c,\
	components/test/build/$(PLATFORM)/bundle7/%.d, $(bundle7_SOURCES))

bundle7_OBJECTS := $(patsubst components/bundle7/src/%.c,\
	components/test/build/$(PLATFORM)/bundle7/%.o, $(bundle7_SOURCES))

-include $(bundle7_DEPS)

components/test/build/$(PLATFORM)/bundle7/%.o: components/bundle7/src/%.c
	$(call cmd,cc_o_c)


# ------------------
# spp implementation
# ------------------

spp_SOURCES := $(wildcard components/spp/src/*.c)

spp_DEPS := $(patsubst components/spp/src/%.c,\
	components/test/build/$(PLATFORM)/spp/%.d, $(spp_SOURCES))

spp_OBJECTS := $(patsubst components/spp/src/%.c,\
	components/test/build/$(PLATFORM)/spp/%.o, $(spp_SOURCES))

-include $(spp_DEPS)

components/test/build/$(PLATFORM)/spp/%.o: components/spp/src/%.c components/test/build/$(PLATFORM)/spp
	$(call cmd,cc_o_c)


# ---------
# upcn core
# ---------

upcn_SOURCES := $(filter-out components/upcn/src/main.c, \
	$(wildcard components/upcn/src/*.c))

upcn_DEPS := $(patsubst components/upcn/src/%.c,\
	components/test/build/$(PLATFORM)/upcn/%.d, $(upcn_SOURCES))

upcn_OBJECTS := $(patsubst components/upcn/src/%.c,\
	components/test/build/$(PLATFORM)/upcn/%.o, $(upcn_SOURCES))

upcn_DEPS    += $(bundle6_DEPS) $(bundle7_DEPS) $(spp_DEPS)
upcn_OBJECTS += $(bundle6_OBJECTS) $(bundle7_OBJECTS) $(spp_OBJECTS)


# ----------
# Unit tests
# ----------

testupcn_SOURCES := $(wildcard components/test/src/*.c)

testupcn_DEPS := $(patsubst components/test/src/%.c,\
	components/test/build/$(PLATFORM)/%.d, $(testupcn_SOURCES))

testupcn_OBJECTS := $(patsubst components/test/src/%.c,\
	components/test/build/$(PLATFORM)/%.o, $(testupcn_SOURCES))

testupcn_LIBS_POSIX := \
	components/hal/src/posix/build/$(PLATFORM)/hal.a \
	components/drv/build/$(PLATFORM)/drv.a \
	components/test/unity/build/$(PLATFORM)/libunity.a \
	components/system/posix/drv/build/$(PLATFORM)/posix-drv.a \
	components/cla/src/build/$(PLATFORM)/$(CLA)/cla.a \
	components/agents/build/$(PLATFORM)/agents.a \
	external/tinycbor/build/$(PLATFORM)/tinycbor.a

testupcn_LIBS_STM32 := \
	components/system/stm32/drv/build/libstm32f4.a \
	components/system/stm32/build/libSystem.a \
	components/test/unity/build/$(PLATFORM)/libunity.a \
	components/hal/src/stm32/build/hal.a \
	components/drv/build/stm32/drv.a \
	components/cla/src/build/$(PLATFORM)/$(CLA)/cla.a \
	components/agents/build/$(PLATFORM)/agents.a \
	external/tinycbor/build/$(PLATFORM)/tinycbor.a


$(eval $(call toolchain,components/test/build/testupcn))

-include $(upcn_DEPS)
-include $(testupcn_DEPS)

components/test/build/$(PLATFORM)/upcn/%.o: components/upcn/src/%.c
	$(call cmd,cc_o_c)

components/test/build/$(PLATFORM)/%.o: components/test/src/%.c
	$(call cmd,cc_o_c)

components/test/build/stm32/testupcn: CXXFLAGS += $(CXXFLAGS_STM32)
components/test/build/stm32/testupcn: LDSCRIPT += $(LDSCRIPT_STM32)
components/test/build/stm32/testupcn: LDFLAGS += $(LDFLAGS_STM32)
components/test/build/stm32/testupcn: CFLAGS += $(CFLAGS_STM32) \
						-DARCHITECTURE_STM32
components/test/build/stm32/testupcn: LIBS    := $(testupcn_LIBS_STM32)
components/test/build/stm32/testupcn: $(testupcn_LIBS_STM32)

components/test/build/posix_local/testupcn: CXXFLAGS += $(CXXFLAGS_POSIX)
components/test/build/posix_local/testupcn: LDSCRIPT += $(LDSCRIPT_POSIX)
components/test/build/posix_local/testupcn: LDFLAGS += $(LDFLAGS_POSIX)
components/test/build/posix_local/testupcn: CFLAGS += $(CFLAGS_POSIX) \
						-DUPCN_POSIX_TEST_BUILD
components/test/build/posix_local/testupcn: LIBS    := $(testupcn_LIBS_POSIX)
components/test/build/posix_local/testupcn: $(testupcn_LIBS_POSIX)
components/test/build/posix_local/testupcn: LDFLAGS  += -lm -lpthread



components/test/build/$(PLATFORM)/testupcn: CFLAGS += -DUPCN_TEST_BUILD \
	-Icomponents/test/include                       \
	-Icomponents/test/unity/include                 \
	-Iinclude		                        \
	-Iexternal/tinycbor/src                         \
	-Icomponents/system/$(ARCHITECTURE)/drv/include \
	-Icomponents/cla/include                        \
	-Icomponents/cla/include/$(ARCHITECTURE)/$(CLA) \
	-Icomponents/system/$(ARCHITECTURE)/include     \
	-Icomponents/hal/include/generic                \
	-Icomponents/hal/include/$(ARCHITECTURE)        \
	$(FREERTOS_INCLUDES)
components/test/build/$(PLATFORM)/testupcn: OBJECTS := $(testupcn_OBJECTS) $(upcn_OBJECTS)
components/test/build/$(PLATFORM)/testupcn: LDFLAGS  += -lm
components/test/build/$(PLATFORM)/testupcn: $(testupcn_OBJECTS) $(upcn_OBJECTS)
components/test/build/$(PLATFORM)/testupcn: | \
	components/test/build/$(PLATFORM)/upcn \
	components/test/build/$(PLATFORM)/bundle6 \
	components/test/build/$(PLATFORM)/bundle7 \
	components/test/build/$(PLATFORM)/spp
	$(call cmd,link)

components/test/build/$(PLATFORM)/testupcn.bin: components/test/build/$(PLATFORM)/testupcn
	$(call cmd,bin)

components/test/build:
	$(call cmd,mkdir)


ifdef PLATFORM

components/test/build/$(PLATFORM): components/test/build
	$(call cmd,mkdir)

endif #PLATFORM

components/test/build/$(PLATFORM)/bundle6: | components/test/build/$(PLATFORM)
	$(call cmd,mkdir)

components/test/build/$(PLATFORM)/bundle7: | components/test/build/$(PLATFORM)
	$(call cmd,mkdir)

components/test/build/$(PLATFORM)/spp: | components/test/build/$(PLATFORM)
	$(call cmd,mkdir)

components/test/build/$(PLATFORM)/upcn: | components/test/build/$(PLATFORM)
	$(call cmd,mkdir)

clean::
	$(call cmd,rm,components/test/build)

tclean::
	$(call cmd,rm,components/test/build)

include components/test/unity/build.mk
