# Makefile for Apollo 68080 Matrix Multiplication Test Suite
# Compiles C Reference and Optimized 68080 ASM, verifies and benchmarks on V4

PREFIX      = ../../Compilers/GCC-6.50-Latest
TARGET      = m68k-amigaos
CC          = $(PREFIX)/bin/m68k-amigaos-gcc
VASM        = $(PREFIX)/bin/vasmm68k_mot
ACP         = ../../Projects/ApolloExplorer/acp/acp
AMIGAHOST   = 192.168.178.50

# C Flags: use -m68020 -m68881 for GCC to avoid GCC 6.5.0 -m68080 moviw.l bug
CFLAGS      = -O2 -m68020 -m68881 -ffast-math -fomit-frame-pointer -noixemul
SFLAGS      = -Fhunk -m68080 -m68881 -opt-speed -kick1hunks -quiet

TARGET_EXE  = Matrix_Mul

all: $(TARGET_EXE)

$(TARGET_EXE): main.o matrix_mul_c.o matrix_mul_asm.o
	$(CC) $(CFLAGS) -o $@ $^ -lm
	@echo "Build successful: $(TARGET_EXE)"

main.o: main.c matrix_mul.h
	$(CC) $(CFLAGS) -c $< -o $@

matrix_mul_c.o: matrix_mul_c.c matrix_mul.h
	$(CC) $(CFLAGS) -c $< -o $@

matrix_mul_asm.o: matrix_mul_asm.s
	$(VASM) $(SFLAGS) $< -o $@

clean:
	rm -f *.o $(TARGET_EXE)

upload: $(TARGET_EXE)
	@echo "Uploading $(TARGET_EXE) to $(AMIGAHOST):RAM/..."
	$(ACP) $(TARGET_EXE) $(AMIGAHOST):RAM/

run:
	@echo "Running $(TARGET_EXE) on $(AMIGAHOST)..."
	"/mnt/c/Program Files/ApolloExplorer/ash.exe" $(AMIGAHOST) "RAM:$(TARGET_EXE)"

.PHONY: all clean upload run
