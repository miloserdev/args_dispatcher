CC       ?= cc
CFLAGS   ?= -O2 -Wall -Wextra
CPPFLAGS += -Isrc
LDLIBS   = -lreadline
PREFIX   ?= /usr/local

LIB_HEADERS     = $(wildcard src/*.h)
EXAMPLE_DIR     = examples
EXAMPLE_HEADERS = $(wildcard $(EXAMPLE_DIR)/commands/*.h)
BUILD           = build

.PHONY: all install clean test

all: $(BUILD)/example $(BUILD)/example_repl

$(BUILD):
	mkdir -p $@

$(BUILD)/example: $(EXAMPLE_DIR)/example.c $(LIB_HEADERS) $(EXAMPLE_HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $<

$(BUILD)/example_repl: $(EXAMPLE_DIR)/example_repl.c $(LIB_HEADERS) $(EXAMPLE_HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

install: all
	install -D -m 0755 $(BUILD)/example      $(DESTDIR)$(PREFIX)/bin/example
	install -D -m 0755 $(BUILD)/example_repl $(DESTDIR)$(PREFIX)/bin/example_repl

clean:
	rm -rf $(BUILD)
test: all
	tests/run.sh
