PG_CONFIG= pg_config
CFLAGS	= $(shell $(PG_CONFIG) --cflags)
CFLAGS	+= -I../..

SRC		= $(wildcard *.c)
OBJ		= $(patsubst %.c,%.o,$(SRC)) ../../libjson-0.8/json.o ../../src/json_parser.o
TARGET	= $(patsubst %.c,%,$(SRC))

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)

$(OBJ): $(SRC)

clean:
	rm -f $(TARGET) $(OBJ)
