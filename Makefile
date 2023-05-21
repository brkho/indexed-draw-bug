ifeq ($(target), dut)
	CXX = /usr/bin/aarch64-cros-linux-gnu-clang++
	CC = /usr/bin/aarch64-cros-linux-gnu-clang
	SYSROOT = /build/cherry
	SSH_DUT = dut
	TARGET = cherry
else ifeq ($(target), local)
	CXX = clang++
	CC = clang
	TARGET = local
else
	CXX = INVALID
	CC = INVALID
	TARGET = INVALID
endif

SUBDIR = "indexed_bug-vertex/"
NAME = indexed_bug
CFLAGS = -std=c++17 --sysroot="$(SYSROOT)" -Wall
LDFLAGS = -lEGL -lGLESv2 -lpng

all: build

build: check
	@$(CXX) $(CFLAGS) -o ${NAME} ${NAME}.cc $(LDFLAGS)

clean: check
	@rm -f ${NAME} with_draw_arrays.png without_draw_arrays.png

check:
ifeq ($(CXX), INVALID)
	$(error $$target must be one of [dut, local] )
endif
