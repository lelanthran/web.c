# Minimal Makefile to compile the two (or more added by user) source files.
# The entire project is compiled and linked into a single executable file.

# TODO: At some point we want the web-add file to be linked into a .so file so
# that multiple instances of the program don't go hogging the memory just
# because the user has a large set of additions in web-add.

# USER SETS THIS VARIABLE
# You can use this to keep multiple versions running in the same directory.
# Each executable will only use its own library. After the three-number tuple
# in the form of X.Y.Z, you can add any suffixes. In this example I use "-rc1"
# to denote "release candidate 1".
VERSION=0.0.1-rc

# ###############################################################

# USER SETS THIS VARIABLE
# If you want any other sources files linked to this web-server, list them
# here, except replace the file extension ".c" with ".o".
OBS=\
	util.o \
	resource.o \
	handler.o \
	web-add.o

# USER SETS THIS VARIABLE
# If you need to add your header directories, this is where it must be done
INCLUDE_PATHS=\
	-I web.c

# USER SETS THIS VARIABLE
# If you want any headers to be considered part of the dependencies, put them
# here (don't change the extension).
HEADERS=\
	web.c/web-main.h \
	web.c/util.h \
	web.c/resource.h \
	web.c/handler.h \
	web.c/config.h \
	web.c/web-add.h

# ###############################################################

# This is the main program, you won't need to change this.
MAIN_OB=web-main.o

# The executable, linked to shared object library. You won't need to change
# this
BINPROG_SHARED=webc-shared

# The executable, linked to the static library. You won't need to change this.
BINPROG_STATIC=webc-static

# The shared object library. You won't need to change this.
LIB_SHARED=webc

# The static library. You won't need to change this.
LIB_STATIC=webc

SO_VER=$(shell echo $(VERSION) | cut -f 1 -d .)

# ###############################################################

CC=gcc
CFLAGS=  -c -fPIC \
	-W -Wall \
	-Wnull-dereference \
	-Wjump-misses-init \
	-Wformat=2
LD=gcc
LDFLAGS= -lpthread

all: $(BINPROG_SHARED)-$(VERSION) $(BINPROG_STATIC)-$(VERSION)

debug:	all
debug:	CFLAGS+= -ggdb -DDEBUG=1

release:	all
release:	CFLAGS+= -O3

$(BINPROG_SHARED)-$(VERSION):	$(MAIN_OB) lib$(LIB_SHARED).so.$(VERSION)
	$(LD) $(MAIN_OB) -L. -l$(LIB_SHARED) $(LDFLAGS) -o $@

$(BINPROG_STATIC)-$(VERSION):	$(MAIN_OB) lib$(LIB_STATIC)-$(VERSION).a
	$(LD) $(MAIN_OB) lib$(LIB_STATIC)-$(VERSION).a $(LDFLAGS) -o $@

lib$(LIB_SHARED).so.$(VERSION):	$(OBS)
	$(LD) -shared $(OBS) -Wl,-soname,lib$(LIB_SHARED).so.$(SO_VER) $(LDFLAGS) -o $@
	rm -rf lib$(LIB_SHARED).so.$(SO_VER)
	ln -s $@ lib$(LIB_SHARED).so.$(SO_VER)
	rm -rf lib$(LIB_SHARED).so
	ln -s $@ lib$(LIB_SHARED).so

lib$(LIB_STATIC)-$(VERSION).a:	$(OBS)
	ar rc $@ $(OBS)

%.o:	web.c/%.c $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rfv $(OBS) $(MAIN_OB)\
		            $(BINPROG_SHARED)-$(VERSION) \
	               $(BINPROG_STATIC)-$(VERSION) \
	               lib$(LIB_SHARED).so.$(VERSION)\
	               lib$(LIB_SHARED).so.$(SO_VER)\
	               lib$(LIB_SHARED).so\
	               lib$(LIB_STATIC)-$(VERSION).a



