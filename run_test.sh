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
working_directory $TESTROOT;
worker_rlimit_core 500M;
worker_processes 1;

events {
    worker_connections 768;
}

http {
    server {
        listen 9005;
        root $TESTROOT;
        redis_auth "http://localhost:9005/login/";

        location / {

        }

        location /login {
            redis_auth off;
            rewrite /login/(.*) /\$1  break;
            proxy_pass http://localhost:2971/;
            proxy_redirect     off;
        }

        location /diffcookie {
            redis_auth_cookie "cookie";
            redis_auth "helo world";
        }
    }
}


JIL

./nginx

cd $PATHROOT;

echo "run to stop:"
echo "$TESTROOT/nginx -s stop"

tail -f $TESTROOT/error.log
