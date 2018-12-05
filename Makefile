#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := console

include $(IDF_PATH)/make/project.mk

TIMESTAMP := $(shell date  "+%Y-%m-%d\ at\ %H:%M:%S")
GIT_BRANCH := $(shell git status | head -n 1  | sed 's/On branch \(.*\)$$/\1/')
GIT_VERSION := $(shell git log | head -n 1 | cut -d ' ' -f 2)

CFLAGS += -DOMAR_TIMESTAMP=\"$(TIMESTAMP)\" -DOMAR_VERSION=\"$(GIT_VERSION)\" -DOMAR_BRANCH=\"$(GIT_BRANCH)\" -DPOWERTEST
