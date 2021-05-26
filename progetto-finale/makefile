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
TEST1PID   := test1.pid

test1:
	@valgrind --leak-check=full $(SERVER_EXE) ./configs/test1.txt & \
	./scripts/test1.sh $(CLIENT_EXE); \
	kill -1 $$!; \
	wait;

test2:
	@$(SERVER_EXE) ./configs/test2.txt & \
	./scripts/test2.sh $(CLIENT_EXE); \
	kill -1 $$!; \
	wait;

test3:
	@$(SERVER_EXE) ./configs/test3.txt & \
	timeout 30 ./scripts/test3.sh $(CLIENT_EXE); \
	kill -2 $$!; \
	wait;
