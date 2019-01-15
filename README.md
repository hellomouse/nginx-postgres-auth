# nginx-redis-auth

## Introduction

This module inserts a request handler into the nginx chain that will force
the user to provide a cookie set to a value that can be found in a redis database.

The code is commented, so it may be used as an example of a simple nginx module, too.

## y tho?

I ask myself that question every day, too.

We have a bunch of webapps (c9, netdata, jupyter, etc.) running on different
subdomains and want to give users access to them. Instead of forcing people to
login for each website, we want to set a single auth cookie for `*.example.com`
and have the user be automagically authenticated across all subdomains.

## Dependencies

nginx-redis-auth uses hiredis to interface with Redis. Please install
`libhiredis-dev` or similar to compile.

## Installation

### nginx
This module is not bundled with nginx. You will have to manually compile nginx
for this module to function:

1. Clone this repository
2. Download nginx source
3. Configure nginx to use this module (e.g. `./configure --add-module=/path/to/nginx-redis-auth`)
4. Build nginx as usual (likely `make`)
5. Install nginx to the correct place (likely `sudo make install`)

### tengine
This module is not bundled with tengine. You will have to manually compile the module
for this module to function. Luckily, tengine has a tool to make it rather easy.

After cloning, just run the dso_tool to install the module to the right place.

```
/path/to/dso_tool --add-module=/path/to/nginx-redis-auth
```

On my system, tengine is installed in `/opt/tengine/sbin`, so I would run `/opt/tengine/sbin/dso_tool`.

See [here](http://tengine.taobao.org/document/dso.html) for more information.

## Usage

This module registers a few directives:
 * `redis_auth`, an on/off flag that will enable or disable checking the redis database for authentication.
   To enable it globally, write `redis_auth on;` in the `http` block.
 * `redis_auth_backend` and `redis_auth_backend_port`, the string hostname and integer port, respectively,
   to connect to (redis server should be running on this port). Be wary of high latency as the requests
   are not cached.
 * `redis_auth_redirect`, a string that points to the url to redirect to.
   This URL can be relative (e.g. `/login`) or absolute (e.g. `https://example.com/`)
 * `redis_auth_cookie`, a string that tells the name of the cookie to check.
   I'm not exactly sure of the use of this one, but it defaults to `ra_cookie`.

To enable or disable per-location write the appropriate directive and argument.

An example where authentication is enabled for all endpoints except for `/login`
and `/login_other_auth` (which proxy passes to a server that could potentially
set cookies), and with an endpoint `/other_auth` that uses a different backend
server and cookie can be seen below.

```
http {
    server {
        listen 9005;
        root /var/www/html;
        redis_auth on;
        redis_auth_redirect "/login";

        location / {
            try_files $uri $uri/ =404;
        }

        location /login {
            redis_auth off;
            proxy_pass http://localhost:2020; # auth app running here
        }

        location /login_other_auth {
            redis_auth off;
            proxy_pass http://localhost:2021; # auth app running here
        }

        location /other_auth {
            redis_auth_backend_port 6380;
            redis_auth_cookie "ra_cookie2";
            redis_auth_redirect "/login_other_auth";
        }
    }
}
```

## Bugs and error reports

Bugtracker available here: https://github.com/hellomouse/nginx-redis-auth/issues

## See also

* Listing of 3rd-party modules: https://www.nginx.com/resources/wiki/modules/index.html
