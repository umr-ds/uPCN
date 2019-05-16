###############################################################################
# Default Commands
###############################################################################

.PHONY: all
all: posix-local tools

.PHONY: upcn
upcn: posix-local

.PHONY: tools
tools:

.PHONY: clean
clean::

.PHONY: uclean
uclean::

.PHONY: tclean
tclean::

.PHONY: toolclean
toolclean::

###############################################################################
# Execution and Deployment
###############################################################################

.PHONY: run-posix-local
run-posix-local: posix-local
	./build/posix_local/upcn

.PHONY: run-posix-dev
run-posix-dev: posix-dev
	ssh $(POSIX_DEV_USERNAME)@$(POSIX_DEV_IP) './upcn'

.PHONY: run-unittest-posix-local
run-unittest-posix-local: unittest-posix-local
	./components/test/build/posix_local/testupcn

.PHONY: run-queuetest-posix-local
run-queuetest-posix-local: queuetest-posix-local
	./components/test/posix/build/queuetest

.PHONY: deploy-posix-dev
deploy-dev: posix-dev
	ssh $(POSIX_DEV_USERNAME)@$(POSIX_DEV_IP) 'pkill -x -KILL upcn'
	scp -q build/posix_dev/upcn $(POSIX_DEV_USERNAME)@$(POSIX_DEV_IP):

.PHONY: deploy-run-posix-dev
deploy-run-dev: deploy-dev run-dev

.PHONY: burn-stm32
burn-stm32: stm32
	$(ST_FLASH) --reset write build/stm32/upcn.bin 0x08000000

.PHONY: burn-unittest-stm32
burn-unittest-stm32: unittest-stm32
	$(ST_FLASH) --reset write components/test/build/stm32/testupcn.bin 0x08000000

.PHONY: flash-upcn-stm32
flash-upcn-stm32: stm32
flash-upcn-stm32: | /usr/bin/expect
flash-upcn-stm32: | /usr/bin/telnet
	tools/ocdflash.exp build/stm32/upcn.bin

.PHONY: flash-unittest-stm32
flash-unittest-stm32: unittest-stm32
flash-unittest-stm32: | /usr/bin/expect
flash-unittest-stm32: | /usr/bin/telnet
	tools/ocdflash.exp components/test/build/stm32/testupcn.bin

###############################################################################
# Tools
###############################################################################

.PHONY: gdb-upcn-stm32
gdb-upcn-stm32: stm32
	$(TOOLCHAIN_STM32)gdb --eval-command="target remote :4242" \
		--eval-command "cont" build/stm32/upcn

.PHONY: gdb-upcn-posix-local
gdb-upcn-posix-local: posix-local
	gdb build/posix_local/upcn

.PHONY: gdb-unittest-stm32
gdb-unittest-stm32: unittest-stm32
	$(TOOLCHAIN_STM32)gdb --eval-command="target remote :4242" \
		--eval-command "cont" components/test/build/stm32/testupcn

.PHONY: stutil
stutil:
	$$(dirname $(ST_FLASH))/st-util -p 4242

.PHONY: connect
connect: tools
	./tools/build/upcn_connect/upcn_connect \
		/dev/serial/by-id/*STM32_Virtual* \
		tcp://127.0.0.1:8726 tcp://127.0.0.1:8727

.PHONY: netconnect
netconnect: tools
	./tools/build/upcn_netconnect/upcn_netconnect \
		tcp://127.0.0.1:4200 \
		tcp://127.0.0.1:8726 \
		tcp://127.0.0.1:8727

.PHONY: receive
receive: tools
	./tools/build/upcn_receive/upcn_receive tcp://127.0.0.1:8726

###############################################################################
# Tests
###############################################################################

.PHONY: test-routing
test-routing: tools
	./tools/build/upcn_test/upcn_test \
		tcp://127.0.0.1:8726 tcp://127.0.0.1:8727 \
		test routing

.PHONY: test-probabilistic-forwarding
test-probabilistic-forwarding: tools
	./tools/build/upcn_test/upcn_test \
		tcp://127.0.0.1:8726 tcp://127.0.0.1:8727 \
		test prob

.PHONY: test-performance
test-performance: tools
	./tools/build/upcn_test/upcn_test \
		tcp://127.0.0.1:8726 tcp://127.0.0.1:8727 \
		test perf

.PHONY: test-throughput
test-throughput: tools
	./tools/build/upcn_test/upcn_test \
		tcp://127.0.0.1:8726 tcp://127.0.0.1:8727 \
		test throughput

.PHONY: send-reset
send-reset: tools
	./tools/build/upcn_test/upcn_test \
		tcp://127.0.0.1:8726 tcp://127.0.0.1:8727 \
		reset

.PHONY: send-query
send-query: tools
	./tools/build/upcn_test/upcn_test \
		tcp://127.0.0.1:8726 tcp://127.0.0.1:8727 \
		query

.PHONY: check-connection
check-connection: tools
	./tools/build/upcn_test/upcn_test \
		tcp://127.0.0.1:8726 tcp://127.0.0.1:8727 \
		check

###############################################################################
# RRND
###############################################################################

.PHONY: nd-local
nd-local: tools
	./tools/build/rrnd_proxy/rrnd_proxy tcp://127.0.0.1:7763

.PHONY: nd-local-callgrind
nd-local-callgrind: tools
nd-local-callgrind: | /usr/bin/valgrind
	mkdir -pv ndtest && \
	/usr/bin/valgrind --tool=callgrind --dump-instr=yes \
		--simulate-cache=yes --collect-jumps=yes \
		--callgrind-out-file=./ndtest/rrnd_proxy.%p.callgrind \
		./tools/build/rrnd_proxy/rrnd_proxy tcp://127.0.0.1:7763

.PHONY: nd-remote
nd-remote: tools
	./tools/build/rrnd_proxy/rrnd_proxy tcp://127.0.0.1:7763 \
		tcp://127.0.0.1:8726 tcp://127.0.0.1:8727

.PHONY: nd-unittests
nd-unittests:
	nosetests tools/rrnd_test/test

.PHONY: nd-test-scenario/%, nd-test-scenario/
nd-test-scenario/%:
	if [ "$(parameter)" = "" ]; then p=0; else p=$(parameter); fi; \
	mkdir -pv ndtest && cd tools && \
	python -m rrnd_test -t $$(basename $@) -p $$p -f \
	../ndtest/$$(basename $@)_$$(date +%s).out
nd-test-scenario/:
	$(error You have to specify the test: make nd-test-scenario/[1-11])

.PHONY: nd-check-style
nd-check-style:
	python -m flake8 --max-complexity 10 ./tools/rrnd_test

_VENV_NAME=upcn

.PHONY: nd-create-virtualenv
nd-create-virtualenv:
	bash -c ". $$(which virtualenvwrapper.sh); \
		mkvirtualenv -ppython3 --system-site-packages $(_VENV_NAME) && \
		pip install -U setuptools pip wheel && \
		pip install -U -r ./tools/rrnd_test/requirements.txt;"; \
	echo "=> To activate the virtualenv, use: workon $(_VENV_NAME)"

.PHONY: nd-update-virtualenv
nd-update-virtualenv:
	bash -c ". $$(which virtualenvwrapper.sh); \
		workon $(_VENV_NAME) && \
		pip install -U setuptools pip wheel && \
		pip install -U -r ./tools/rrnd_test/requirements.txt;"; \
	echo "=> To activate the virtualenv, use: workon $(_VENV_NAME)"

.PHONY: nd-perfeval-sgp4
nd-perfeval-sgp4: tools
nd-perfeval-sgp4: | /usr/bin/valgrind
	mkdir -pv ndtest && \
	/usr/bin/valgrind --tool=callgrind --dump-instr=yes \
		--simulate-cache=yes --collect-jumps=yes \
		--callgrind-out-file=./ndtest/sgp4_perf.%p.callgrind \
		./tools/build/rrnd_perf_eval/sgp4_perf

.PHONY: nd-perfeval-mpfit
nd-perfeval-mpfit: tools
nd-perfeval-mpfit: | /usr/bin/valgrind
	mkdir -pv ndtest && \
	/usr/bin/valgrind --tool=callgrind --dump-instr=yes \
		--simulate-cache=yes --collect-jumps=yes \
		--callgrind-out-file=./ndtest/mpfit_perf.%p.callgrind \
		./tools/build/rrnd_perf_eval/mpfit_perf

.PHONY: nd-perfeval-ipnd
nd-perfeval-ipnd: tools
nd-perfeval-ipnd: | /usr/bin/valgrind
	mkdir -pv ndtest && \
	/usr/bin/valgrind --tool=callgrind --dump-instr=yes \
		--simulate-cache=yes --collect-jumps=yes \
		--callgrind-out-file=./ndtest/ipnd_perf.%p.callgrind \
		./tools/build/rrnd_perf_eval/ipnd_perf

###############################################################################
# Code Quality Tests (and Release Tool)
###############################################################################

.PHONY: count-lines
count-lines:
	find components/upcn components/hal -name "*.c" -or -name "*.h" | xargs wc -l

.PHONY: check-style
check-style:
	./tools/analysis/stylecheck.sh

.PHONY: clang-check-stm32
clang-check-stm32:
	./tools/analysis/clang-check.sh \
		"stm32 unittest-stm32 tools" \
		"components/hal/src/stm32"

.PHONY: clang-tidy-stm32
clang-tidy-stm32:
	./tools/analysis/clang-check.sh tidy \
		"stm32 unittest-stm32 tools" \
		"components/hal/src/stm32"

.PHONY: clang-check-posix
clang-check-posix:
	./tools/analysis/clang-check.sh \
		"posix-local unittest-posix-local tools" \
		"components/hal/src/posix" \
		"components/cla/src/posix/TCP_LEGACY" \
		"components/cla/src/posix/TCPCL"

.PHONY: clang-tidy-posix
clang-tidy-posix:
	./tools/analysis/clang-check.sh tidy \
		"posix-local unittest-posix-local tools" \
		"components/hal/src/posix" \
		"components/cla/src/posix/TCP_LEGACY" \
		"components/cla/src/posix/TCPCL"


###############################################################################
# Flags
###############################################################################

CFLAGS_WEXTRA = -Wextra -Wno-unused-parameter \
	-Wno-override-init -Wno-unused-but-set-parameter -Wno-error=extra

ifeq "$(type)" "release"
  CFLAGS += $(CFLAGS_ALL) -DDEBUG_FREERTOS_TRACE
  CFLAGS_LOCAL += $(CFLAGS_ALL) -DUPCN_LOCAL
else
  CFLAGS += -Wall -g -DDEBUG -DDEBUG_FREERTOS_TRACE
  CFLAGS_LOCAL += -Wall -g -DDEBUG -DUPCN_LOCAL
endif

ifeq "$(memdebug)" "strict"
  CFLAGS += -DMEMDEBUG_STRICT
else
  ifeq "$(memdebug)" "no"
    CFLAGS += -DNO_MEMDEBUG
  endif
endif

ifeq "$(log)" "yes"
  CFLAGS += -DLOGGING
else
  ifeq "$(log)" "no"
    CFLAGS += -DNO_LOGGING
  else
    CFLAGS += -DNO_LOGGING -DLOG_PRINT
  endif
endif

ifeq "$(verbose)" "yes"
  CFLAGS += -DVERBOSE
else
  ifeq "$(verbose)" "no"
    CFLAGS += -DQUIET
  endif
endif

ifeq "$(bprint)" "yes"
  CFLAGS += -DBUNDLE_PRINT
endif

ifeq "$(optimize)" "yes"
  CFLAGS += -O3
  CFLAGS_LOCAL += -O3
endif

ifneq "$(wextra)" "no"
  CFLAGS += $(CFLAGS_WEXTRA)
  CFLAGS_LOCAL += $(CFLAGS_WEXTRA)
endif

ifeq "$(werror)" "yes"
  CFLAGS += -Werror
  CFLAGS_LOCAL += -Werror
endif

ifeq "$(target)" "somp2"
  CFLAGS += -DBOARD_SOMP2
endif

ifeq "$(perf_test)" "throughput"
  CFLAGS += -DTHROUGHPUT_TEST
endif

CFLAGS += $(CFLAGS_EXTRA)
LDFLAGS += $(LDFLAGS_EXTRA)

-include config.mk

.PHONY: posix-dev posix-local stm32

###############################################################################
# uPCN-Builds
###############################################################################

ifndef PLATFORM

posix-local: export PLATFORM := posix_local
posix-local: export ARCHITECTURE := posix
posix-local: export TOOLCHAIN := $(TOOLCHAIN_POSIX_LOCAL)

posix-dev: export PLATFORM := posix_dev
posix-dev: export ARCHITECTURE := posix
posix-dev: export TOOLCHAIN := $(TOOLCHAIN_POSIX_DEV)

stm32: export PLATFORM := stm32
stm32: export ARCHITECTURE := stm32
stm32: export TOOLCHAIN := $(TOOLCHAIN_STM32)

posix-local:
	@$(MAKE) build/posix_local/upcn

posix-dev:
	@$(MAKE) build/posix_dev/upcn

stm32:
	@$(MAKE) build/stm32/upcn.bin

endif #PLATFORM

###############################################################################
# Test-Builds
###############################################################################

ifndef PLATFORM

unittest-posix-local: export PLATFORM := posix_local
unittest-posix-local: export ARCHITECTURE := posix
unittest-posix-local: export TOOLCHAIN := $(TOOLCHAIN_POSIX_LOCAL)

queuetest-posix-local: export PLATFORM := posix_local
queuetest-posix-local: export ARCHITECTURE := posix
queuetest-posix-local: export TOOLCHAIN := $(TOOLCHAIN_POSIX_LOCAL)

unittest-stm32: export PLATFORM := stm32
unittest-stm32: export ARCHITECTURE := stm32
unittest-stm32: export TOOLCHAIN := $(TOOLCHAIN_STM32)

unittest-posix-local:
	@$(MAKE) -s components/test/build/posix_local/testupcn

unittest-stm32:
	@$(MAKE) -s components/test/build/stm32/testupcn.bin

queuetest-posix-local:
	@$(MAKE) components/test/posix/build/queuetest

endif #PLATFORM

###############################################################################
# Included Makefiles
###############################################################################

include build.mk

include components/system/toolchain.mk

include components/system/posix/board.mk
include components/system/posix/drv/build.mk

include components/system/stm32/board.mk
include components/system/stm32/build.mk
include components/system/stm32/drv/build.mk

include components/hal/src/posix/build.mk
include components/hal/src/stm32/build.mk

include components/test/posix/build.mk
include components/test/build.mk

include components/drv/build.mk
include external/build.mk
include components/upcn/build.mk

include components/cla/build.mk
include components/agents/build.mk


include tools/build.mk
