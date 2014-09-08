#ifndef FLUX_H
#define FLUX_H

/* The flux_t handle and its operations works in multiple environments.
 * Not all environments will implement all operations.
 * If an unimplemented function is called, it will return an error
 * with errno == ENOSYS.
 */

typedef void (*FluxFreeFn)(void *arg);

typedef struct flux_handle_struct *flux_t;

#include "reactor.h"
#include "security.h"
#include "reduce.h"
#include "message.h"
#include "handle.h"
#include "panic.h"
#include "event.h"
#include "request.h"

#include "api.h"
#include "kvs.h"
#include "live.h"
#include "modctl.h"

/* Flags for handle creation and flux_flags_set()/flux_flags_unset.
 */
enum {
    FLUX_FLAGS_TRACE = 1,   /* print 0MQ messages sent over the flux_t */
                            /*   handle on stdout. */
};

/* A mechanism is provide for other modules to attach auxiliary state to
 * the flux_t handle by name.  The FluxFreeFn, if non-NULL, will be called
 * to destroy this state when the handle is destroyed.
 */
void *flux_aux_get (flux_t h, const char *name);
void flux_aux_set (flux_t h, const char *name, void *aux, FluxFreeFn destroy);

/* Set/clear FLUX_FLAGS_* on a flux_t handle.
 * Flags can also be set when the handle is created, e.g. in cmb_init_full().
 */
void flux_flags_set (flux_t h, int flags);
void flux_flags_unset (flux_t h, int flags);

/* Send/receive requests and responses.
 * - flux_request_sendmsg() expects a route delimiter (request envelope)
 * - flux_request_sendmsg()/flux_response_sendmsg() free '*zmsg' and set it
 *   to NULL on success.
 * - int-returning functions return 0 on success, -1 on failure with errno set.
 * - pointer-returning functions return NULL on failure with errno set.
 */
int flux_request_sendmsg (flux_t h, zmsg_t **zmsg);
zmsg_t *flux_request_recvmsg (flux_t h, bool nb);
int flux_response_sendmsg (flux_t h, zmsg_t **zmsg);
zmsg_t *flux_response_recvmsg (flux_t h, bool nb);
int flux_response_putmsg (flux_t h, zmsg_t **zmsg);
int flux_request_send (flux_t h, json_object *request, const char *fmt, ...);
zmsg_t *flux_response_matched_recvmsg (flux_t h, const char *match, bool nb);
json_object *flux_rpc (flux_t h, json_object *in, const char *fmt, ...);
int flux_response_recv (flux_t h, json_object **respp, char **tagp, bool nb);
int flux_respond (flux_t h, zmsg_t **request, json_object *response);
int flux_respond_errnum (flux_t h, zmsg_t **request, int errnum);

/* Send a request to a particular cmb rank
 *   otherwise identical to flux_request_sendmsg()/flux_request_send()
 */
int flux_rank_request_sendmsg (flux_t h, int rank, zmsg_t **zmsg);
int flux_rank_request_send (flux_t h, int rank,
                            json_object *request, const char *fmt, ...);
json_object *flux_rank_rpc (flux_t h, int rank,
                            json_object *in, const char *fmt, ...);

/* Send/receive events
 * - an event consists of a tag frame and an optional JSON frame
 * - flux_event_sendmsg() frees '*zmsg' and sets it to NULL on success.
 * - int-returning functions return 0 on success, -1 on failure with errno set.
 * - pointer-returning functions return NULL on failure with errno set.
 * - topics are period-delimited strings following 0MQ subscription semantics
 */
int flux_event_sendmsg (flux_t h, zmsg_t **zmsg);
zmsg_t *flux_event_recvmsg (flux_t h, bool nonblock);
int flux_event_send (flux_t h, json_object *request, const char *fmt, ...);
int flux_event_recv (flux_t h, json_object **respp, char **tagp, bool nb);
int flux_event_subscribe (flux_t h, const char *topic);
int flux_event_unsubscribe (flux_t h, const char *topic);

/* Get information about this cmbd instance's position in the flux comms
 * session.
 */
int flux_info (flux_t h, int *rankp, int *sizep, bool *treerootp);
int flux_rank (flux_t h);
int flux_size (flux_t h);
bool flux_treeroot (flux_t h);

/* Manipulate comms modules.
 * Use rank=-1 for local.
 */
enum {
    FLUX_MOD_FLAGS_MANAGED = 1,
};
int flux_rmmod (flux_t h, int rank, const char *name, int flags);
json_object *flux_lsmod (flux_t h, int rank);
int flux_insmod (flux_t h, int rank, const char *path, int flags,
                 json_object *args);

json_object *flux_lspeer (flux_t h, int rank);

/* Accessor for zeromq context.
 * N.B. The zctx_t is thread-safe but zeromq sockets, and therefore
 * flux_t handle operations are not.
 */
zctx_t *flux_get_zctx (flux_t h);

/* Accessor for security context.  Same comments on thread safety apply.
 */
flux_sec_t flux_get_sec (flux_t h);

/* Interface for logging via the comms' reduction network.
 */
void flux_log_set_facility (flux_t h, const char *facility);
int flux_vlog (flux_t h, int lev, const char *fmt, va_list ap);
int flux_log (flux_t h, int lev, const char *fmt, ...)
            __attribute__ ((format (printf, 3, 4)));

int flux_log_subscribe (flux_t h, int lev, const char *sub);
int flux_log_unsubscribe (flux_t h, const char *sub);
int flux_log_dump (flux_t h, int lev, const char *fac);

char *flux_log_decode (zmsg_t *zmsg, int *lp, char **fp, int *cp,
                    struct timeval *tvp, char **sp);

/* flux_getattr is used to read misc. attributes internal to the cmbd.
 * The caller must dispose of the returned string with free ().
 * The following attributes are valid:
 *    cmbd-snoop-uri   The name of the socket to be used by flux-snoop.
 *    cmbd-parent-uri  The name of parent socket.
 *    cmbd-request-uri The name of request socket.
 */
char *flux_getattr (flux_t h, int rank, const char *name);

/* Reparenting functions.
 */
int flux_reparent (flux_t h, int rank, const char *uri);
int flux_failover (flux_t h, int rank);

/* Comms modules must define  MOD_NAME and mod_main().
 */
typedef int (mod_main_f)(flux_t h, zhash_t *args);
extern mod_main_f mod_main;
#define MOD_NAME(x) const char *mod_name = x


#endif /* !FLUX_H */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
