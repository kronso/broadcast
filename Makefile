CC = gcc
CFLAGS = -g -Wall -Wextra -DDEBUG

INCDIR = include
SRCDIR = src

CFILES = $(wildcard $(SRCDIR)/*.c)
HFILES = $(wildcard $(INCDIR)/*.h)

LIBS = 

all: sockets

ifeq ($(OS), Windows_NT)
LIBS += -luser32 -lws2_32 -liphlpapi
CFLAGS += -DWIN_PLATFORM
else ifeq ($(shell "uname -s"), Linux)
CFLAGS += -DLINUX_PLATFORM
else ifeq ($(shell "uname -s"), Darwin)
CFLAGS += -DLINUX_PLATFORM
endif

sockets: sockets.c $(CFILES) $(HFILES)
	@$(CC) $(CFLAGS) -o $@ $^ -I$(INCDIR) $(LIBS)

clean:
	file -f 

