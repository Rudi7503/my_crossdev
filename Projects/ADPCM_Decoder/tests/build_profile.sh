#!/bin/bash
set -e
BASE=/home/papa/Vampire_V4/ApolloCrossDev
PREFIX=$BASE/Compilers/GCC-6.50-Latest
VASM=$PREFIX/bin/vasmm68k_mot
CC=$PREFIX/bin/m68k-amigaos-gcc
INC="-I$PREFIX/m68k-amigaos/ndk32-include/Include_H -I$PREFIX/m68k-amigaos/sys-include"
cd "$(dirname "$0")"
"$VASM" -quiet -Fhunk -kick1hunks -m68080 -m68881 -opt-speed -I"$PREFIX/m68k-amigaos/DevPac/Include" -o decoder_4bit_prof.o ../experiments/decoder_4bit_prof.s
"$CC" -noixemul -O0 -m68080 -m68881 -fno-inline $INC -c profile_test.c -o profile_test.o
"$CC" -noixemul -O0 -m68080 -m68881 -fno-inline $INC profile_test.o decoder_4bit_prof.o -o profile_test -lm
echo "built $(pwd)/profile_test"
