CC       =  gcc
CFLAGS  += -std=c11 -Wall
OPTFLAGS = -O3 -g

.PHONY: all clean

TARGETS = main

all: $(TARGETS)

./main: main.o
	$(CC) $(CFLAGS) $(OPTFLAGS) -o $@ $^

run: clean all
	./main

run-valgrind: clean all
	valgrind --leak-check=full ./main

clean:
	rm -rf *.o ./main

%.o: %.c
	$(CC) $(CFLAGS) $(OPTFLAGS) -c -o $@ $<

