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

python3 "$(dirname "$0")/gen_hybrid.py"
cd "$(dirname "$0")"

"$VASM" -quiet -Fhunk -kick1hunks -m68080 -opt-speed -o dec32_orig.o ../experiments/decoder_3_2ms_orig.s
"$VASM" -quiet -Fhunk -kick1hunks -m68080 -opt-speed -o dec42_orig.o ../experiments/decoder_4_2ms_orig.s
"$VASM" -quiet -Fhunk -kick1hunks -m68080 -opt-speed -o dec53_orig.o ../experiments/decoder_5_3ms_orig.s

"$VASM" -quiet -Fhunk -kick1hunks -m68080 -opt-speed -o dec32_signed.o ../experiments/decoder_3_2ms_signed.s
"$VASM" -quiet -Fhunk -kick1hunks -m68080 -opt-speed -o dec42_signed.o ../experiments/decoder_4_2ms_signed.s
"$VASM" -quiet -Fhunk -kick1hunks -m68080 -opt-speed -o dec53_signed.o ../experiments/decoder_5_3ms_signed.s

"$VASM" -quiet -Fhunk -kick1hunks -m68080 -opt-speed -o dec2_orig.o ../experiments/decoder_2bit_orig.s
"$VASM" -quiet -Fhunk -kick1hunks -m68080 -opt-speed -o dec2_signed.o ../experiments/decoder_2bit_signed.s
"$VASM" -quiet -Fhunk -kick1hunks -m68080 -opt-speed -o dec3_orig.o ../experiments/decoder_3bit_orig.s
"$VASM" -quiet -Fhunk -kick1hunks -m68080 -opt-speed -o dec3_signed.o ../experiments/decoder_3bit_signed.s
"$VASM" -quiet -Fhunk -kick1hunks -m68080 -opt-speed -o dec5_orig.o ../experiments/decoder_5bit_orig.s
"$VASM" -quiet -Fhunk -kick1hunks -m68080 -opt-speed -o dec5_signed.o ../experiments/decoder_5bit_signed.s
"$VASM" -quiet -Fhunk -kick1hunks -m68080 -opt-speed -o dec6_orig.o ../experiments/decoder_6bit_orig.s
"$VASM" -quiet -Fhunk -kick1hunks -m68080 -opt-speed -o dec6_signed.o ../experiments/decoder_6bit_signed.s

"$CC" -noixemul -O2 -m68020 -m68881 -fno-inline $INC -c golden_all.c -o golden_all.o
"$CC" -noixemul -O2 -m68020 -m68881 dec2_orig.o dec3_orig.o dec5_orig.o dec6_orig.o dec32_orig.o dec42_orig.o dec53_orig.o golden_all.o -o golden_all_orig -lm
"$CC" -noixemul -O2 -m68020 -m68881 dec2_signed.o dec3_signed.o dec5_signed.o dec6_signed.o dec32_signed.o dec42_signed.o dec53_signed.o golden_all.o -o golden_all_signed -lm

"$CC" -noixemul -O2 -m68020 -m68881 -fno-inline $INC repeat_hybrid.c dec32_orig.o dec42_orig.o dec53_orig.o -o repeat_hybrid_orig -lm
"$CC" -noixemul -O2 -m68020 -m68881 -fno-inline $INC repeat_hybrid.c dec32_signed.o dec42_signed.o dec53_signed.o -o repeat_hybrid_signed -lm

"$ACP" golden_all_orig golden_all_signed repeat_hybrid_orig repeat_hybrid_signed "${IP}:RAM/"

echo "=== GOLDEN ALL ORIG (5_3MS) ==="
"$ASH" "$IP" "RAM:golden_all_orig 14"
echo "=== GOLDEN ALL SIGNED (ALL 15 MODES) ==="
"$ASH" "$IP" RAM:golden_all_signed
echo "=== REPEAT HYBRID ORIG ==="
"$ASH" "$IP" RAM:repeat_hybrid_orig
echo "=== REPEAT HYBRID SIGNED ==="
"$ASH" "$IP" RAM:repeat_hybrid_signed
