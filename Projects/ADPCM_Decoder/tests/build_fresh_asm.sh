#!/bin/bash
set -e
BASE=/home/papa/Vampire_V4/ApolloCrossDev
PREFIX=$BASE/Compilers/GCC-6.50-Latest
VASM=$PREFIX/bin/vasmm68k_mot
CC=$PREFIX/bin/m68k-amigaos-gcc
INC="-I$PREFIX/m68k-amigaos/ndk32-include/Include_H -I$PREFIX/m68k-amigaos/sys-include"
cd "$(dirname "$0")"
"$VASM" -quiet -Fhunk -kick1hunks -m68080 -m68881 -opt-speed -o decoder_fresh_asm.o ../experiments/decoder_4bit_fresh_asm.s
"$VASM" -quiet -Fhunk -kick1hunks -m68080 -o uart_asm.o uart_asm.s
"$CC" -noixemul -O0 -m68080 -m68881 -fno-inline $INC -DTEST_VERSION='"freshasm"' -c golden_test.c -o golden_freshasm.o
"$CC" -noixemul -O0 -m68080 -m68881 -fno-inline $INC -DTEST_VERSION='"freshasm"' -c repeat_test.c -o repeat_freshasm.o
"$CC" -noixemul -O0 -m68080 -m68881 -fno-inline $INC golden_freshasm.o decoder_fresh_asm.o uart_asm.o -o golden_test_freshasm -lm
"$CC" -noixemul -O0 -m68080 -m68881 -fno-inline $INC repeat_freshasm.o decoder_fresh_asm.o uart_asm.o -o repeat_test_freshasm -lm
echo built
