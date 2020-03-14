# Minimal Makefile to compile the two (or more added by user) source files.
#

BINPROG=web.c

CC=gcc
CFLAGS= -W -Wall -Wconversion
LD=gcc
LDFLAGS= -o$(BINPROG)

debug:	all
debug:	CFLAGS+= -ggdb

release:	all
release:	CFLAGS+= -O3

$(BINPROG): $(OBS)
