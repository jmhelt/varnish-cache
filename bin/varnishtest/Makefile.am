#

VTC_LOG_COMPILER = ./varnishtest -v -i
TEST_EXTENSIONS = .vtc
TESTS = @VTC_TESTS@

# Make sure we run check-local first
check: check-am check-local
check-am: check-local
# See if list of checks have changed, recheck
check-local:
	@mkdir -p tests ; \
	LC_ALL=C; \
	if [ "$$(cd $(srcdir) && echo tests/*.vtc)" != "@VTC_TESTS@" ]; then \
		cd $(top_builddir) && ./config.status --recheck ; \
	fi

DISTCLEANFILES = _.ok

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_builddir)/include \
	-I$(top_srcdir)/lib/libvgz

bin_PROGRAMS =	varnishtest

varnishtest_SOURCES = \
		hpack.h \
		programs.h \
		cmds.h \
		vmods.h \
		vtc.h \
		vtc.c \
		vtc_barrier.c \
		vtc_client.c \
		vtc_haproxy.c \
		vtc_h2_dectbl.h \
		vtc_h2_enctbl.h \
		vtc_h2_hpack.c \
		vtc_h2_priv.h \
		vtc_h2_stattbl.h \
		vtc_h2_tbl.c \
		vtc_http.c \
		vtc_http.h \
		vtc_http2.c \
		vtc_log.c \
		vtc_logexp.c \
		vtc_misc.c \
		vtc_main.c \
		vtc_process.c \
		vtc_proxy.c \
		vtc_server.c \
		vtc_subr.c \
		vtc_term.c \
		vtc_varnish.c

varnishtest_LDADD = \
		$(top_builddir)/lib/libvarnish/libvarnish.a \
		$(top_builddir)/lib/libvarnishapi/libvarnishapi.la \
		$(top_builddir)/lib/libvgz/libvgz.a \
		@SAN_LDFLAGS@ \
		@PCRE_LIBS@ \
		${PTHREAD_LIBS} ${NET_LIBS} ${LIBM}

varnishtest_CFLAGS = \
		@SAN_CFLAGS@ \
		-DTOP_BUILDDIR='"${top_builddir}"'

EXTRA_DIST = $(top_srcdir)/bin/varnishtest/tests/*.vtc \
	$(top_srcdir)/bin/varnishtest/tests/README
