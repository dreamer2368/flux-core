AM_CFLAGS = \
	$(WARNING_CFLAGS) \
	$(CODE_COVERAGE_CFLAGS)

AM_LDFLAGS = \
	$(CODE_COVERAGE_LIBS)

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src/include \
	-I$(top_builddir)/src/common/libflux \
	$(ZMQ_CFLAGS) $(JANSSON_CFLAGS)

fluxmod_LTLIBRARIES = job-manager.la

job_manager_la_SOURCES = \
	job-manager.c \
	job-manager.h \
	job.c \
	job.h \
	submit.c \
	submit.h \
	drain.c \
	drain.h \
	wait.c \
	wait.h \
	event.h \
	event.c \
	restart.h \
	restart.c \
	raise.h \
	raise.c \
	kill.h \
	kill.c \
	alloc.h \
	alloc.c \
	start.h \
	start.c \
	list.h \
	list.c \
	priority.h \
	priority.c \
	annotate.h \
	annotate.c \
	journal.h \
	journal.c

job_manager_la_LDFLAGS = $(fluxmod_ldflags) -module
job_manager_la_LIBADD = $(fluxmod_libadd) \
		    $(top_builddir)/src/common/libjob/libjob.la \
		    $(top_builddir)/src/common/libflux-internal.la \
		    $(top_builddir)/src/common/libflux-core.la \
		    $(top_builddir)/src/common/libflux-optparse.la \
		    $(ZMQ_LIBS)

TESTS = \
	test_job.t \
	test_list.t \
	test_raise.t \
	test_kill.t \
	test_restart.t \
	test_submit.t \
	test_annotate.t

test_ldadd = \
        $(top_builddir)/src/modules/job-manager/event.o \
        $(top_builddir)/src/modules/job-manager/job.o \
        $(top_builddir)/src/modules/job-manager/alloc.o \
        $(top_builddir)/src/modules/job-manager/start.o \
        $(top_builddir)/src/modules/job-manager/drain.o \
        $(top_builddir)/src/modules/job-manager/submit.o \
        $(top_builddir)/src/modules/job-manager/wait.o \
        $(top_builddir)/src/modules/job-manager/annotate.o \
        $(top_builddir)/src/modules/job-manager/journal.o \
	$(top_builddir)/src/common/libtap/libtap.la \
	$(top_builddir)/src/common/libjob/libjob.la \
	$(top_builddir)/src/common/libflux-internal.la \
	$(top_builddir)/src/common/libflux-core.la \
	$(ZMQ_LIBS) $(LIBPTHREAD) $(JANSSON_LIBS)

test_cppflags = \
	$(AM_CPPFLAGS)

test_ldflags = \
	-no-install

check_PROGRAMS = $(TESTS)

TEST_EXTENSIONS = .t
T_LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
       $(top_srcdir)/config/tap-driver.sh

test_job_t_SOURCES = test/job.c
test_job_t_CPPFLAGS = $(test_cppflags)
test_job_t_LDADD = \
        $(test_ldadd)
test_job_t_LDFLAGS = \
        $(test_ldflags)

test_list_t_SOURCES = test/list.c
test_list_t_CPPFLAGS = $(test_cppflags)
test_list_t_LDADD = \
        $(top_builddir)/src/modules/job-manager/list.o \
	$(test_ldadd)
test_list_t_LDFLAGS = \
        $(test_ldflags)

test_raise_t_SOURCES = test/raise.c
test_raise_t_CPPFLAGS = $(test_cppflags)
test_raise_t_LDADD = \
        $(top_builddir)/src/modules/job-manager/raise.o \
        $(test_ldadd)
test_raise_t_LDFLAGS = \
        $(test_ldflags)

test_kill_t_SOURCES = test/kill.c
test_kill_t_CPPFLAGS = $(test_cppflags)
test_kill_t_LDADD = \
        $(top_builddir)/src/modules/job-manager/kill.o \
        $(test_ldadd)
test_kill_t_LDFLAGS = \
        $(test_ldflags)

test_restart_t_SOURCES = test/restart.c
test_restart_t_CPPFLAGS = $(test_cppflags)
test_restart_t_LDADD = \
        $(top_builddir)/src/modules/job-manager/restart.o \
        $(test_ldadd)
test_restart_t_LDFLAGS = \
        $(test_ldflags)

test_submit_t_SOURCES = test/submit.c
test_submit_t_CPPFLAGS = $(test_cppflags)
test_submit_t_LDADD = \
        $(test_ldadd)
test_submit_t_LDFLAGS = \
        $(test_ldflags)

test_annotate_t_SOURCES = test/annotate.c
test_annotate_t_CPPFLAGS = $(test_cppflags)
test_annotate_t_LDADD = \
        $(test_ldadd)
test_annotate_t_LDFLAGS = \
        $(test_ldflags)
