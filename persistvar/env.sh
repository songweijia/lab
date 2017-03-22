#!/bin/bash
# some common definitions
EXTRAS=extras
DEPENDENCIES=dependencies
BUILD=build

assert_success () {
  err=$?
  if [ ! ${err} -eq 0 ]; then
    echo "Failed at " $@ ",error=" $?
    echo "quit..."
    exit ${err}
  fi
}
