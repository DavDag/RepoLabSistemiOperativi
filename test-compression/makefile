CC = gcc
CFLAGS += -std=c99 -Wall -Werror -g
OPTFLAGS = -O3 

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
