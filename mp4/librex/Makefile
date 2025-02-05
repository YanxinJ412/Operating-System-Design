LINUX ?= $(HOME)/linux

ifeq ($(LLVM),1) # Using LLVM toolchain
CXX = clang++
LD = clang++
AR = llvm-ar
else # Using GNU toolchain
CXX = g++
LD = g++
AR = gcc-ar
endif

INC += -I${LINUX}/usr/include -I${LINUX}/tools/lib

CXXFLAGS += -O2 -march=native -pipe -std=c++23 -fPIC -Wall -Werror -g
CXXFLAGS += -ffunction-sections -fdata-sections -fno-semantic-interposition
CXXFLAGS += -fvisibility=hidden

ifeq ($(LLVM),1) # LLVM ThinLTO
CXXFLAGS += -flto=thin
else # GCC FAT LTO
CXXFLAGS += -flto=jobserver -ffat-lto-objects -fuse-linker-plugin
endif

LDFLAGS += -fuse-ld=mold -Wl,--as-needed -Wl,-O1 -Wl,--gc-sections
LDFLAGS += -L${LINUX}/tools/lib/bpf -lbpf -lelf

all: librex.a librex.so

librex.a: librex.o
	+${AR} rcs $@ $<

librex.so: librex.o
	+${LD} ${CXXFLAGS} -shared -o $@ $< ${LDFLAGS}

librex.o: librex.cpp librex.h bindings.h
	+${CXX} ${INC} ${CXXFLAGS} -o $@ -c librex.cpp

clean:
	rm -f librex.o librex.a librex.so
