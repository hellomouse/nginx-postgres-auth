#!/bin/bash


PATHROOT=$(pwd)
TESTROOT=$PATHROOT/nginx-tests
mkdir -p $PATHROOT/nginx-tests

$TESTROOT/nginx -s stop

set -e

# please have a working nginx in nginx/

cd nginx
[ $PATHROOT/ngx_http_redis_auth_module.c -ot $TESTROOT/nginx ] &&
    ./configure \
    --add-module=$PATHROOT \
    --prefix=$PATHROOT/nginx-tests \
    --conf-path=$TESTROOT/test.conf \
    --http-log-path=$TESTROOT/access.log \
    --error-log-path=$TESTROOT/error.log \
    --with-http_addition_module \
    --with-cc-opt='-g -O0' \
    #--with-debug

make -j 4

cp objs/nginx $TESTROOT

cd ../nginx-tests

cat <<JIL >test.conf
error_log $TESTROOT/error.log debug;
worker_processes 1;

events {
    worker_connections 768;
}

http {
    server {
        listen 9005;
        root $TESTROOT;
        redis_auth on;
        redis_auth_redirect "/login";

        location / {

        }

        location /login {
            redis_auth off;
        }

        location /diffcookie {
            redis_auth_cookie "cookie";
        }
    }
}


JIL

./nginx

cd $PATHROOT;

echo "run to stop:"
echo "$TESTROOT/nginx -s stop"

tail -f $TESTROOT/error.log
