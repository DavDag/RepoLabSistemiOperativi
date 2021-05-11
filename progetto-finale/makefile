MAKEFLAGS += -s

CC       =  gcc
CFLAGS  += -std=c11 -Wall -Werror -pedantic-errors
INCLUDES = -I $(PWD)/common/include -I ./include
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
	./client/bin/main -T arg_pars,"-h option" -h
	./client/bin/main -T arg_pars,"-f option" -f ./dir/file
	./client/bin/main -T arg_pars,"-w option" -w ./dir -w ./dir,n=32
	./client/bin/main -T arg_pars,"-W option" -W ./file1 -W ./file1,./dir/file2,./file3
	./client/bin/main -T arg_pars,"-D option" -D ./dir
	./client/bin/main -T arg_pars,"-r option" -r ./file1,./dir/file2,./file3
	./client/bin/main -T arg_pars,"-R option" -R -R n=2
	./client/bin/main -T arg_pars,"-d option" -d ./dir
	./client/bin/main -T arg_pars,"-t option" -t 128
	./client/bin/main -T arg_pars,"-l option" -l ./file1 -l ./file1,./dir/file2,./file3
	./client/bin/main -T arg_pars,"-u option" -u ./file1 -u ./file1,./dir/file2,./file3
	./client/bin/main -T arg_pars,"-c option" -c ./file1 -c ./file1,./dir/file2,./file3
	./client/bin/main -T arg_pars,"-p option" -p

