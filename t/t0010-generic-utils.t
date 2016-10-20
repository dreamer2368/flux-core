#!/bin/sh
#

test_description='Test various flux utilities

Verify basic functionality of generic flux utils
'

. `dirname $0`/sharness.sh
SIZE=4
LASTRANK=$((SIZE-1))
test_under_flux ${SIZE} kvs

test_expect_success 'up: load live module' '
	flux module load -r all barrier &&
	flux module load -r all live barrier-count=${SIZE}
'
#
# Wait for all ranks to leave unknown state in live module:
# (i.e. conf.live.status has unknown == ""), since this is required
# for predictable results with flux-up:
test "$debug" = "t" && ARG="--verbose"
$SHARNESS_TEST_SRCDIR/scripts/kvs-watch-until.lua ${ARG} \
	--timeout=5 \
	conf.live.status 'v.unknown == ""' \
    || die "failed to wait on live module status"

test_expect_success 'up: flux up works' '
	flux up > output_up &&
	cat >expected_up <<-EOF &&
	ok:     [0-${LASTRANK}]
	slow:   
	fail:   
	unknown:
	EOF
	test_cmp expected_up output_up
'
test_expect_success 'up: flux up works with --comma' '
	flux up --comma > output_up_comma.0 &&
	EXPECTED="" &&
	for i in $(seq 0 ${LASTRANK}); do
		EXPECTED=${EXPECTED}${EXPECTED:+,}${i}
	done &&
	cat >expected_up_comma.0 <<-EOF &&
	ok:     ${EXPECTED}
	slow:   
	fail:   
	unknown:
	EOF
	test_cmp expected_up_comma.0 output_up_comma.0
'
test_expect_success 'up: flux up works with --newline' '
	flux up --newline > output_up_newline.0 &&
	cat >expected_up_newline.0 <<-EOF &&
	ok:     
	EOF
	for i in $(seq 0 ${LASTRANK}); do
		echo ${i} >> expected_up_newline.0
	done &&
	cat >>expected_up_newline.0 <<-EOF &&
	slow:   
	fail:   
	unknown:
	EOF
	test_cmp expected_up_newline.0 output_up_newline.0
'
test_expect_success 'up: flux up --up' '
	OUTPUT=$(flux up --up) &&
	test_debug say "got \"$OUTPUT\"" &&
	test "$OUTPUT" = "[0-${LASTRANK}]"
'
test_expect_success 'up: flux up --down' '
	OUTPUT=$(flux up --down) &&
	test_debug say "got \"$OUTPUT\"" &&
	test "$OUTPUT" = ""
'
test_expect_success 'up: flux up --up --comma' '
	flux up --up --comma > output_up_comma.1 &&
	EXPECTED="" &&
	for i in $(seq 0 ${LASTRANK}); do
		EXPECTED=${EXPECTED}${EXPECTED:+,}${i}
	done &&
	echo $EXPECTED > expected_up_comma.1
	test_cmp output_up_comma.1 expected_up_comma.1
'
test_expect_success 'up: flux up --down --comma' '
	OUTPUT=$(flux up --down --comma) &&
	test_debug say "got \"$OUTPUT\"" &&
	test "$OUTPUT" = ""
'
test_expect_success 'up: flux up --up --newline' '
	flux up --up --newline > output_up_newline.1 &&
	for i in $(seq 0 ${LASTRANK}); do
		echo ${i} >> expected_up_newline.1
	done &&
	test_cmp output_up_newline.1 expected_up_newline.1
'
test_expect_success 'up: flux up --down --newline' '
	OUTPUT=$(flux up --down --newline) &&
	test_debug say "got \"$OUTPUT\"" &&
	test "$OUTPUT" = ""
'
test_expect_success 'event: can publish' '
	run_timeout 5 \
	  $SHARNESS_TEST_SRCDIR/scripts/event-trace.lua \
	    testcase testcase.foo \
	    flux event pub testcase.foo > output_event_pub &&
	cat >expected_event_pub <<-EOF &&
	testcase.foo
	EOF
	test_cmp expected_event_pub output_event_pub
'
test_expect_success 'event: can subscribe' '
	flux event sub --count=1 hb >output_event_sub &&
	grep "^hb" output_event_sub
'
# for now we just ensure snoop basically works
test_expect_success 'snoop: produces output (XXX needs fixing)' '
	flux snoop -c 1 --verbose >output_snoop 2>&1 &
	test_expect_code 0 wait &&
	test -s output_snoop &&
	head output_snoop | grep "^flux-snoop: connecting to"
'

test_expect_success 'version: reports an expected string' '
        set -x
	flux version | grep -q "flux-core-[0-9]+\.[0-9]+\.[0-9]"
	set +x
'
test_done
