# Minimal Makefile to compile the two (or more added by user) source files.
# The entire project is compiled and linked into a single executable file.

# TODO: At some point we want the web-add file to be linked into a .so file so
# that multiple instances of the program don't go hogging the memory just
# because the user has a large set of additions in web-add.

BINPROG=web-c

# If you want any other sources files linked to this web-server, list them
# here, except replace the file extension ".c" with ".o".
OBS=\
	web-main.o \
	web-add.o

# If you want any headers to be considered part of the dependencies, put them
# here (don't change the extension.
HEADERS=\
	web-main.h \
	web-add.h

CC=gcc
CFLAGS= -W -Wall -Wconversion -c
LD=gcc
LDFLAGS= -o$(BINPROG)

debug:	all
debug:	CFLAGS+= -ggdb

release:	all
release:	CFLAGS+= -O3

all: $(BINPROG)

$(BINPROG): $(OBS)
	$(LD) -o $@ $^ $(LDFLAGS)

%.o:	%.c $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rfv $(OBS) $(BINPROG)



