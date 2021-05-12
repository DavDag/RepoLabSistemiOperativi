MAKEFLAGS += -s

CC       =  gcc
CFLAGS  += -std=c11 -Wall -Werror -pedantic-errors
INCLUDES = -I $(PWD)/common/include -I ./include
LDFLAGS  = 
LIBS     = 
OPTFLAGS = -g
AR       =  ar
ARFLAGS  =  crs

SOURCES_DIR  := src
OBJECTS_DIR  := obj
BINARIES_DIR := bin

SERVERAPI_LIB_DIR  := $(PWD)/serverapi/$(BINARIES_DIR)
COMMON_LIB_DIR     := $(PWD)/common/$(BINARIES_DIR)

export

TARGETS := all clean
SUBDIRS := common server serverapi client

.PHONY: $(TARGETS) $(SUBDIRS) run-client run-valgrind-client run-server run-valgrind-server

$(TARGETS): $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)


test1:
	

test2:


test3:


#
# Shortcuts to run binaries
#

run-client:
	./client/bin/main

run-valgrind-client:
	$(MAKE) valgrind-test -C ./client

run-server:
	./server/bin/main

run-valgrind-server:
	$(MAKE) valgrind-test -C ./server

#
# Testing
#
test-client-args:
	./client/bin/main -f ./dir/file
	./client/bin/main -w ./dir -w ./dir,n=32
	./client/bin/main -W ./file1 -W ./file1,./dir/file2,./file3
	./client/bin/main -D ./dir
	./client/bin/main -r ./file1,./dir/file2,./file3
	./client/bin/main -R -R n=2
	./client/bin/main -d ./dir
	./client/bin/main -t 128
	./client/bin/main -l ./file1 -l ./file1,./dir/file2,./file3
	./client/bin/main -u ./file1 -u ./file1,./dir/file2,./file3
	./client/bin/main -c ./file1 -c ./file1,./dir/file2,./file3
	./client/bin/main -p
	./client/bin/main -L0 -L1 -L2 -L3 -L4 -L5
