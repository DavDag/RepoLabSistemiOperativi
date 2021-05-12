OBJECTS := $(patsubst $(SOURCES_DIR)/%.c,$(OBJECTS_DIR)/%.o,$(wildcard $(SOURCES_DIR)/*.c))
EXE := $(BINARIES_DIR)/libcommon.a

.PHONY: all

all: $(EXE)

clean:
	rm -rf $(BINARIES_DIR) $(OBJECTS_DIR)

$(OBJECTS_DIR):
	mkdir -p $@

$(BINARIES_DIR):
	mkdir -p $@

$(EXE): $(OBJECTS) | $(BINARIES_DIR)
	$(AR) $(ARFLAGS) $@ $<

$(OBJECTS_DIR)/%.o: $(SOURCES_DIR)/%.c | $(OBJECTS_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c -o $@ $<

