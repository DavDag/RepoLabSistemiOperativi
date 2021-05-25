CC       =  gcc
CFLAGS  += -std=c11 -Wall -Werror -pedantic-errors -fPIC -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE
INCLUDES = -I $(PWD)/common/include -I ./include
LDFLAGS  = 
LIBS     = -L$(PWD)/common/$(BINARIES_DIR) -lcommon -pthread
OPTFLAGS = -g
AR       =  ar
ARFLAGS  =  rvs

SOURCES_DIR  := src
OBJECTS_DIR  := obj
BINARIES_DIR := bin

SERVERAPI_LIB_DIR  := $(PWD)/serverapi/$(BINARIES_DIR)

export

TARGETS := all clean
SUBDIRS := common server serverapi client

.PHONY: $(TARGETS) $(SUBDIRS) run-client run-valgrind-client run-server run-valgrind-server

$(TARGETS): $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)


test1:
	$(MAKE) -C server valgrind-test ARGS="../configs/test1.txt" &
	./scripts/test1.sh

test2:
	$(MAKE) -C server simple-run ARGS="../configs/test2.txt" &
	./scripts/test2.sh

test3:
	$(MAKE) -C server simple-run ARGS="../configs/test3.txt" &
	./scripts/test3.sh
