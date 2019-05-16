ifndef CLA
  ifeq "$(ARCHITECTURE)" "posix"
    CLA = TCP_LEGACY
  endif
  ifeq "$(ARCHITECTURE)" "stm32"
    CLA = USB_OTG
  endif
endif

upcn_LIBS_POSIX := \
	components/upcn/build/$(PLATFORM)/upcn.a \
	components/agents/build/$(PLATFORM)/agents.a \
	components/hal/src/posix/build/$(PLATFORM)/hal.a \
	components/cla/src/build/$(PLATFORM)/$(CLA)/cla.a \
	components/drv/build/$(PLATFORM)/drv.a \
	components/system/posix/drv/build/$(PLATFORM)/posix-drv.a \
	external/tinycbor/build/$(PLATFORM)/tinycbor.a

upcn_LIBS_STM32 := \
	components/upcn/build/$(PLATFORM)/upcn.a \
	components/agents/build/$(PLATFORM)/agents.a \
	components/hal/src/stm32/build/hal.a \
	components/cla/src/build/$(PLATFORM)/$(CLA)/cla.a \
	components/drv/build/$(PLATFORM)/drv.a \
	components/system/stm32/drv/build/libstm32f4.a \
	components/system/stm32/build/libSystem.a \
	external/tinycbor/build/$(PLATFORM)/tinycbor.a

$(eval $(call toolchain,build/$(PLATFORM)/upcn))

build/posix_local/upcn: CFLAGS += $(CFLAGS_POSIX) -DPLATFORM_POSIX
build/posix_local/upcn: CXXFLAGS += $(CXXFLAGS_POSIX)
build/posix_local/upcn: LIBS    := $(upcn_LIBS_POSIX)
build/posix_local/upcn: LDFLAGS  += -lm -lpthread
build/posix_local/upcn: $(upcn_LIBS_POSIX)
build/posix_local/upcn: | build/posix_local
	$(call cmd,link)

build/posix_dev/upcn: CFLAGS += $(CFLAGS_POSIX)
build/posix_dev/upcn: CXXFLAGS += $(CXXFLAGS_POSIX)
build/posix_dev/upcn: LIBS    := $(upcn_LIBS_POSIX)
build/posix_dev/upcn: LDFLAGS  += -lm -lpthread
build/posix_dev/upcn: $(upcn_LIBS_POSIX)
build/posix_dev/upcn: | build/posix_dev
	$(call cmd,link)

build/stm32/upcn: CFLAGS += $(CFLAGS_STM32) -DPLATFORM_STM32
build/stm32/upcn: CXXFLAGS += $(CXXFLAGS_STM32)
build/stm32/upcn: LDSCRIPT += $(LDSCRIPT_STM32)
build/stm32/upcn: LDFLAGS += $(LDFLAGS_STM32)
build/stm32/upcn: LIBS    := $(upcn_LIBS_STM32)
build/stm32/upcn: LDFLAGS  += -lm
build/stm32/upcn: $(upcn_LIBS_STM32)
build/stm32/upcn: | build/stm32
	$(call cmd,link)


build/stm32/upcn.bin: build/stm32/upcn
	$(call cmd,bin)


# Directories

build:
	$(call cmd,mkdir)

ifdef PLATFORM

build/$(PLATFORM): build
	$(call cmd,mkdir)

endif #PLATFORM

# Cleaning

clean::
	$(call cmd,rm,build)

uclean::
	$(call cmd,rm,build)
