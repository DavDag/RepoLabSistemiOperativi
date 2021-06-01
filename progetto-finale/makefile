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

CLIENT_EXE := ./client/$(BINARIES_DIR)/main
SERVER_EXE := ./server/$(BINARIES_DIR)/main

all-db:
	$(eval CFLAGS += -DLOG_DEBUG)
	$(MAKE) -C . clean
	$(MAKE) -C . all

all-comp:
	$(eval CFLAGS += -DCOMPRESS_MESSAGES)
	$(MAKE) -C . clean
	$(MAKE) -C . all

clean: resettest
	@rm -f ./log-test1.txt
	@rm -f ./log-test2.txt
	@rm -f ./log-test3.txt
	@rm -f ./Available

test1: resettest | addperm files
	@valgrind --leak-check=full $(SERVER_EXE) ./configs/test1.txt & \
	./scripts/test1.sh $(CLIENT_EXE); \
	kill -1 $$!; \
	wait;

test2: resettest | addperm files
	@$(SERVER_EXE) ./configs/test2.txt & \
	./scripts/test2.sh $(CLIENT_EXE); \
	kill -1 $$!; \
	wait;

test3: resettest | addperm files
	@$(SERVER_EXE) ./configs/test3.txt & \
	./scripts/test3.sh $(CLIENT_EXE); \
	kill -2 $$!; \
	wait;

clean-files:
	@rm -rf ./tdir/longdir
	@rm -rf ./tdir/bigdir

addperm:
	@chmod +rx $(CLIENT_EXE)
	@chmod +rx $(SERVER_EXE)

tdir/bigdir:
	python3 ./tdir/gen_files.py -numFiles=8 -minSizeKb=512 -maxSizeKb=4096 -outDir=./tdir/bigdir

tdir/longdir:
	python3 ./tdir/gen_files.py -numFiles=64 -minSizeKb=1 -maxSizeKb=64 -outDir=./tdir/longdir

files: tdir/bigdir tdir/longdir

resettest:
	@rm -rf ./out
