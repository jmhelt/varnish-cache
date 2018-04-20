lib_LTLIBRARIES = libdrr.la

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_builddir)/include \
	-fPIC

AM_LDFLAGS = \
	$(AM_LT_LDFLAGS) \
	-static-libstdc++ \
	-shared

libdrr_la_SOURCES = \
	drr.c \
	uint32_void_tbl.cc \
	cuckoohash_config.hh \
	cuckoohash_map.hh \
	cuckoohash_util.hh \
	libcuckoo_bucket_container.hh

libdrr_la_LIBADD = $(AM_LT_LDFLAGS)

TESTS = drr_test

noinst_PROGRAMS = ${TESTS}

drr_test_SOURCES = drr_test.c
drr_test_LDADD = libdrr.la
drr_test_LDFLAGS = -pthread
