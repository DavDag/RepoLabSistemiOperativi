CC = gcc
AR = ar
CFLAGS += -std=c99 -Wall -Werror -g
ARFLAGS = rvs
INCLUDES = -I. -I ./utils/includes
LDFLAGS = -L.
OPTFLAGS = -O3 
LIBS = -lpthread

TARGETS = main


.SUFFIXES: .c .h


%: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $< $(LDFLAGS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c -o $@ $<

all : $(TARGETS)

valgrind-test:
	valgrind --leak-check=full ./main

clean :
	rm -f $(TARGETS)