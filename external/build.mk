ifdef PLATFORM

tinycbor_SOURCES := \
	cborerrorstrings.c \
	cborencoder.c \
	cborencoder_close_container_checked.c \
	cborparser.c \
	cborparser_dup_string.c \
	cborpretty.c \
	cbortojson.c

tinycbor_OBJECTS := $(patsubst %.c,external/tinycbor/build/$(PLATFORM)/%.o, \
	$(tinycbor_SOURCES))

$(eval $(call toolchain,external/tinycbor/build/$(PLATFORM)/tinycbor.a))

external/tinycbor/build/$(PLATFORM)/%.o: external/tinycbor/src/%.c
	$(call cmd,cc_o_c)


external/tinycbor/build/$(PLATFORM)/tinycbor.a: OBJECTS := $(tinycbor_OBJECTS)
external/tinycbor/build/$(PLATFORM)/tinycbor.a: CFLAGS += -Iexternal/tinycbor/src/
external/tinycbor/build/$(PLATFORM)/tinycbor.a: $(tinycbor_OBJECTS)
external/tinycbor/build/$(PLATFORM)/tinycbor.a: | external/tinycbor/build/$(PLATFORM)
	$(call cmd,ar)

external/tinycbor/build/$(PLATFORM): external/tinycbor/build
	$(call cmd,mkdir)

external/tinycbor/build:
	$(call cmd,mkdir)

endif #PLATFORM

clean::
	$(call cmd,rm,external/tinycbor/build)

uclean::
	$(call cmd,rm,external/tinycbor/build)
