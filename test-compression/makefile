CC       =  gcc
CFLAGS  += -std=c11 -Wall
OPTFLAGS = -O3 -g

.PHONY: all clean

TARGETS = main main2

all: $(TARGETS)

./main: main.o
	$(CC) $(CFLAGS) $(OPTFLAGS) -o $@ $^

./main2: main2.o
	$(CC) $(CFLAGS) $(OPTFLAGS) -o $@ $^

clean:
	rm -rf *.o ./main ./main2

%.o: %.c
	$(CC) $(CFLAGS) $(OPTFLAGS) -c -o $@ $<

