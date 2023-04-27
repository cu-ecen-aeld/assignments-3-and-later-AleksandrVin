CC ?= $(CROSS_COMPILE)gcc

CFLAGS ?= -Wall -g -O2 -Wall -Werror -pthread

target ?= aesdsocket

LDFLAGS ?= -lpthread

$(target):

all: $(target)

default: all

clean:
	rm -rf *.o
	rm -f $(target)
