#!/bin/bash
set -e
BASE=/home/papa/Vampire_V4/ApolloCrossDev
PREFIX=$BASE/Compilers/GCC-6.50-Latest
VASM=$PREFIX/bin/vasmm68k_mot
CC=$PREFIX/bin/m68k-amigaos-gcc
INC="-I$PREFIX/m68k-amigaos/ndk32-include/Include_H -I$PREFIX/m68k-amigaos/sys-include"
ACP="/mnt/c/Program Files/ApolloExplorer/acp.exe"
ASH="/mnt/c/Program Files/ApolloExplorer/ash.exe"
IP="192.168.178.50"

cd "$(dirname "$0")"

"$VASM" -quiet -Fhunk -kick1hunks -m68080 -opt-speed -o dec5_orig.o ../decoder_5bit.s
"$VASM" -quiet -Fhunk -kick1hunks -m68080 -opt-speed -o dec5_signed.o ../experiments/decoder_5bit_signed.s
"$VASM" -quiet -Fhunk -kick1hunks -m68080 -opt-speed -o dec6_orig.o ../decoder_6bit.s
"$VASM" -quiet -Fhunk -kick1hunks -m68080 -opt-speed -o dec6_signed.o ../experiments/decoder_6bit_signed.s

# Always compile C test harnesses with -m68020 -O2 (never -m68080 -O2!)
"$CC" -noixemul -O2 -m68020 -m68881 -fno-inline $INC golden_5bit.c dec5_signed.o -o golden_5bit_signed -lm
"$CC" -noixemul -O2 -m68020 -m68881 -fno-inline $INC golden_6bit.c dec6_signed.o -o golden_6bit_signed -lm
"$CC" -noixemul -O2 -m68020 -m68881 -fno-inline $INC repeat5.c dec5_orig.o -o repeat5_orig -lm
"$CC" -noixemul -O2 -m68020 -m68881 -fno-inline $INC repeat5.c dec5_signed.o -o repeat5_signed -lm
"$CC" -noixemul -O2 -m68020 -m68881 -fno-inline $INC repeat6.c dec6_orig.o -o repeat6_orig -lm
"$CC" -noixemul -O2 -m68020 -m68881 -fno-inline $INC repeat6.c dec6_signed.o -o repeat6_signed -lm

"$ACP" golden_5bit_signed golden_6bit_signed repeat5_orig repeat5_signed repeat6_orig repeat6_signed "${IP}:RAM/"

echo "=== GOLDEN 5-BIT SIGNED ==="
"$ASH" "$IP" RAM:golden_5bit_signed
echo "=== REPEAT 5-BIT ORIG ==="
"$ASH" "$IP" RAM:repeat5_orig
echo "=== REPEAT 5-BIT SIGNED ==="
"$ASH" "$IP" RAM:repeat5_signed
echo "=== GOLDEN 6-BIT SIGNED ==="
"$ASH" "$IP" RAM:golden_6bit_signed
echo "=== REPEAT 6-BIT ORIG ==="
"$ASH" "$IP" RAM:repeat6_orig
echo "=== REPEAT 6-BIT SIGNED ==="
"$ASH" "$IP" RAM:repeat6_signed
