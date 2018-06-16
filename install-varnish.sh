#!/usr/bin/env bash

# Build and install varnish
./autogen.sh \
	&& ./configure --with-papi \
	&& make -j`nproc` \
	&& sudo make install \
	&& sudo ldconfig
