SOURCES := $(wildcard $(SOURCES_DIR)/*.c)
OBJECTS := $(SOURCES:$(SOURCES_DIR)/%.c=$(OBJECTS_DIR)/%.o)
EXE := $(BINARIES_DIR)/main

all: $(EXE)

valgrind-test:
	valgrind --leak-check=full $(EXE)

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
