#!/bin/sh

if [ -z "$1" ]; then
   echo You must specify the program to run as a daemon
   exit 127;
fi

until $1; do
    echo "Server '$1' crashed with exit code $?.  Respawning..." >&2
    sleep 1
done

