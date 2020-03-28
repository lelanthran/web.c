#!/bin/sh

until $1; do
    echo "Server '$1' crashed with exit code $?.  Respawning..." >&2
    sleep 1
done

