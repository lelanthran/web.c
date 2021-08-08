#!/bin/bash

function die () {
   echo $@
   exit 127
}

function runcurl () {
   curl -c cookie-jar.txt -b cookie-jar.txt -d @posted-data -k $1
   export RETCODE=$?
   [ $RETCODE -ne 0 ] && die "url [$1] failed: $RETCODE"
   return $RETCODE
}

function auth_login () {
   echo '
   {
      "email":    "'"$1"'",
      "password": "'"$2"'"
   }
   ' >> posted-data
   runcurl localhost:5998/auth/login
}

auth_login root@localhost 123456
