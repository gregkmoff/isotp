OBJS = isotp.o \
	isotp_addressing.o \
	isotp_cf.o \
	isotp_common.o \
	isotp_fc.o \
	isotp_ff.o \
	isotp_recv.o \
	isotp_send.o \
	isotp_sf.o \
	can/can.o
SRCS = isotp.c \
	isotp_addressing.c \
	isotp_cf.c \
	isotp_common.c \
	isotp_fc.c \
	isotp_ff.c \
	isotp_recv.c \
	isotp_send.c \
	isotp_sf.c \
	can/can.c
LINTS = isotp.lint \
	isotp_addressing.lint \
	isotp_cf.lint \
	isotp_common.lint \
	isotp_fc.lint \
	isotp_ff.lint \
	isotp_recv.lint \
	isotp_send.lint \
	isotp_sf.lint \
	can/can.lint

CC = gcc
CFLAGS += -c -I. -W -Wall -Werror
LINT = cpplint

%.o : %.c | %.lint
	$(CC) -o $@ $(CFLAGS) $<

can/%.o : can/%.c | can/%.lint
	$(CC) -o $@ $(CFLAGS) $<

%.lint : %.c
	$(LINT) --filter=-readability/casting $< > $@

lint : %.lint

clean :
	rm $(OBJS) $(LINTS)

.PHONY : lint clean all

all : $(OBJS) $(LINTS)
