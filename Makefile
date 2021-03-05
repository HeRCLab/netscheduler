CC             ?= gcc
CFLAGS          = -g -Wno-format-truncation -Wextra -Wall -Wpedantic -std=gnu11 $(shell libherc-config --cflags)
SOURCES         = $(wildcard src/*.c)
OBJECTS         = $(patsubst %.c,%.o,$(SOURCES))
HEADERS         = $(wildcard src/*.h)
LDFLAGS         = $(shell libherc-config --libs)

# Use make LIBHERC_CONFIG=/some/path/to/libherc-config if your installation
# does not have it placed in PATH.
LIBHERC_CONFIG ?= libherc-config
LIBHERC_MIN_VERSION = 0.0.1

all: schednet
.PHONY: all

# Phony target that just asserts libherc is installed, since we have it as a
# dependency.
libherc:
	@if [ ! -x "$$(which $(LIBHERC_CONFIG) 2>/dev/null)" ] ; then \
		echo "No libherc-config found, maybe you forgot to set LIBHERC_CONFIG?" 2>&1 ; \
		echo "If you don't have it already, install libherc from here: https://github.com/HeRCLab/libherc" 2>&1 ; \
		exit 1 ; \
	fi
	@if ! $(LIBHERC_CONFIG) --want $(LIBHERC_MIN_VERSION) > /dev/null 2>&1 ; then \
		echo "$(LIBHERC_CONFIG) --want $(LIBHERC_MIN_VERSION) failed, maybe your libherc installation is too old, at least $(LIBHERC_MIN_VERSION) is required"; \
		exit 1 ; \
	fi
.PHONY: libherc

src/%.o: src/%.c $(HEADERS) libherc
	$(CC) $(CFLAGS) -c $< -o $@

schednet: $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f schednet $(OBJECTS)
.PHONY: clean
