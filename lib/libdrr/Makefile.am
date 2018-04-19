noinst_LIBRARIES = libdrr.a

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_builddir)/include \
	-pthread

AM_LDFLAGS = $(AM_LT_LDFLAGS) -lstdc++ -static

libdrr_a_SOURCES = \
	drr.c \
	uint32_void_tbl.cc \
	cuckoohash_config.hh \
	cuckoohash_map.hh \
	cuckoohash_util.hh \
	libcuckoo_bucket_container.hh

libdrr_a_LIBADD = $(AM_LT_LDFLAGS)

