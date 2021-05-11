CC       =  gcc
CFLAGS  += -std=c99 -Wall -Werror -pedantic-errors
INCLUDES = -I $(PWD)/common/include -I ./src/include
LDFLAGS  = 
LIBS     = 
OPTFLAGS = -g

SOURCES_DIR  := src
OBJECTS_DIR  := obj
BINARIES_DIR := bin
SERVERAPI_LIB_DIR  := $(PWD)/serverapi/$(BINARIES_DIR)

export

TARGETS := all clean
SUBDIRS := server serverapi client

.PHONY: $(TARGETS) $(SUBDIRS) run-client run-valgrind-client run-server run-valgrind-server

$(TARGETS): $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

run-client:
	./client/bin/main

run-valgrind-client:
	$(MAKE) valgrind-test -C ./client

run-server:
	./server/bin/main

run-valgrind-server:
	$(MAKE) valgrind-test -C ./server

test1:


test2:


test3:

