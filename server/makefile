PLATFORM ?= HOST
CROSS_COMPILE = 

ifeq ($(PLATFORM), AARCH64)
	CROSS_COMPILE = aarch64-none-linux-gnu-
endif

CC=$(CROSS_COMPILE)gcc

CFLAGS=-Wall -g -O2 -Wall -Werror -pthread

target=aesdsocket

$(target):

all: $(target)

default: all

clean:
	rm -rf *.o
	rm -f $(target)
