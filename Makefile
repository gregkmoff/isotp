# Copyright 2024, Greg Moffatt (Greg.Moffatt@gmail.com)
# 
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
# 
# 1. Redistributions of source code must retain the above copyright notice, this
# list of conditions and the following disclaimer.
# 
# 2. Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation and/or
# other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS” AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
UNIT_TESTS = can/can_ut.c

CC = gcc
CFLAGS += -c -I. -W -Wall -Werror -fPIC
LINT = cpplint
LINT_FLAGS = --filter=-readability/casting,-build/include_what_you_use
BUILD_DIR = ./build
OBJ_DIR = ${BUILD_DIR}/obj
LINT_DIR = ${BUILD_DIR}/lint
REV=$(git rev-parse --short HEAD)
CMOCKA_FLAGS=$(pkg-config --cflags --libs cmocka)
LIB = ${BUILD_DIR}/libisotp.so

default: all

all : setup $(OBJS) lib

setup:
	@mkdir -p ${BUILD_DIR}
	@mkdir -p ${OBJ_DIR}/can
	@mkdir -p ${LINT_DIR}/can

%.o : %.c
	$(CC) -o ${OBJ_DIR}/$@ $(CFLAGS) $<

can/%.o : can/%.c
	$(CC) -o ${OBJ_DIR}/$@ $(CFLAGS) $<

lib: setup $(SRCS) $(OBJS)
	@echo "Linking libisotp.so..."
	$(eval GIT_TAG := $(shell git rev-parse --short HEAD))
	@echo "...generating version $(GIT_TAG)"
	$(CC) -shared -o ${BUILD_DIR}/libisotp.$(GIT_TAG).so ${OBJ_DIR}/can/*.o ${OBJ_DIR}/*.o
	@ln -sf libisotp.$(GIT_TAG).so ${LIB}

.PHONY : clean all lib test main_test

clean :
	@rm -rf ${BUILD_DIR}

.PHONY : lint
lint: $(SRCS)
	@$(LINT) $(LINT_FLAGS) $(SRCS)

.PHONY : debug
debug: CFLAGS += -g -DDEBUG
debug: lib

test: $(UNIT_TESTS) $(OBJS)
	@$(eval CMOCKA_FLAGS := $(shell pkg-config --cflags --libs cmocka))
	@$(CC) -I. -o ${BUILD_DIR}/can_ut $(CMOCKA_FLAGS) ${OBJ_DIR}/can/can.o can/can_ut.c
	${BUILD_DIR}/can_ut
	@$(CC) -I. -o ${BUILD_DIR}/isotp_cf_ut $(CMOCKA_FLAGS) ${OBJ_DIR}/isotp_cf.o unit_tests/isotp_cf_ut.c
	${BUILD_DIR}/isotp_cf_ut
	@$(CC) -I. -o ${BUILD_DIR}/isotp_fc_ut $(CMOCKA_FLAGS) ${OBJ_DIR}/isotp_fc.o unit_tests/isotp_fc_ut.c
	${BUILD_DIR}/isotp_fc_ut
	@$(CC) -I. -o ${BUILD_DIR}/isotp_ff_ut $(CMOCKA_FLAGS) ${OBJ_DIR}/isotp_ff.o unit_tests/isotp_ff_ut.c
	${BUILD_DIR}/isotp_ff_ut
	@$(CC) -I. -o ${BUILD_DIR}/isotp_sf_ut $(CMOCKA_FLAGS) ${OBJ_DIR}/isotp_sf.o unit_tests/isotp_sf_ut.c
	${BUILD_DIR}/isotp_sf_ut

main_test: $(LIB)
	$(CC) -I. -L${BUILD_DIR} -lc -lisotp unit_tests/main_test.c -o ${BUILD_DIR}/main_test
	${BUILD_DIR}/main_test

isotp_udp: debug
	$(CC) -g -DDEBUG -I. -L${BUILD_DIR} -lc -lisotp unit_tests/isotp_udp.c -o ${BUILD_DIR}/isotp_udp
