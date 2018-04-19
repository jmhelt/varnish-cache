noinst_LIBRARIES = libdrr.a

AM_CPPFLAGS = \
	-I$(top_builddir)/include \
	-I$(top_srcdir)/include

libdrr_a_SOURCES = \
	drr.c

check_PROGRAMS = drr_test

TESTS = ${check_PROGRAMS}

drr_test_SOURCES = drr_test.c

drr_test_LDADD = \
	libdrr.a \
	$(top_builddir)/lib/libcuckoo/libcuckoo.la

