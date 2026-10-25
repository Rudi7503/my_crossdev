#!/bin/bash
set -e
BASE=/home/papa/Vampire_V4/ApolloCrossDev
PREFIX=$BASE/Compilers/GCC-6.50-Latest
VASM=$PREFIX/bin/vasmm68k_mot
CC=$PREFIX/bin/m68k-amigaos-gcc
INC="-I$PREFIX/m68k-amigaos/ndk32-include/Include_H -I$PREFIX/m68k-amigaos/sys-include"
cd "$(dirname "$0")"
"$VASM" -quiet -Fhunk -kick1hunks -m68080 -opt-speed -o pcm_ovf_asm.o pcm_ovf.s
"$CC" -noixemul -O2 -m68080 -m68881 -fno-inline $INC -c pcm_ovf_test.c -o pcm_ovf_test.o
"$CC" -noixemul -O2 -m68080 -m68881 pcm_ovf_test.o pcm_ovf_asm.o -o pcm_ovf_test -lm
echo built
