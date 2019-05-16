#!/bin/bash

# --codespell --codespellfile tools/checkpatch/spelling.txt \
CHECKPATCH_FLAGS="--no-tree --terse --show-types --ignore AVOID_EXTERNS \
--ignore LINE_CONTINUATIONS --ignore COMPLEX_MACRO --ignore DATE_TIME \
--ignore NEW_TYPEDEFS --ignore FUNCTION_ARGUMENTS --ignore SPACING --emacs"

S=0

CHECKPATCH=./tools/analysis/checkpatch.pl

check_dirs=(include/upcn \
	include/bundle6 \
	include/bundle7 \
	include/agents \
	include/spp \
	components/upcn/src \
	components/agents/src \
	components/bundle6/src \
	components/bundle7/src \
	components/test/include \
	components/test/src \
	components/test/posix/src \
	components/hal/src/posix \
	components/hal/src/stm32 \
	components/hal/include/generic \
	components/hal/include/posix \
	components/hal/include/stm32 \
	components/system/posix/drv/include \
	components/system/posix/drv/src \
	components/test/posix/src \
	components/cla/include \
	components/cla/include/posix/TCPCL \
	components/cla/include/posix/TCP_LEGACY \
	components/cla/include/stm32/USB_OTG \
	components/cla/src/posix/TCPCL \
	components/cla/src/posix/TCP_LEGACY \
	components/cla/src/posix/TCPSPP \
	components/cla/src/stm32/USB_OTG \
	components/spp/src \
	tools/include \
	tools/upcn_connect \
	tools/upcn_test \
	tools/upcn_receive \
	tools/rrnd_proxy \
	tools/rrnd_perf_eval \
	tools/rrnd_test \
	tools/upcn_posix_performance \
	tools/upcn_netconnect \
	)

for check_dir in "${check_dirs[@]}"; do
	echo "Checking $check_dir..."
	cnt=`ls -1 $check_dir/*.c 2>/dev/null | wc -l`
	if [ $cnt != 0 ]
	then
		$CHECKPATCH $CHECKPATCH_FLAGS --file $check_dir/*.c
		S=$(($S + $?))
	fi
	cnt=`ls -1 $check_dir/*.h 2>/dev/null | wc -l`
	if [ $cnt != 0 ]
	then
		$CHECKPATCH $CHECKPATCH_FLAGS --file $check_dir/*.h
		S=$(($S + $?))
	fi
done
exit $S
