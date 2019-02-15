/* postgres */
#include <postgresql/libpq-fe.h>

/* nginx */
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

/* config options */
typedef struct {
    ngx_flag_t  enable;
    ngx_str_t   backend_opts;
    ngx_str_t   table_name;
    ngx_str_t   redirect;
    ngx_str_t   cookie;
} ngx_http_postgres_auth_conf_t;

/* function definitions for later on */
static void *ngx_http_postgres_auth_create_conf(ngx_conf_t *cf);
static char *ngx_http_postgres_auth_merge_conf(ngx_conf_t *cf, void *parent, void *child);
static ngx_int_t ngx_http_postgres_auth_init(ngx_conf_t *cf);

/* directives provided by this module */
static ngx_command_t ngx_http_postgres_auth_commands[] = {
    {
        /* directive string - enable or not */
        ngx_string("postgres_auth"),
        /* can be used anywhere, and has a single flag of enable/disable */
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
        /* simple set on/off */
        ngx_conf_set_flag_slot,
        /* flags apply to each location */
        NGX_HTTP_LOC_CONF_OFFSET,
        /* memory address offset to read/write from/to */
        offsetof(ngx_http_postgres_auth_conf_t, enable),
        NULL
    },
    {
        /* directive string - backend postgres server options */
        ngx_string("postgres_auth_backend_opts"),
        /* can be used anywhere, and takes in a single argument */
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        /* simple set string to value */
        ngx_conf_set_str_slot,
        /* flags apply to each location */
        NGX_HTTP_LOC_CONF_OFFSET,
        /* memory address offset to read/write from/to */
        offsetof(ngx_http_postgres_auth_conf_t, backend_opts),
        NULL
    },
    {
        /* directive string - table name in backend postgres server */
        ngx_string("postgres_auth_backend_table_name"),
        /* can be used anywhere, and takes in a single argument */
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        /* simple set string to value */
        ngx_conf_set_str_slot,
        /* flags apply to each location */
        NGX_HTTP_LOC_CONF_OFFSET,
        /* memory address offset to read/write from/to */
        offsetof(ngx_http_postgres_auth_conf_t, table_name),
        NULL
    },
    {
        /* directive string - url to redirect to for logging in */
        ngx_string("postgres_auth_redirect"),
        /* can be used anywhere, and takes in a single argument */
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        /* simple set string to value */
        ngx_conf_set_str_slot,
        /* flags apply to each location */
        NGX_HTTP_LOC_CONF_OFFSET,
        /* memory address offset to read/write from/to */
        offsetof(ngx_http_postgres_auth_conf_t, redirect),
        NULL
    },
    {
        /* directive string - name of the cookie */
        ngx_string("postgres_auth_cookie"),
        /* can be used anywhere, and takes in a single argument */
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        /* simple set string to value */
        ngx_conf_set_str_slot,
        /* flags apply to each location */
        NGX_HTTP_LOC_CONF_OFFSET,
        /* memory address offset to read/write from/to */
        offsetof(ngx_http_postgres_auth_conf_t, cookie),
        NULL
    },
    /* no more directives */
    ngx_null_command
};

/* set up contexts and callbacks */
static ngx_http_module_t ngx_http_postgres_auth_module_ctx = {
    /* preconfiguration */
    NULL,
    /* postconfiguration */
    ngx_http_postgres_auth_init,
    /* create main configuration */
    NULL,
    /* init main configuration */
    NULL,
    /* create server configuration */
    NULL,
    /* merge server configuration */
    NULL,
    /* create location configuration */
    ngx_http_postgres_auth_create_conf,
    /* merge location configuration */
    ngx_http_postgres_auth_merge_conf
};

/* main module exported to nginx */
ngx_module_t ngx_http_postgres_auth_module = {
    /* mandatory padding */
    NGX_MODULE_V1,
    /* context */
    &ngx_http_postgres_auth_module_ctx,
    /* directives used */
    ngx_http_postgres_auth_commands,
    /* module type */
    NGX_HTTP_MODULE,
    /* init master callback */
    NULL,
    /* init module callback */
    NULL,
    /* init process callback */
    NULL,
    /* init thread callback */
    NULL,
    /* exit thread callback */
    NULL,
    /* exit process callback */
    NULL,
    /* exit master callback */
    NULL,
    /* mandatory padding */
    NGX_MODULE_V1_PADDING
};

/* hook for requests - handler */
static ngx_int_t ngx_http_postgres_auth_handler(ngx_http_request_t *r) {
    ngx_http_postgres_auth_conf_t   *pacf;
    ngx_str_t                       val;
    ngx_int_t                       n;
    ngx_table_elt_t                 *location;
    PGconn                          *c = NULL;
    PGresult                        *res = NULL;
    const char                      *p[2];
    /* TODO: fix X-Auth-Username header */
    /* char                            *z; */

    /* get this module's configuration (scoped to location) */
    pacf = ngx_http_get_module_loc_conf(r, ngx_http_postgres_auth_module);

    /* check if enabled */
    if (!pacf->enable) {
        /* skip */
        return NGX_OK;
    }

    /* try to read the cookie */
    n = ngx_http_parse_multi_header_lines(&r->headers_in.cookies, &pacf->cookie, &val);
    if (n == NGX_DECLINED) {
        /* no cookie found */
        goto send_redir;
    }

    /* it is probably terribly inefficient to do this way... */

    /* connect to server */
    c = PQconnectdb((const char *)pacf->backend_opts.data);
    if (c == NULL || PQstatus(c) != CONNECTION_OK) {
        /* error connecting to postgres server */
        ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0,
            "errror connecting to backend server %s: %s", pacf->backend_opts.data, PQerrorMessage(c));
        n = NGX_HTTP_INTERNAL_SERVER_ERROR;
        goto end;
    }

    /* setup params for query */
    p[1] = (const char *)pacf->table_name.data;
    p[0] = (const char *)val.data;

    /* query for key */
    res = PQexecParams(c,
        "SELECT username FROM sessions WHERE session_key=$1 AND expiry > now()",
        1, /* 1 params */
        NULL, /* let backend guess param types */
        p, /* array of params */
        NULL, /* all text params */
        NULL, /* all text params */
        1
    );
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        /* error querying postgres server */
        ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0,
            "errror querying backend server %s: %s", pacf->backend_opts.data, PQerrorMessage(c));
        n = NGX_HTTP_INTERNAL_SERVER_ERROR;
        goto end;
    } else if (PQntuples(res) >= 1) {
        /* access granted! */
#if 0
        /* push the X-Auth-Username header to input headers */
        location = ngx_list_push(&r->headers_in.headers);
        if (location == NULL) {
            ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0,
                "errror adding X-Auth-Username header");
            goto skiphdr;
        }

        /* get the username */
        n = PQfnumber(res, "username");
        z = PQgetvalue(res, 0, n);

        /* need to make a copy of the string since the request freeing will kill it */
        val.data = (unsigned char *)z;
        val.len = strlen(z);

        /* add it to the headers */
        location->hash = 1;
        location->key.len = sizeof("X-Auth-Username") - 1;
        location->key.data = (u_char *) "X-Auth-Username";
        location->value.len = val.len;
        location->value.data = ngx_pstrdup(r->pool, &val);
skiphdr:
#endif
        /* pass to next handler */
        n = NGX_OK;
        goto end;
    }

    /* default to send a redir */

send_redir:
    /* no/invalid auth cookie, so send a redirect to a place to log in */
    if (r->http_version < NGX_HTTP_VERSION_11) {
        /* send 302 for clients that don't support 303 */
        r->headers_out.status = NGX_HTTP_MOVED_TEMPORARILY;
        n = NGX_HTTP_MOVED_TEMPORARILY;
    } else {
        /* send 303 for clients that do */
        r->headers_out.status = NGX_HTTP_TEMPORARY_REDIRECT;
        n = NGX_HTTP_TEMPORARY_REDIRECT;
    }

    /* push the location header */
    location = ngx_list_push(&r->headers_out.headers);
    if (location == NULL) {
        n = NGX_HTTP_INTERNAL_SERVER_ERROR;
        goto end;
    }

    location->hash = 1;
    location->key.len = sizeof("Location") - 1;
    location->key.data = (u_char *) "Location";
    location->value.len = pacf->redirect.len;
    location->value.data = pacf->redirect.data;

    r->headers_out.location = location;

    ngx_http_clear_accept_ranges(r);
    ngx_http_clear_last_modified(r);
    ngx_http_clear_content_length(r);
    ngx_http_clear_etag(r);

end:
    if (res) PQclear(res);
    if (c) PQfinish(c);

    return n;
}

/* config initalization function */
static void *ngx_http_postgres_auth_create_conf(ngx_conf_t *cf) {
    ngx_http_postgres_auth_conf_t *mod_conf;

    /* allocate memory for config struct */
    mod_conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_postgres_auth_conf_t));
    if (mod_conf == NULL) {
        /* OOM */
        return NULL;
    }

    /* unknown value right now */
    mod_conf->enable = NGX_CONF_UNSET;

    /* return pointer */
    return mod_conf;
}

/* let child blocks set postgres_auth even if the parent has it set */
static char *ngx_http_postgres_auth_merge_conf(ngx_conf_t *cf, void *parent, void *child) {
    ngx_http_postgres_auth_conf_t *prev = (ngx_http_postgres_auth_conf_t *)parent;
    ngx_http_postgres_auth_conf_t *curr = (ngx_http_postgres_auth_conf_t *)child;

    /* child takes precendence over parent */
    ngx_conf_merge_value(curr->enable, prev->enable, 0);
    ngx_conf_merge_str_value(curr->backend_opts, prev->backend_opts, "dbname = auth");
    ngx_conf_merge_str_value(curr->table_name, prev->table_name, "sessions");
    ngx_conf_merge_str_value(curr->redirect, prev->redirect, "/auth");
    ngx_conf_merge_str_value(curr->cookie, prev->cookie, "ra_cookie");

    /* TODO: parameter validation here... */
    /* ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "ahh");  */

    return NGX_CONF_OK;
}

/* initialization callback */
static ngx_int_t ngx_http_postgres_auth_init(ngx_conf_t *cf) {
    ngx_http_handler_pt         *h;
    ngx_http_core_main_conf_t   *cmcf;

    /* get the main config */
    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    /* register ourselves as an access-phase handler */
    h = ngx_array_push(&cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    /* set the handler */
    *h = ngx_http_postgres_auth_handler;

    return NGX_OK;
}
