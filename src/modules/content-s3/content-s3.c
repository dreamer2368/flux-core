/************************************************************\
 * Copyright 2020 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <jansson.h>
#include <flux/core.h>

#include "src/common/libutil/blobref.h"
#include "src/common/libutil/log.h"

#include "src/common/libcontent/content-util.h"

#include "src/common/libtomlc99/toml.h"
#include "src/common/libutil/tomltk.h"

#include "src/common/libyuarel/yuarel.h"

#include "s3.h"

struct content_s3 {
    flux_msg_handler_t **handlers;
    struct s3_config *cfg;
    flux_t *h;
    const char *hashfun;
};

static void s3_config_destroy (struct s3_config *ctx)
{
    if (ctx) {
        int saved_errno = errno;
        free (ctx->bucket);
        free (ctx->hostname);
        free (ctx->access_key);
        free (ctx->secret_key);
        free (ctx);
        errno = saved_errno;
    }
}

/* Destroy module context.
 */
static void content_s3_destroy (struct content_s3 *ctx)
{
    if (ctx) {
        int saved_errno = errno;
        flux_msg_handler_delvec (ctx->handlers);
        s3_config_destroy (ctx->cfg);
        free (ctx);
        errno = saved_errno;
    }

    s3_cleanup ();
}

static int parse_credentials (struct s3_config *cfg,
                              const char *cred_file,
                              char *errbuff,
                              int eb_size)
{
    struct tomltk_error toml_error;
    toml_table_t *tbl;
    const char *raw;
    char *access_key;
    char *secret_key;
    int saved_errno;

    if (!(tbl = tomltk_parse_file (cred_file, &toml_error))) {
        errno = EINVAL;
        snprintf (errbuff, eb_size, "toml parse failed: %s", toml_error.errbuf);
        goto error;
    }

    if (!(raw = toml_raw_in (tbl, "secret-access-key"))) {
        errno = EINVAL;
        snprintf (errbuff, eb_size, "failed to parse secret key");
        goto error;
    }

    if (toml_rtos (raw, &secret_key)) {
        errno = EINVAL;
        snprintf (errbuff, eb_size, "failed to parse secret key");
        goto error;
    }

    if (!(raw = toml_raw_in (tbl, "access-key-id"))) {
        free (secret_key);
        errno = EINVAL;
        snprintf (errbuff, eb_size, "failed to parse access key");
        goto error;
    }

    if (toml_rtos (raw, &access_key)) {
        free (secret_key);
        errno = EINVAL;
        snprintf (errbuff, eb_size, "failed to parse access key");
        goto error;
    }

    cfg->secret_key = secret_key;
    cfg->access_key = access_key;

    return 0;

error:
    saved_errno = errno;
    toml_free (tbl);
    errno = saved_errno;
    return -1;
}

static char *hostport (const char *host, int port)
{
    char *s;
    if (port == 0) {
        if (!(s = strdup (host)))
            return NULL;
    }
    else {
        if (asprintf (&s, "%s:%d", host, port) < 0)
            return NULL;
    }
    return s;
}

static struct s3_config *parse_config (const flux_conf_t *conf,
                                       char *errbuff,
                                       int eb_size)
{
    struct s3_config *cfg;
    flux_conf_error_t error;
    const char *uri = NULL;
    const char *bucket = NULL;
    const char *cred_file = NULL;
    int is_virtual_host = 0;
    struct yuarel yuri;
    char *cpy = NULL;
    int saved_errno;

    if (!(cfg = calloc (1, sizeof (*cfg))))
        return NULL;

    cfg->retries = 5;
    cfg->is_secure = 0;

    if (flux_conf_unpack (conf,
                          &error,
                          "{s:{s:s, s:s, s:s, s?:b !} }",
                          "content-s3",
                          "credential-file",
                          &cred_file,
                          "bucket",
                          &bucket,
                          "uri",
                          &uri,
                          "virtual-host-style",
                          &is_virtual_host) < 0) {
        snprintf(errbuff, eb_size, "%s", error.errbuf);
        goto error;
    }

    if (!(cpy = strdup (uri)))
        goto error;

    if (yuarel_parse (&yuri, cpy) < 0) {
        snprintf(errbuff, eb_size, "failed to parse uri");
        errno = EINVAL;
        goto error;
    }
    
    if (!(cfg->hostname = hostport (yuri.host, yuri.port))) {
        snprintf(errbuff, eb_size, "failed to form hostname");
        errno = ENOMEM;
        goto error;
    }

    if (!(cfg->bucket = strdup (bucket)))
        goto error;

    if (!strncmp (yuri.scheme, "https", 5))
        cfg->is_secure = 1;

    cfg->is_virtual_host = is_virtual_host;

    if (parse_credentials (cfg, cred_file, errbuff, eb_size))
        goto error;

    free (cpy);
    return cfg;

error:
    saved_errno = errno;
    free (cpy);
    errno = saved_errno;
    s3_config_destroy (cfg);
    return NULL;
}

/* Broker is sending us a new config object because 'flux config reload'
 * was run.  Parse it and respond with human readable errors.
 * If events are posted, block until they complete so that:
 * - any KVS commit errors are captured by 'flux config reload'
 * - tests can look for eventlog entry after running 'flux config reload'
 */
static void config_reload_cb (flux_t *h,
                              flux_msg_handler_t *mh,
                              const flux_msg_t *msg,
                              void *arg)
{
    struct s3_config *cfg;
    const flux_conf_t *conf;
    char errbuf[256];
    const char *errstr = NULL;

    if (flux_conf_reload_decode (msg, &conf) < 0)
        goto error;
    if (!(cfg = parse_config (conf, errbuf, sizeof (errbuf))) ){
        errstr = errbuf;
        goto error;
    }
    free (cfg);
    flux_log (h, LOG_WARNING, "config-reload: changes will not take effect until next flux restart");

    if (flux_set_conf (h, flux_conf_incref (conf)) < 0) {
        errstr = "error updating cached configuration";
        goto error;
    }
    if (flux_respond (h, msg, NULL) < 0)
        flux_log_error (h, "error responding to config-reload request");

    return;

error:
    if (flux_respond_error (h, msg, errno, errstr) < 0)
        flux_log_error (h, "error responding to config-reload request");
}

/* Handle a content-backing.load request from the rank 0 broker's
 * content-cache service.  The raw request payload is a blobref string,
 * including NULL terminator.  The raw response payload is the blob content.
 * These payloads are specified in RFC 10.
 */
static void load_cb (flux_t *h, flux_msg_handler_t *mh, const flux_msg_t *msg, void *arg)
{
    struct content_s3 *ctx = arg;
    const char *blobref;
    int blobref_size;
    void *data = NULL;
    size_t size;
    const char *errstr = NULL;

    if (flux_request_decode_raw (msg,
                                 NULL,
                                 (const void **)&blobref,
                                 &blobref_size) < 0)
        goto error;
    if (!blobref || blobref[blobref_size - 1] != '\0'
                 || blobref_validate (blobref) < 0) {
        errno = EPROTO;
        errstr = "invalid blobref";
        goto error;
    }
    if (s3_get (ctx->cfg, blobref, &data, &size, &errstr) < 0)
        goto error;
    if (flux_respond_raw (h, msg, data, size) < 0)
        flux_log_error (h, "error responding to load request");
    free (data);
    return;

error:
    if (flux_respond_error (h, msg, errno, errstr) < 0)
        flux_log_error (h, "error responding to load request");
    free (data);
}

/* Handle a content-backing.store request from the rank 0 broker's
 * content-cache service.  The raw request payload is the blob content.
 * The raw response payload is a blobref string including NULL terminator.
 * These payloads are specified in RFC 10.
 */
void store_cb (flux_t *h, flux_msg_handler_t *mh, const flux_msg_t *msg, void *arg)
{
    struct content_s3 *ctx = arg;
    const void *data;
    int size;
    char blobref[BLOBREF_MAX_STRING_SIZE];
    const char *errstr = NULL;

    if (flux_request_decode_raw (msg, NULL, &data, &size) < 0)
        goto error;
    if (blobref_hash (ctx->hashfun,
                      (uint8_t *)data,
                      size,
                      blobref,
                      sizeof (blobref)) < 0)
        goto error;
    if (s3_put (ctx->cfg, blobref, data, size, &errstr) < 0)
        goto error;
    if (flux_respond_raw (h, msg, blobref, strlen (blobref) + 1) < 0)
        flux_log_error (h, "error responding to store request");
    return;

error:
    if (flux_respond_error (h, msg, errno, errstr) < 0)
        flux_log_error (h, "error responding to store request");
}

/* Handle a kvs-checkpoint.get request from the rank 0 kvs module.
 * The KVS stores its last root reference here for restart purposes.
 *
 * N.B. filedb_get() calls read_all() which ensures that the returned buffer
 * is padded with an extra NULL not included in the returned length,
 * so it is safe to use the result as a string argument in flux_respond_pack().
 */
void checkpoint_get_cb (flux_t *h, flux_msg_handler_t *mh, const flux_msg_t *msg, void *arg)
{
    const char *errstr = NULL;
    struct content_s3 *ctx = arg;
    const char *key;
    void *data = NULL;
    char *dup = NULL;
    size_t size;

    if (flux_request_unpack (msg, NULL, "{s:s}", "key", &key) < 0)
        goto error;

    if (s3_get (ctx->cfg, key, &data, &size, &errstr) < 0)
        goto error;

    if (!(dup = strndup (data, size)))
        goto error;

    if (flux_respond_pack (h,
                           msg,
                           "{s:s}",
                           "value",
                           size > 0 ? dup : "", 0) < 0) {
        errno = EIO;
        flux_log_error (h, "error responding to kvs-checkpoint.get request (pack)");
    }
    free (data);
    free (dup);
    return;

error:
    if (flux_respond_error (h, msg, errno, errstr) < 0)
        flux_log_error (h, "error responding to kvs-checkpoint.get request");
    free (data);
    free (dup);
}

/* Handle a kvs-checkpoint.put request from the rank 0 kvs module.
 * The KVS stores its last root reference here for restart purposes.
 */
void checkpoint_put_cb (flux_t *h, flux_msg_handler_t *mh, const flux_msg_t *msg, void *arg)
{
    struct content_s3 *ctx = arg;
    const char *key;
    const char *value;
    const char *errstr = NULL;

    if (flux_request_unpack (msg,
                             NULL,
                             "{s:s s:s}",
                             "key",
                             &key,
                             "value",
                             &value) < 0)
        goto error;
    if (s3_put (ctx->cfg, key, value, strlen (value), &errstr) < 0)
        goto error;
    if (flux_respond (h, msg, NULL) < 0)
        flux_log_error (h, "error responding to kvs-checkpoint.put request (pack)");
    return;

error:
    if (flux_respond_error (h, msg, errno, errstr) < 0)
        flux_log_error (h, "error responding to kvs-checkpoint.put request");
}

/* Table of message handler callbacks registered below.
 * The topic strings in the table consist of <service name>.<method>.
 */
static const struct flux_msg_handler_spec htab[] = {
    { FLUX_MSGTYPE_REQUEST, "content-backing.load",    load_cb, 0 },
    { FLUX_MSGTYPE_REQUEST, "content-backing.store",   store_cb, 0 },
    { FLUX_MSGTYPE_REQUEST, "kvs-checkpoint.get", checkpoint_get_cb, 0 },
    { FLUX_MSGTYPE_REQUEST, "kvs-checkpoint.put", checkpoint_put_cb, 0 },
    { FLUX_MSGTYPE_REQUEST, "content-s3.config-reload", config_reload_cb, 0 },
    FLUX_MSGHANDLER_TABLE_END,
};

/* Create the s3 context, initalize the connection, and
 * create the working bucket
 */
static struct content_s3 *content_s3_create (flux_t *h)
{
    const char *errstr = NULL;
    char errbuff[256];
    struct content_s3 *ctx;

    if (!(ctx = calloc (1, sizeof (*ctx))))
        return NULL;
    ctx->h = h;

    if (!(ctx->hashfun = flux_attr_get (h, "content.hash"))) {
        flux_log_error (h, "content.hash");
        goto error;
    }

    if (!(ctx->cfg = parse_config (flux_get_conf (h), errbuff, sizeof (errbuff)))) {
        errstr = errbuff;
        flux_log (h, LOG_ERR, "content-s3 parsing config file: %s", errstr);
        goto error;
    }

    if (s3_init (ctx->cfg, &errstr) < 0) {
        flux_log_error (h, "content-s3 init");
        goto error;
    }

    if (s3_bucket_create (ctx->cfg, &errstr) < 0) {
        flux_log_error (h, "content-s3 create bucket");
        goto error;
    }

    if (flux_msg_handler_addvec (h, htab, ctx, &ctx->handlers) < 0)
        goto error;

    return ctx;

error:
    content_s3_destroy (ctx);
    return NULL;
}

int mod_main (flux_t *h, int argc, char **argv)
{
    struct content_s3 *ctx;
    int rc = -1;

    if (!(ctx = content_s3_create (h))) {
        flux_log_error (h, "content_s3_create failed");
        return -1;
    }
    if (content_register_backing_store (h, "content-s3") < 0)
        goto done;
    if (content_register_service (h, "content-backing") < 0)
        goto done;
    if (content_register_service (h, "kvs-checkpoint") < 0)
        goto done;
    if (flux_reactor_run (flux_get_reactor (h), 0) < 0) {
        flux_log_error (h, "flux_reactor_run");
        goto done;
    }
    if (content_unregister_backing_store (h) < 0)
        goto done;

    rc = 0;
done:
    content_s3_destroy (ctx);
    return rc;
}

MOD_NAME ("content-s3");

/*
 * vi:ts=4 sw=4 expandtab
 */
