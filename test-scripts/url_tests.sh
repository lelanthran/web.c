#!/bin/bash

set -e

pushd $PWD
cd test-scripts

trap 'catch' ERR

catch () {
   popd
}

function display_file () {
   cat $2 | while read LINE; do
      echo $1 $LINE
   done
}

call_curl () {
   echo "Cookie=[$HTTP_COOKIE]"

   trap 'catch' ERR

   echo $4 > tmp.input
   curl -X POST 'http://localhost:5998'/$1 \
      -H "$2" \
      -d @tmp.input\
      -o $3.curl

   if [ "$?" -ne 0 ]; then
      echo "Error calling '$1', executable returned: "
      display_file "✘" $3.curl
      exit 127
   else
      echo "Success calling '$1', executable returned: "
      display_file "✔" $3.curl
   fi
}

call_curl "" "" webroot ""
call_curl "/" "" webroot-slash ""

echo "do 'git diff test-scripts' to ensure that all tests passed"

popd

