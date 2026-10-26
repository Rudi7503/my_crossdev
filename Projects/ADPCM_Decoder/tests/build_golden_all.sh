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

"$VASM" -quiet -Fhunk -kick1hunks -m68080 -opt-speed -o dec2_orig.o ../decoder_2bit.s
"$VASM" -quiet -Fhunk -kick1hunks -m68080 -opt-speed -o dec2_signed.o ../experiments/decoder_2bit_signed.s
"$VASM" -quiet -Fhunk -kick1hunks -m68080 -opt-speed -o dec3_orig.o ../decoder_3bit.s
"$VASM" -quiet -Fhunk -kick1hunks -m68080 -opt-speed -o dec3_signed.o ../experiments/decoder_3bit_signed.s

# IMPORTANT: Never pass -m68080 with -O2 to m68k-amigaos-gcc (broken moviw.l/cmpiw.l/dbf offsets)
"$CC" -noixemul -O2 -m68020 -m68881 -fno-inline $INC golden_2bit.c dec2_signed.o -o golden_2bit_signed -lm
"$CC" -noixemul -O2 -m68020 -m68881 -fno-inline $INC golden_3bit.c dec3_signed.o -o golden_3bit_signed -lm
"$CC" -noixemul -O2 -m68020 -m68881 -fno-inline $INC repeat2.c dec2_orig.o -o repeat2_orig -lm
"$CC" -noixemul -O2 -m68020 -m68881 -fno-inline $INC repeat2.c dec2_signed.o -o repeat2_signed -lm
"$CC" -noixemul -O2 -m68020 -m68881 -fno-inline $INC repeat3.c dec3_orig.o -o repeat3_orig -lm
"$CC" -noixemul -O2 -m68020 -m68881 -fno-inline $INC repeat3.c dec3_signed.o -o repeat3_signed -lm

"$ACP" golden_2bit_signed golden_3bit_signed repeat2_orig repeat2_signed repeat3_orig repeat3_signed "${IP}:RAM/"

echo "=== GOLDEN 2-BIT SIGNED ==="
"$ASH" "$IP" RAM:golden_2bit_signed
echo "=== REPEAT 2-BIT ORIG ==="
"$ASH" "$IP" RAM:repeat2_orig
echo "=== REPEAT 2-BIT SIGNED ==="
"$ASH" "$IP" RAM:repeat2_signed
echo "=== GOLDEN 3-BIT SIGNED ==="
"$ASH" "$IP" RAM:golden_3bit_signed
echo "=== REPEAT 3-BIT ORIG ==="
"$ASH" "$IP" RAM:repeat3_orig
echo "=== REPEAT 3-BIT SIGNED ==="
"$ASH" "$IP" RAM:repeat3_signed
