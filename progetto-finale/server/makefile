OBJECTS := $(patsubst $(SOURCES_DIR)/%.c,$(OBJECTS_DIR)/%.o,$(wildcard $(SOURCES_DIR)/*.c))
EXE := $(BINARIES_DIR)/main

LIBS += -lpthread

.PHONY: all

all: $(EXE)

simple-run:
	$(EXE) $(ARGS)

valgrind-test:
	valgrind --leak-check=full --track-origins=yes $(EXE) $(ARGS)

valgrind-test-quiet:
	valgrind --leak-check=full --track-origins=yes --quiet $(EXE) $(ARGS)

clean:
	rm -rf $(BINARIES_DIR) $(OBJECTS_DIR)

$(OBJECTS_DIR):
	mkdir -p $@

$(BINARIES_DIR):
	mkdir -p $@

$(EXE): $(OBJECTS) | $(BINARIES_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

$(OBJECTS_DIR)/%.o: $(SOURCES_DIR)/%.c | $(OBJECTS_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c -o $@ $<
