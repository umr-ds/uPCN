#!/bin/bash

# --codespell --codespellfile tools/checkpatch/spelling.txt \
CHECKPATCH_FLAGS="--no-tree --terse --show-types --ignore AVOID_EXTERNS \
--ignore LINE_CONTINUATIONS --ignore COMPLEX_MACRO --ignore DATE_TIME \
--ignore NEW_TYPEDEFS --ignore FUNCTION_ARGUMENTS --ignore SPACING \
--ignore LONG_LINE_STRING --emacs"

S=0

CHECKPATCH=./external/checkpatch/checkpatch.pl

check_dirs=(include components test/unit)

for check_dir in "${check_dirs[@]}"; do
	sub_dirs="$(find "$check_dir" -type d)"
	for dir in $sub_dirs; do
		cnt=`ls -1 $dir/*.c 2>/dev/null | wc -l`
		if [ $cnt != 0 ]
		then
			echo "Checking $dir/*.c..."
			$CHECKPATCH $CHECKPATCH_FLAGS --file $dir/*.c
			S=$(($S + $?))
		fi
		cnt=`ls -1 $dir/*.h 2>/dev/null | wc -l`
		if [ $cnt != 0 ]
		then
			echo "Checking $dir/*.h..."
			$CHECKPATCH $CHECKPATCH_FLAGS --file $dir/*.h
			S=$(($S + $?))
		fi
	done
done

exit $S
