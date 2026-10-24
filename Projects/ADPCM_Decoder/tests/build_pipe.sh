#!/bin/bash
set -e
BASE=/home/papa/Vampire_V4/ApolloCrossDev
PREFIX=$BASE/Compilers/GCC-6.50-Latest
VASM=$PREFIX/bin/vasmm68k_mot
CC=$PREFIX/bin/m68k-amigaos-gcc
INC="-I$PREFIX/m68k-amigaos/ndk32-include/Include_H -I$PREFIX/m68k-amigaos/sys-include"
cd "$(dirname "$0")"
"$VASM" -quiet -Fhunk -kick1hunks -m68080 -m68881 -opt-speed -I"$PREFIX/m68k-amigaos/DevPac/Include" -o decoder_4bit_pipe.o ../decoder_4bit.s
"$VASM" -quiet -Fhunk -kick1hunks -m68080 -o uart_asm.o uart_asm.s
"$VASM" -quiet -Fhunk -kick1hunks -m68080 -o iep_read.o iep_read.s
"$CC" -noixemul -O0 -m68080 -m68881 -fno-inline $INC -c pipe_test.c -o pipe_test.o
"$CC" -noixemul -O0 -m68080 -m68881 -fno-inline $INC pipe_test.o decoder_4bit_pipe.o uart_asm.o iep_read.o -o pipe_test -lm
echo built
