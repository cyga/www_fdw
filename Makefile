# contrib/www_fdw/Makefile

LIBJSON = libjson-0.8
MODULE_big = www_fdw
OBJS	= www_fdw.o $(LIBJSON)/json.o
EXTENSION = www_fdw
DATA = www_fdw--1.0.0.sql

REGRESS = www_fdw
PG_CPPFLAGS += -I/usr/include/libxml2
SHLIB_LINK += -lcurl -lxml2

all:all-libjson

all-libjson:
	$(MAKE) -C $(LIBJSON) all

clean: clean-libjson

clean-libjson:
	$(MAKE) -C $(LIBJSON) clean

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/www_fdw
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
