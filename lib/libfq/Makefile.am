noinst_LIBRARIES = libfq.a

AM_CPPFLAGS = \
	-I$(top_builddir)/include \
	-I$(top_srcdir)/include

libfq_a_SOURCES = \
	rr.c

check_PROGRAMS = rr_test

TESTS = ${check_PROGRAMS}

rr_test_SOURCES = rr_test.c

rr_test_LDADD = \
	libfq.a \
	$(top_builddir)/lib/libcuckoo/libcuckoo.la

