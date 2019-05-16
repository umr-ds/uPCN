
drv_SOURCES := $(wildcard components/system/posix/drv/src/*.c)

drv_DEPS := $(patsubst components/system/posix/drv/src/%.c,\
	components/test/posix/build/drv/%.d, $(drv_SOURCES))

drv_OBJECTS := $(patsubst components/system/posix/drv/src/%.c,\
	components/test/posix/build/drv/%.o, $(drv_SOURCES))

testposix_SOURCES := $(wildcard components/test/posix/src/*.c)

testposix_OBJECTS := $(patsubst components/test/posix/src/%.c,\
	components/test/posix/build/%.o, $(testposix_SOURCES))

testposix_LIBS := components/test/unity/build/$(PLATFORM)/libunity.a \

$(eval $(call toolchain,components/test/posix/build/queuetest))

-include $(drv_DEPS)

components/test/posix/build/drv/%.o: components/system/posix/drv/src/%.c
	$(call cmd,cc_o_c)
	
components/test/posix/build/%.o: components/test/posix/src/%.c
	$(call cmd,cc_o_c)

components/test/posix/build/queuetest: OBJECTS := $(testposix_OBJECTS) $(drv_OBJECTS)
components/test/posix/build/queuetest: LIBS    := $(testposix_LIBS)
components/test/posix/build/queuetest: CFLAGS  += -Icomponents/test/posix/include \
					   -Icomponents/test/unity/include \
					   -Icomponents/system/posix/drv/include
components/test/posix/build/queuetest: LDFLAGS  += -lm -pthread
components/test/posix/build/queuetest: $(testposix_LIBS)
components/test/posix/build/queuetest: $(testposix_OBJECTS) $(drv_OBJECTS)
components/test/posix/build/queuetest: | components/test/posix/build/drv
	$(call cmd,link)

components/test/posix/build:
	$(call cmd,mkdir)

components/test/posix/build/drv: | components/test/posix/build
	$(call cmd,mkdir)

clean::
	$(call cmd,rm,components/test/posix/build)

tclean::
	$(call cmd,rm,components/test/posix/build)
