# Makefile for Apollo 68080 Audio MDCT / IMDCT Benchmark
PWD = $(shell pwd)
PREFIX = $(PWD)/../../Compilers/GCC-6.50-Dev
TARGET = m68k-amigaos

C_COMPILER = $(PREFIX)/bin/m68k-amigaos-gcc
S_COMPILER = $(PREFIX)/bin/vasmm68k_mot
O_LINKER   = $(PREFIX)/bin/m68k-amigaos-gcc

C_LIB      = -noixemul
C_LIBS_MATH = -lm
C_INCL_CLB = $(PREFIX)/$(TARGET)/libnix/include
C_INCL_SYS = $(PREFIX)/$(TARGET)/sys-include
C_INCL_NDK = $(PREFIX)/$(TARGET)/ndk32-include/Include_H

C_INCL_ALL = -I. -I$(C_INCL_CLB) -I$(C_INCL_SYS) -I$(C_INCL_NDK)
C_OPTIONS  = -O2 -m68020 -m68881 -ffast-math -fomit-frame-pointer
C_FLAGS    = $(C_LIB) $(C_OPTIONS) $(C_INCL_ALL)

S_FLAGS    = -Fhunk -m68080 -m68881 -opt-speed -spaces

C_FILES    = $(wildcard *.c)
S_FILES    = $(wildcard *.s)
O_FILES    = $(patsubst %.c,%.o,$(C_FILES)) $(patsubst %.s,%.o,$(S_FILES))

EXE        = MDCT

all: $(EXE)

$(EXE): $(O_FILES)
	$(O_LINKER) $(C_LIB) $(O_FILES) $(C_LIBS_MATH) -o $(EXE)

%.o: %.c
	$(C_COMPILER) -c $(C_FLAGS) -o $@ $<

%.o: %.s
	$(S_COMPILER) $(S_FLAGS) -o $@ $<

clean:
	rm -f *.o $(EXE)

.PHONY: all clean
