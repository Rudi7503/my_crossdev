#!/bin/bash
set -e
BASE=/home/papa/Vampire_V4/ApolloCrossDev
PREFIX=$BASE/Compilers/GCC-6.50-Latest
VASM=$PREFIX/bin/vasmm68k_mot
CC=$PREFIX/bin/m68k-amigaos-gcc
INC="-I$PREFIX/m68k-amigaos/ndk32-include/Include_H -I$PREFIX/m68k-amigaos/sys-include"
cd "$(dirname "$0")"
"$VASM" -quiet -Fhunk -kick1hunks -m68080 -opt-speed -o pcm_asm_test_asm.o pcm_asm_test.s
"$CC" -noixemul -O2 -m68080 -m68881 -fno-inline $INC -c pcm_asm_main.c -o pcm_asm_main.o
"$CC" -noixemul -O2 -m68080 -m68881 pcm_asm_main.o pcm_asm_test_asm.o -o pcm_asm_test -lm
echo built
