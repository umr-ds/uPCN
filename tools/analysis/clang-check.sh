#!/bin/bash

set -o errexit
set -o nounset
set -o pipefail

DIRS=(
    components/upcn/src
    components/test/src
    components/system/posix/drv/src
    tools/upcn_connect
    tools/upcn_netconnect
    tools/upcn_receive
    tools/upcn_test
    tools/rrnd_perf_eval
    tools/rrnd_proxy
    tools/upcn_posix_performance
)
CHECKER_ARGS=(
    -Qunused-arguments
    -m32
    -Wextra
    -Wno-unused-parameter
    -fdiagnostics-color
)

if [[ ! -r ./Makefile ]]; then
    echo 'Please run this script from the main project directory!' >&2
    exit 1
fi

function clang-binary {
    for suffix in 3.10 3.9 3.8 3.7 3.6; do
        if which "$1-$suffix" > /dev/null 2>&1; then
            echo "$(which "$1-$suffix")"
            return 0
        fi
    done
    echo "$(which "$1")"
}

BEAR="$(which bear)"
CLANG_CHECK="$(clang-binary clang-check)"
CLANG_TIDY="$(clang-binary clang-tidy)"

CHECKER_CMD_ARGS=""
for arg in "${CHECKER_ARGS[@]}"; do
    CHECKER_CMD_ARGS="$CHECKER_CMD_ARGS -extra-arg=$arg"
done

if [[ "${1:-}" == "tidy" ]]; then
    echo 'Running clang-tidy instead of clang-check'
    CHECKER="$CLANG_TIDY"
    CHECKER_CMD_ARGS="$CHECKER_CMD_ARGS -header-filter='.*'"
    TARGETS="${2:-}"
    shift
    shift
    DIRS+=("${@}")
else
    CHECKER="$CLANG_CHECK"
    CHECKER_CMD_ARGS="$CHECKER_CMD_ARGS -analyze"
    TARGETS="${1:-}"
    shift
    DIRS+=("${@}")
fi

rm -fv ./compile_commands.json
make clean
$BEAR make $TARGETS wextra=no

if [[ ! -r ./compile_commands.json ]]; then
    echo 'compile_commands.json not created, please check your setup!' >&2
    exit 1
fi

function check-dir {
    echo "Checking $1..."
    local out="$("$CHECKER" $CHECKER_CMD_ARGS $1/*.c 2>&1 | tee >(cat 1>&2))"
    if ! (echo "$out" | grep -i -e 'error' -e 'warning' > /dev/null); then
        return 0
    else
        echo "Check for $1 failed"
        return 1
    fi
}

FAIL=0
for checkdir in "${DIRS[@]}"; do
    if ! check-dir "$checkdir"; then
        FAIL=$[FAIL + 1]
    fi
done

rm -f ./*.plist

if [[ $FAIL -ne 0 ]]; then
    echo 'At least one check failed'
    exit 1
fi
