CC = gcc
CFLAGS += -std=c99 -Wall -g
INCLUDES = -I ./includes/
LDFLAGS = -L ./src/
OPTFLAGS = -O3
LIBS = -lpthread

.PHONY: all clean cleanall
.SUFFIXES: .c .h

TARGETS = main

%: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $< $(LDFLAGS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c -o $@ $<

all: $(TARGETS)
