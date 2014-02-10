# parse extension name from META.json:
EXTENSION	= $(shell grep '"name"\s*:' META.json | \
               sed -e 's/[[:space:]]*"name"[[:space:]]*:[[:space:]]*"\([^"]*\)"[[:space:]]*,[[:space:]]*/\1/')
# parse extension version from control file:
EXTVERSION	= $(shell grep default_version $(EXTENSION).control | \
               sed -e "s/default_version[[:space:]]*=[[:space:]]*'\([^']*\)'/\1/")

DATA		= $(filter-out $(wildcard sql/*--*.sql),$(wildcard sql/*.sql))
TESTS		= $(wildcard test/sql/*.sql)
REGRESS		= $(patsubst test/sql/%.sql,%,$(TESTS))
REGRESS_OPTS= --inputdir=test
PG_CONFIG	= pg_config
LIBJSON		= libjson-0.8
# use module big instead:
#MODULES		= $(patsubst %.c,%,$(wildcard src/*.c))
MODULE_big	= $(EXTENSION)
OBJS		= $(patsubst %.c,%.o,$(wildcard src/*.c)) $(LIBJSON)/json.o
PG_CPPFLAGS	+= -I/usr/include/libxml2
SHLIB_LINK	+= -lcurl -lxml2

PG91         = $(shell $(PG_CONFIG) --version | grep -qE " 8\.| 9\.0" && echo no || echo yes)

ifeq ($(PG91),yes)
all: sql/$(EXTENSION)--$(EXTVERSION).sql

sql/$(EXTENSION)--$(EXTVERSION).sql: sql/$(EXTENSION).sql
	cp $< $@

DATA = sql/$(EXTENSION)--$(EXTVERSION).sql
EXTRA_CLEAN = sql/$(EXTENSION)--$(EXTVERSION).sql
endif

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
