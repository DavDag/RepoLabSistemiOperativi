OBJECTS := $(patsubst $(SOURCES_DIR)/%.c,$(OBJECTS_DIR)/%.o,$(wildcard $(SOURCES_DIR)/*.c))
EXE := $(BINARIES_DIR)/main

INCLUDES += -I "../serverapi/include"
LIBS += -L$(COMMON_LIB_DIR) -lcommon -L$(SERVERAPI_LIB_DIR) -lserverapi -Wl,-rpath=$(SERVERAPI_LIB_DIR)

.PHONY: all

all: $(EXE)

simple-run:
	$(EXE) $(ARGS)

valgrind-test:
	valgrind --leak-check=full $(EXE) $(ARGS)

valgrind-test-quiet:
	valgrind --leak-check=full --quiet $(EXE) $(ARGS)

clean: $(BINARIES_DIR)
	rm -rf $(BINARIES_DIR) $(OBJECTS_DIR)

$(OBJECTS_DIR):
	mkdir -p $@

$(BINARIES_DIR):
	mkdir -p $@

$(EXE): $(OBJECTS) | $(BINARIES_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS) 

$(OBJECTS_DIR)/%.o: $(SOURCES_DIR)/%.c | $(OBJECTS_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c -o $@ $<
