#
# Sample of a plugin built for the webc server.
#

PLUGIN_NAME=example_plugin

# If the location of the webc headers is not specified, it will use ../include
WEBC_INCLUDE_DIR?=../include

# If the location of the webc library file is not specified, it will use
# ../lib
WEBC_LIB_DIR?=../lib


CC=gcc
CFLAGS= -W -Wall -c -ggdb -I$(WEBC_INCLUDE_DIR) -fPIC
LD=gcc
LDFLAGS=-dynamic -shared -L$(WEBC_LIB_DIR) -lwebc

all:	$(PLUGIN_NAME).so

$(PLUGIN_NAME).so:	$(PLUGIN_NAME).o
	$(LD) -o $@ $< $(LDFLAGS)

%.o:	%.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -rfv $(PLUGIN_NAME).so $(PLUGIN_NAME).o


