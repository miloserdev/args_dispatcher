CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra
LDLIBS  = -lreadline
PREFIX  ?= /usr/local

SRC_DIR = src
HEADERS = $(wildcard $(SRC_DIR)/*.h $(SRC_DIR)/commands/*.h)
BUILD   = build

.PHONY: all install clean test

all: $(BUILD)/xf $(BUILD)/xf_repl

$(BUILD):
	mkdir -p $@

$(BUILD)/xf: $(SRC_DIR)/xf.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(BUILD)/xf_repl: $(SRC_DIR)/xf_repl.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

install: all
	install -D -m 0755 $(BUILD)/xf      $(DESTDIR)$(PREFIX)/bin/xf
	install -D -m 0755 $(BUILD)/xf_repl $(DESTDIR)$(PREFIX)/bin/xf_repl

clean:
	rm -rf $(BUILD)
test: all
	tests/run.sh
