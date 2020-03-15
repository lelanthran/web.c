# Minimal Makefile to compile the two (or more added by user) source files.
# The entire project is compiled and linked into a single executable file.

# TODO: At some point we want the web-add file to be linked into a .so file so
# that multiple instances of the program don't go hogging the memory just
# because the user has a large set of additions in web-add.

# You can use this to keep multiple versions running in the same directory.
# Each executable will only use its own library. After the three-number tuple
# in the form of X.Y.Z, you can add any suffixes. In this example I use "-rc1"
# to denote "release candidate 1".
VERSION=0.0.1-rc

# ###############################################################

# The executable, linked to shared object library
BINPROG_SHARED=webc-shared-$(VERSION)

# The executable, linked to the static library
BINPROG_STATIC=webc-static-$(VERSION)

# The shared object library
LIB_SHARED=webc-$(VERSION)

# The static library
LIB_STATIC=webc-$(VERSION)

# If you want any other sources files linked to this web-server, list them
# here, except replace the file extension ".c" with ".o".
OBS=web-add.o

# This is the main program, you won't need to change this.
MAIN_OB=web-main.o

# If you want any headers to be considered part of the dependencies, put them
# here (don't change the extension).
HEADERS=\
	web.c/web-main.h \
	web.c/web-add.h

# If you need to add your header directories, this is where it must be done
INCLUDE_PATHS=\
	-I web.c

# ###############################################################

CC=gcc
CFLAGS= -W -Wall -Wconversion -c -fPIC
LD=gcc
LDFLAGS=

all: $(BINPROG_SHARED) $(BINPROG_STATIC)

debug:	all
debug:	CFLAGS+= -ggdb -DDEBUG=1

release:	all
release:	CFLAGS+= -O3

$(BINPROG_SHARED):	$(MAIN_OB) lib$(LIB_SHARED).so
	$(LD) $(MAIN_OB) -L. -l$(LIB_SHARED) $(LDFLAGS) -o $@

$(BINPROG_STATIC):	$(MAIN_OB) lib$(LIB_STATIC).a
	$(LD) $(MAIN_OB) lib$(LIB_SHARED).a $(LDFLAGS) -o $@

lib$(LIB_SHARED).so:	$(OBS)
	$(LD) -shared $(OBS) $(LDFLAGS) -o $@

lib$(LIB_SHARED).a:	$(OBS)
	ar rc $@ $(OBS)

%.o:	web.c/%.c $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rfv $(OBS) $(MAIN_OB)\
		            $(BINPROG_SHARED) \
	               $(BINPROG_STATIC) \
	               lib$(LIB_SHARED).so\
	               lib$(LIB_STATIC).a



