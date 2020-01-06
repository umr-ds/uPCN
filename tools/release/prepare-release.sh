#!/bin/bash
BRANCH="master"
git checkout $BRANCH
CHECKOUT=$?
if [ $CHECKOUT -eq 0 ]; then
	git merge develop
	rm .gitlab-ci.yml .gitlab-ci-stm32.sh ./tools/release/prepare-release.sh
	sed -e '/.PHONY: prepare-release/,+2d' Makefile > Makefile.tmp; mv Makefile.tmp Makefile
	git commit -a
else
	echo "Checkout of branch $BRANCH failed!"
	exit 1
fi
