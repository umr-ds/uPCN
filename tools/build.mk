all: tools

tools: tools/build/upcn_connect/upcn_connect
tools: tools/build/upcn_netconnect/upcn_netconnect
tools: tools/build/upcn_posix_performance/upcn_posix_performance
tools: tools/build/upcn_receive/upcn_receive
tools: tools/build/upcn_test/upcn_test
tools: tools/build/upcn_parse/upcn_parse
tools: tools/build/rrnd_proxy/rrnd_proxy
tools: tools/build/upcn_posix_performance/upcn_posix_performance

LFLAGS_LOCAL := -lrt -lzmq -lm
CFLAGS_LOCAL += -std=gnu11 -DTOOLS

# The -MMD flags additionaly creates a .d file with
# the same name as the .o file.
echo-cc-local = echo '  CC_L    $@';
cc-local = @$(call echo-cc-local,$@) gcc -MMD -o "$@" $(CFLAGS_LOCAL) -c "$<"

echo-link-local = echo '  LINK_L  $@';
link-local = @$(call echo-link-local,$@) gcc -o "$@" $(OBJECTS) $(LFLAGS_LOCAL)

echo-ar-local = echo '  AR_L    $@';
ar-local = @$(call echo-ar-local,$@) ar rcs "$@" $(OBJECTS)


# --------
# TinyCBOR
# --------

tinycbor_SOURCES := \
	cborerrorstrings.c \
	cborencoder.c \
	cborencoder_close_container_checked.c \
	cborparser.c \
	cborparser_dup_string.c \
	cborpretty.c \
	cbortojson.c

tinycbor_DEPS := $(patsubst external/tinycbor/src/%.c, \
	tools/build/tinycbor/%.d, $(tinycbor_SOURCES))

tinycbor_OBJECTS := $(patsubst %.c,tools/build/tinycbor/%.o, \
	$(tinycbor_SOURCES))

tools/build/tinycbor/%.o: external/tinycbor/src/%.c
	$(call cc-local)

tools/build/tinycbor/tinycbor.a: OBJECTS := $(tinycbor_OBJECTS)
tools/build/tinycbor/tinycbor.a: CFLAGS_LOCAL += \
	-Iexternal/tinycbor/src
tools/build/tinycbor/tinycbor.a: $(tinycbor_OBJECTS)
tools/build/tinycbor/tinycbor.a: | tools/build/tinycbor
	$(call ar-local)


# ------------
# drv (driver)
# ------------

drv_SOURCES := $(wildcard components/drv/src/*.c)

drv_DEPS := $(patsubst components/drv/src/%.c, \
	tools/build/drv/%.d, $(drv_SOURCES))

drv_OBJECTS := $(patsubst components/drv/src/%.c, \
	tools/build/drv/%.o, $(drv_SOURCES))

tools/build/drv/%.o: components/drv/src/%.c
	$(call cc-local)

tools/build/drv/drv.a: OBJECTS := $(drv_OBJECTS)
tools/build/drv/drv.a: CFLAGS_LOCAL += \
	-Iinclude
tools/build/drv/drv.a: $(drv_OBJECTS)
tools/build/drv/drv.a: | tools/build/drv
	$(call ar-local)


# -------
# libupcn
# -------

libupcn_SOURCES := $(wildcard tools/libupcn/*.c)

libupcn_DEPS := $(patsubst tools/libupcn/%.c, \
	tools/build/libupcn/%.d, $(libupcn_SOURCES))

libupcn_OBJECTS := $(patsubst tools/libupcn/%.c, \
	tools/build/libupcn/%.o, $(libupcn_SOURCES))


# RFC 5050
bundle6_SOURCES := $(wildcard components/bundle6/src/*.c)

bundle6_DEPS := $(patsubst components/bundle6/src/%.c, \
	tools/build/libupcn/bundle6/%.d, $(bundle6_SOURCES))

bundle6_OBJECTS := $(patsubst components/bundle6/src/%.c, \
	tools/build/libupcn/bundle6/%.o, $(bundle6_SOURCES))


# BPv7-bis
bundle7_SOURCES := $(wildcard components/bundle7/src/*.c)

bundle7_DEPS := $(patsubst components/bundle7/src/%.c, \
	tools/build/libupcn/bundle7/%.d, $(bundle7_SOURCES))

bundle7_OBJECTS := $(patsubst components/bundle7/src/%.c, \
	tools/build/libupcn/bundle7/%.o, $(bundle7_SOURCES))


# Add protocol implementations
libupcn_DEPS    += $(bundle6_DEPS) $(bundle7_DEPS)
libupcn_OBJECTS += $(bundle6_OBJECTS) $(bundle7_OBJECTS)

tools/build/libupcn/%.o: tools/libupcn/%.c
	$(call cc-local)

tools/build/libupcn/bundle6/%.o: components/bundle6/src/%.c
	$(call cc-local)

tools/build/libupcn/bundle7/%.o: components/bundle7/src/%.c
	$(call cc-local)

tools/build/libupcn/libupcn.a: OBJECTS := $(libupcn_OBJECTS)
tools/build/libupcn/libupcn.a: CFLAGS_LOCAL += \
	-Iinclude \
	-Icomponents/hal/include/generic \
	-Icomponents/hal/include/generic \
	-Icomponents/hal/include/posix \
	-Iexternal/tinycbor/src
tools/build/libupcn/libupcn.a: $(libupcn_OBJECTS)
tools/build/libupcn/libupcn.a: | \
	tools/build/libupcn \
	tools/build/libupcn/bundle6 \
	tools/build/libupcn/bundle7
	$(call ar-local)


# All static libraries required to execute upcn
libupcn := \
	tools/build/libupcn/libupcn.a \
	tools/build/tinycbor/tinycbor.a \
	tools/build/drv/drv.a


# ------------
# upcn_connect
# ------------

upcn_connect_SOURCES := $(wildcard tools/upcn_connect/*.c)

upcn_connect_DEPS := $(patsubst tools/upcn_connect/%.c, \
	tools/build/upcn_connect/%.d, $(upcn_connect_SOURCES))

upcn_connect_OBJECTS := $(patsubst tools/upcn_connect/%.c, \
	tools/build/upcn_connect/%.o, $(upcn_connect_SOURCES))

tools/build/upcn_connect/%.o: tools/upcn_connect/%.c
	$(call cc-local)

tools/build/upcn_connect/upcn_connect: OBJECTS := $(upcn_connect_OBJECTS)
tools/build/upcn_connect/upcn_connect: CFLAGS_LOCAL += \
	-Itools/include -Itools/upcn_connect
tools/build/upcn_connect/upcn_connect: LFLAGS_LOCAL += -pthread
tools/build/upcn_connect/upcn_connect: $(upcn_connect_OBJECTS)
tools/build/upcn_connect/upcn_connect: | tools/build/upcn_connect
	$(call link-local)


# ---------------
# upcn_netconnect
# ---------------

upcn_netconnect_SOURCES := $(wildcard tools/upcn_netconnect/*.c)

upcn_netconnect_DEPS := $(patsubst tools/upcn_netconnect/%.c, \
	tools/build/upcn_netconnect/%.d, $(upcn_netconnect_SOURCES))

upcn_netconnect_OBJECTS := $(patsubst tools/upcn_netconnect/%.c, \
	tools/build/upcn_netconnect/%.o, $(upcn_netconnect_SOURCES))

tools/build/upcn_netconnect/%.o: tools/upcn_netconnect/%.c
	$(call cc-local)

tools/build/upcn_netconnect/upcn_netconnect: OBJECTS := $(upcn_netconnect_OBJECTS)
tools/build/upcn_netconnect/upcn_netconnect: CFLAGS_LOCAL += \
	-Itools/include \
	-Itools/upcn_netconnect
tools/build/upcn_netconnect/upcn_netconnect: LFLAGS_LOCAL += -pthread
tools/build/upcn_netconnect/upcn_netconnect: $(upcn_netconnect_OBJECTS)
tools/build/upcn_netconnect/upcn_netconnect: | tools/build/upcn_netconnect
	$(call link-local)


# ----------------------
# upcn_posix_performance
# ----------------------

upcn_posix_performance_SOURCES := $(wildcard tools/upcn_posix_performance/*.c)

upcn_posix_performance_DEPS := $(patsubst tools/upcn_posix_performance/%.c, \
	tools/build/upcn_posix_performance/%.d, $(upcn_posix_performance_SOURCES))

upcn_posix_performance_OBJECTS := $(patsubst tools/upcn_posix_performance/%.c, \
	tools/build/upcn_posix_performance/%.o, $(upcn_posix_performance_SOURCES))

tools/build/upcn_posix_performance/%.o: tools/upcn_posix_performance/%.c
	$(call cc-local)

tools/build/upcn_posix_performance/upcn_posix_performance: OBJECTS := $(upcn_posix_performance_OBJECTS)
tools/build/upcn_posix_performance/upcn_posix_performance: CFLAGS_LOCAL += \
	-Itools/include -Itools/upcn_posix_performance
tools/build/upcn_posix_performance/upcn_posix_performance: LFLAGS_LOCAL += -pthread
tools/build/upcn_posix_performance/upcn_posix_performance: $(upcn_posix_performance_OBJECTS)
tools/build/upcn_posix_performance/upcn_posix_performance: | tools/build/upcn_posix_performance
	$(call link-local)


# ------------
# upcn_receive
# ------------

upcn_receive_SOURCES := $(wildcard tools/upcn_receive/*.c)

upcn_receive_DEPS := $(patsubst tools/upcn_receive/%.c, \
	tools/build/upcn_receive/%.d, $(upcn_receive_SOURCES))

upcn_receive_OBJECTS := $(patsubst tools/upcn_receive/%.c, \
	tools/build/upcn_receive/%.o, $(upcn_receive_SOURCES))

tools/build/upcn_receive/%.o: tools/upcn_receive/%.c
	$(call cc-local)

tools/build/upcn_receive/upcn_receive: OBJECTS := $(upcn_receive_OBJECTS)
tools/build/upcn_receive/upcn_receive: CFLAGS_LOCAL += \
	-Itools/include -Itools/upcn_receive
tools/build/upcn_receive/upcn_receive: $(upcn_receive_OBJECTS)
tools/build/upcn_receive/upcn_receive: | tools/build/upcn_receive
	$(call link-local)


# ---------
# upcn_test
# ---------

upcn_test_SOURCES := $(wildcard tools/upcn_test/*.c)

upcn_test_DEPS := $(patsubst tools/upcn_test/%.c, \
	tools/build/upcn_test/%.d, $(upcn_test_SOURCES))

upcn_test_OBJECTS := $(patsubst tools/upcn_test/%.c, \
	tools/build/upcn_test/%.o, $(upcn_test_SOURCES))

tools/build/upcn_test/%.o: tools/upcn_test/%.c
	$(call cc-local)

tools/build/upcn_test/upcn_test: OBJECTS := $(upcn_test_OBJECTS) $(libupcn)
tools/build/upcn_test/upcn_test: CFLAGS_LOCAL += \
	-Iinclude \
	-Itools/upcn_test \
	-Icomponents/system/posix/drv/include \
	-Icomponents/hal/include/generic \
	-Icomponents/hal/include/posix \
	-Icomponents/cla/include \
	-Icomponents/cla/include/$(ARCHITECTURE)/$(CLA) \
	-Iexternal/tinycbor/src
tools/build/upcn_test/upcn_test: $(upcn_test_OBJECTS)
tools/build/upcn_test/upcn_test: $(libupcn)
tools/build/upcn_test/upcn_test: | tools/build/upcn_test
	$(call link-local)


# ----------
# upcn_parse
# ----------

upcn_parse_SOURCES := $(wildcard tools/upcn_parse/*.c)

upcn_parse_DEPS := $(patsubst tools/upcn_parse/%.c, \
	tools/build/upcn_parse/%.d, $(upcn_parse_SOURCES))

upcn_parse_OBJECTS := $(patsubst tools/upcn_parse/%.c, \
	tools/build/upcn_parse/%.o, $(upcn_parse_SOURCES))

tools/build/upcn_parse/%.o: tools/upcn_parse/%.c
	$(call cc-local)

tools/build/upcn_parse/upcn_parse: OBJECTS := $(upcn_parse_OBJECTS)
tools/build/upcn_parse/upcn_parse: CFLAGS_LOCAL += \
	-Iinclude \
	-Icomponents/system/posix/drv/include \
	-Icomponents/hal/include/generic \
	-Icomponents/hal/include/posix \
	-Iexternal/tinycbor/src
tools/build/upcn_parse/upcn_parse: LFLAGS_LOCAL += $(libupcn)
tools/build/upcn_parse/upcn_parse: $(upcn_parse_OBJECTS)
tools/build/upcn_parse/upcn_parse: $(libupcn)
tools/build/upcn_parse/upcn_parse: | tools/build/upcn_parse
	$(call link-local)


# ----------
# rrnd_proxy
# ----------

rrnd_proxy_SOURCES := $(wildcard tools/rrnd_proxy/*.c)

rrnd_proxy_DEPS := $(patsubst tools/rrnd_proxy/%.c, \
	tools/build/rrnd_proxy/%.d, $(rrnd_proxy_SOURCES))

rrnd_proxy_OBJECTS := $(patsubst tools/rrnd_proxy/%.c, \
	tools/build/rrnd_proxy/%.o, $(rrnd_proxy_SOURCES))

tools/build/rrnd_proxy/%.o: tools/rrnd_proxy/%.c
	$(call cc-local)

tools/build/rrnd_proxy/rrnd_proxy: OBJECTS := \
	$(rrnd_proxy_OBJECTS) $(libupcn)
tools/build/rrnd_proxy/rrnd_proxy: CFLAGS_LOCAL += \
	-Iinclude -Itools/rrnd_proxy \
	-Itools/include
tools/build/rrnd_proxy/rrnd_proxy: $(rrnd_proxy_OBJECTS)
tools/build/rrnd_proxy/rrnd_proxy: $(libupcn)
tools/build/rrnd_proxy/rrnd_proxy: | tools/build/rrnd_proxy
	$(call link-local)


# --------------
# rrnd_perf_eval
# --------------

tools/build/rrnd_perf_eval/%.o: tools/rrnd_perf_eval/%.c
	$(call cc-local)

tools/build/rrnd_perf_eval/%: CFLAGS_LOCAL += \
	-Itools/include \
	-Iinclude \
	-Itools/rrnd_perf_eval
tools/build/rrnd_perf_eval/%: $(libupcn)
tools/build/rrnd_perf_eval/%: | tools/build/rrnd_perf_eval
	$(call link-local)


# -----
# tools
# -----

tools/build/rrnd_perf_eval/sgp4_perf: OBJECTS := \
	tools/build/rrnd_perf_eval/sgp4_perf.o $(libupcn)
tools/build/rrnd_perf_eval/sgp4_perf: tools/build/rrnd_perf_eval/sgp4_perf.o
tools: tools/build/rrnd_perf_eval/sgp4_perf

tools/build/rrnd_perf_eval/mpfit_perf: OBJECTS := \
	tools/build/rrnd_perf_eval/mpfit_perf.o $(libupcn)
tools/build/rrnd_perf_eval/mpfit_perf: tools/build/rrnd_perf_eval/mpfit_perf.o
tools: tools/build/rrnd_perf_eval/mpfit_perf

tools/build/rrnd_perf_eval/ipnd_perf: OBJECTS := \
	tools/build/rrnd_perf_eval/ipnd_perf.o $(libupcn)
tools/build/rrnd_perf_eval/ipnd_perf: tools/build/rrnd_perf_eval/ipnd_perf.o
tools: tools/build/rrnd_perf_eval/ipnd_perf


# Dependencies

-include $(tinycbor_DEPS)
-include $(libupcn_DEPS)
-include $(upcn_connect_DEPS)
-include $(upcn_netconnect_DEPS)
-include $(upcn_posix_performance_DEPS)
-include $(upcn_receive_DEPS)
-include $(upcn_test_DEPS)
-include $(upcn_parse_DEPS)
-include $(rrnd_proxy_DEPS)


# General

tools/build/drv: | tools/build
	$(call cmd,mkdir)

tools/build/libupcn: | tools/build
	$(call cmd,mkdir)

tools/build/libupcn/bundle6: | tools/build/libupcn
	$(call cmd,mkdir)

tools/build/libupcn/bundle7: | tools/build/libupcn
	$(call cmd,mkdir)

tools/build/tinycbor: | tools/build
	$(call cmd,mkdir)

tools/build/upcn_connect: | tools/build
	$(call cmd,mkdir)

tools/build/upcn_netconnect: | tools/build
	$(call cmd,mkdir)

tools/build/upcn_posix_performance: | tools/build
	$(call cmd,mkdir)

tools/build/upcn_receive: | tools/build
	$(call cmd,mkdir)

tools/build/upcn_test: | tools/build
	$(call cmd,mkdir)

tools/build/upcn_parse: | tools/build
	$(call cmd,mkdir)

tools/build/rrnd_proxy: | tools/build
	$(call cmd,mkdir)

tools/build/rrnd_perf_eval: | tools/build
	$(call cmd,mkdir)

tools/build:
	$(call cmd,mkdir)

clean::
	$(call cmd,rm,tools/build)

toolclean::
	$(call cmd,rm,tools/build)
