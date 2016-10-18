#include <czmq.h>
#include "message.h"
#include "event.h"
#include "src/common/libtap/tap.h"

int main (int argc, char *argv[])
{
    flux_msg_t *msg;
    const char *topic, *s;
    const char *json_str = "{\"a\":42}";

    plan (8);

    /* no topic is an error */
    errno = 0;
    ok ((msg = flux_event_encode (NULL, json_str)) == NULL && errno == EINVAL,
        "flux_event_encode returns EINVAL with no topic string");

    /* without payload */
    ok ((msg = flux_event_encode ("foo.bar", NULL)) != NULL,
        "flux_event_encode works with NULL payload");

    topic = NULL;
    ok (flux_event_decode (msg, &topic, NULL) == 0
        && topic != NULL && !strcmp (topic, "foo.bar"),
        "flux_event_decode returns encoded topic");
    ok (flux_event_decode (msg, NULL, NULL) == 0,
        "flux_event_decode topic is optional");
    errno = 0;
    ok (flux_event_decode (msg, NULL, &s) < 0 && errno == EPROTO,
        "flux_event_decode returns EPROTO when expected payload is missing");
    flux_msg_destroy(msg);

    /* with payload */
    ok ((msg = flux_event_encode ("foo.bar", json_str)) != NULL,
        "flux_event_encode works with payload");

    s = NULL;
    ok (flux_event_decode (msg, NULL, &s) == 0
        && s != NULL && !strcmp (s, json_str),
        "flux_event_decode returns encoded payload");
    errno = 0;
    ok (flux_event_decode (msg, NULL, NULL) < 0 && errno == EPROTO,
        "flux_event_decode returns EPROTO when payload is unexpected");
    flux_msg_destroy (msg);

    done_testing();
    return (0);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

