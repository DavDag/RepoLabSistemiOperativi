CC       =  gcc
CFLAGS  += -std=c11 -Wall -Werror -pedantic-errors -fPIC -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE
INCLUDES = -I $(PWD)/common/include -I ./include
LDFLAGS  = 
LIBS     = -L$(PWD)/common/$(BINARIES_DIR) -lcommon
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
	

test2:


test3:


#
# Shortcuts to run binaries
#
run-client:
	$(MAKE) -C client simple-run ARGS="-p -L5 -w ./dir"

run-valgrind-client:
	$(MAKE) -C client valgrind-test ARGS="-p -w ./dir"

run-server:
	$(MAKE) -C server simple-run ARGS=""

run-valgrind-server:
	$(MAKE) -C server valgrind-test ARGS=""

#
# Testing
#
test-client:
	$(MAKE) test-client-impl TARGET=simple-run

test-client-valgrind:
	$(MAKE) test-client-impl TARGET=valgrind-test

test-client-valgrind-quiet:
	$(MAKE) test-client-impl TARGET=valgrind-test-quiet

test-client-impl:
	$(MAKE) -C client $(TARGET) ARGS="-p -W ./file1 -t 1000 -W ./file1,./dir/file2,./file3 -D ./dir" &
	$(MAKE) -C client $(TARGET) ARGS="-p -f ./dir/socket -w ./dir -t 750 -w ./dir,n=32" &
	$(MAKE) -C client $(TARGET) ARGS="-p -r ./file1 -t 1500 -r./dir/file2,./file3" &
	$(MAKE) -C client $(TARGET) ARGS="-p -R -t 2000 -R n=2 -d ./dir" &
	$(MAKE) -C client $(TARGET) ARGS="-p -t 500 -l ./file1 -t 2000 -l ./file1,./dir/file2,./file3" &
	$(MAKE) -C client $(TARGET) ARGS="-p -u ./file1 -t 1000 -u ./file1,./dir/file2,./file3" &
	$(MAKE) -C client $(TARGET) ARGS="-p -t 2500 -c ./file1 -t 1100 -c ./file1,./dir/file2,./file3"
	$(MAKE) -C client $(TARGET) ARGS="-p -L0 -L1 -L2 -L3 -L4"
