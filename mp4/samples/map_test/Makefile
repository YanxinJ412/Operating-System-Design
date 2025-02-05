LINUX ?= $(HOME)/linux

LINUX_INC = ${LINUX}/usr/include
LIBBPF_DIR = $(abspath ${LINUX}/tools/lib/bpf)
LIBREX_DIR := $(shell if [ -d "/rex-kernel/librex" ]; then echo "/rex-kernel/librex"; else echo `realpath ../../librex`; fi)

RUST_FLAGS = -Funsafe_code -Clink-arg=-nostartfiles -Cenable_rex
CARGO_FLAGS = 

CC = gcc

CFLAGS += -O2 -march=native -pipe -std=gnu11
CFLAGS += -ffunction-sections -fdata-sections -fno-semantic-interposition
LDFLAGS += -fuse-ld=mold -Wl,--as-needed -Wl,-O1 -Wl,--gc-sections

LOADER_INC += -I${LINUX_INC} -I${LIBBPF_DIR} -I${LIBREX_DIR}
LOADER_LDFLAGS = -L${LIBBPF_DIR} -L${LIBREX_DIR} -lbpf -lrex -Wl,-rpath=${LIBBPF_DIR} -Wl,-rpath=${LIBREX_DIR}

V ?= 0

ifeq ($(V),1)
CARGO_FLAGS += -v
endif

ifeq ($(V),2)
CARGO_FLAGS += -vv
endif

all: target/x86_64-unknown-linux-gnu/release/map_test loader event-trigger

target/x86_64-unknown-linux-gnu/debug/map_test: Cargo.toml ./src/*.rs
	cargo rustc ${CARGO_FLAGS} -- ${RUST_FLAGS}

target/x86_64-unknown-linux-gnu/release/map_test: Cargo.toml ./src/*.rs
	cargo rustc ${CARGO_FLAGS} --release -- ${RUST_FLAGS}

llvm-ir: Cargo.toml ./src/*.rs
	cargo rustc ${CARGO_FLAGS} --release -- ${RUST_FLAGS} --emit llvm-ir

mir: ./src/*.rs prep_interface.py
	cargo rustc ${CARGO_FLAGS} -- ${RUST_FLAGS} --emit mir

asm: ./src/*.rs prep_interface.py
	cargo rustc ${CARGO_FLAGS} -- ${RUST_FLAGS} --emit asm

loader: loader.c
	${CC} ${LOADER_INC} ${CFLAGS} ${LDFLAGS} -o $@ $< ${LOADER_LDFLAGS}

event-trigger: event-trigger.c
	${CC} -I${LINUX_INC} ${CFLAGS} ${LDFLAGS} -o $@ $<

clean:
	cargo clean
	rm -rf loader event-trigger ./src/linux ./src/stub.rs
