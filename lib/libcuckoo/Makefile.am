noinst_LIBRARIES = libcuckoo.a

libcuckoo_a_SOURCES = \
	int_str_table.cc \
	cuckoohash_config.hh \
	cuckoohash_map.hh \
	cuckoohash_util.hh \
	libcuckoo_bucket_container.hh

libcuckoo_a_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_builddir)/include

libcuckoo_a_LIBADD = $(AM_LT_LDFLAGS)
