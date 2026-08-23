CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2

ifeq ($(OS),Windows_NT)
    TARGET = tetru.exe
    CFLAGS += -DPDC_WIDE -DPDC_FORCE_UTF8
    LDFLAGS ?= -lpdcurses
    RM = rm -f
else
    UNAME_S := $(shell uname -s)
    TARGET = tetru
    ifeq ($(UNAME_S),Darwin)
        LDFLAGS ?= -lncurses
    else
        LDFLAGS ?= -lncurses
    endif
    RM = rm -f
endif

SRCS = main.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

clean:
	$(RM) $(TARGET)

.PHONY: all clean
