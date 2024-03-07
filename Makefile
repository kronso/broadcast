CC = gcc
CFLAGS = -g -Wall -Wextra -DDEBUG

INCDIR = include
SRCDIR = src

CFILES = $(wildcard $(SRCDIR)/*.c)
HFILES = $(wildcard $(INCDIR)/*.h)
OBJS = $(patsubst %.c,%.o,common.c debug.c) 

LDFLAGS = 

CLEAN_CMD = 

all: sockets clean
	
ifeq ($(OS), Windows_NT)
LDFLAGS += -luser32 -lws2_32 -liphlpapi
CLEAN_CMD = del *.o/s
else ifeq ($(shell "uname -s"), Linux)
CLEAN_CMD = rm *.o
else ifeq ($(shell "uname -s"), Darwin)
CLEAN_CMD = rm *.o
endif

$(OBJS): $(CFILES) $(HFILES)
	$(CC) $(CFLAGS) -c $^ -I$(INCDIR) $(LDFLAGS)

sockets: sockets.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -I$(INCDIR) $(LDFLAGS)

clean:
	$(CLEAN_CMD)

