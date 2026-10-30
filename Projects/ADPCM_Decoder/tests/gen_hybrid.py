#!/usr/bin/env python3
from pathlib import Path

steps = [
 7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,
 118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,
 876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,
 4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,
 18500,20350,22385,24623,27086,29794,32767
]

def gen_table(bps, name=None):
    if name is None:
        name = f"delta{bps}_table"
    lines = [f"            even\n{name}:"]
    row = []
    for s in steps:
        for code in range(1 << bps):
            if bps == 2:
                d = (s >> 1) + (s if (code & 1) else 0)
                if code & 2: d = -d
            elif bps == 3:
                d = (s >> 2) + ((s >> 1) if (code & 1) else 0) + (s if (code & 2) else 0)
                if code & 4: d = -d
            elif bps == 4:
                d = (s >> 3) + ((s >> 2) if (code & 1) else 0) + ((s >> 1) if (code & 2) else 0) + (s if (code & 4) else 0)
                if code & 8: d = -d
            elif bps == 5:
                d = (s >> 4) + ((s >> 3) if (code & 1) else 0) + ((s >> 2) if (code & 2) else 0) + ((s >> 1) if (code & 4) else 0) + (s if (code & 8) else 0)
                if code & 16: d = -d
            row.append(str(d))
            if len(row) == 8:
                lines.append("            dc.l    " + ",".join(row))
                row = []
    if row:
        lines.append("            dc.l    " + ",".join(row))
    return "\n".join(lines)

proj = Path(__file__).resolve().parent.parent

# 1. decoder_3_2ms_signed.s
src32 = """            SECTION CODE,CODE
            XDEF    _decode_3_2bit_ms_asm

DEC_PAIR_3_2 MACRO
            move.l  d7,d0
            andi.l  #7,d0
            lsr.l   #3,d7
            move.w  d3,d1
            lsl.w   #3,d1
            add.w   d0,d1
            move.l  (a4,d1.w*4),d1
            add.l   d1,d2
            cmp.l   #32767,d2
            ble.s   .mmin\\@
            move.l  #32767,d2
            bra.s   .mq\\@
.mmin\\@:
            cmp.l   #-32768,d2
            bge.s   .mq\\@
            move.l  #-32768,d2
.mq\\@:
            andi.w  #3,d0
            add.w   (a5,d0.w*2),d3
            tst.w   d3
            bge.s   .mmax\\@
            moveq   #0,d3
            bra.s   .mi\\@
.mmax\\@:
            cmp.w   #88,d3
            ble.s   .mi\\@
            move.w  #88,d3
.mi\\@:
            move.l  d7,d0
            andi.l  #3,d0
            lsr.l   #2,d7
            move.w  d5,d1
            lsl.w   #2,d1
            add.w   d0,d1
            move.l  (a2,d1.w*4),d1
            add.l   d1,d4
            cmp.l   #32767,d4
            ble.s   .smin\\@
            move.l  #32767,d4
            bra.s   .sq\\@
.smin\\@:
            cmp.l   #-32768,d4
            bge.s   .sq\\@
            move.l  #-32768,d4
.sq\\@:
            add.w   (a6,d0.w*2),d5
            tst.w   d5
            bge.s   .smax\\@
            moveq   #0,d5
            bra.s   .si\\@
.smax\\@:
            cmp.w   #88,d5
            ble.s   .si\\@
            move.w  #88,d5
.si\\@:
            move.l  d2,d0
            add.l   d4,d0
            move.l  d2,d1
            sub.l   d4,d1
            cmp.l   #32767,d0
            ble.s   .cl1\\@
            move.l  #32767,d0
            bra.s   .cr\\@
.cl1\\@:
            cmp.l   #-32768,d0
            bge.s   .cr\\@
            move.l  #-32768,d0
.cr\\@:
            cmp.l   #32767,d1
            ble.s   .cr1\\@
            move.l  #32767,d1
            bra.s   .pk\\@
.cr1\\@:
            cmp.l   #-32768,d1
            bge.s   .pk\\@
            move.l  #-32768,d1
.pk\\@:
            swap    d0
            move.w  d1,d0
            move.l  d0,(a1)+
        ENDM

_decode_3_2bit_ms_asm:
            movem.l d2-d7/a2-a6,-(a7)
            movec   ccc,d0
            move.l  d0,e2
            move.l  48(a7),a0
            move.l  52(a7),a1
            move.l  56(a7),a5
            move.l  60(a7),a3
            move.l  (a5),d2
            move.w  4(a5),d3
            ext.l   d3
            move.l  (a3),d4
            move.w  4(a3),d5
            ext.l   d5
            lea     delta3_table(pc),a4
            lea     delta2_table(pc),a2
            lea     index_adj_3bit(pc),a5
            lea     index_adj_2bit(pc),a6
            tst.l   64(a7)
            beq     .ms_done
.ms_block_loop:
            movex.l (a0)+,d7
            REPT    6
            DEC_PAIR_3_2
            ENDR
            ror.l   #2,d7
            movex.w (a0)+,d7
            rol.l   #2,d7
            REPT    3
            DEC_PAIR_3_2
            ENDR
            ror.l   #3,d7
            movex.w (a0)+,d7
            rol.l   #3,d7
            REPT    3
            DEC_PAIR_3_2
            ENDR
            ror.l   #4,d7
            movex.w (a0)+,d7
            rol.l   #4,d7
            REPT    4
            DEC_PAIR_3_2
            ENDR
            movex.l (a0)+,d7
            REPT    6
            DEC_PAIR_3_2
            ENDR
            ror.l   #2,d7
            moveq   #0,d1
            move.b  (a0)+,d1
            or.l    d1,d7
            rol.l   #2,d7
            REPT    2
            DEC_PAIR_3_2
            ENDR
            subq.l  #1,64(a7)
            bne     .ms_block_loop
.ms_done:
            move.l  56(a7),a5
            move.l  d2,(a5)
            move.w  d3,4(a5)
            move.l  60(a7),a3
            move.l  d4,(a3)
            move.w  d5,4(a3)
            movem.l (a7)+,d2-d7/a2-a6
            movec   ccc,d0
            move.l  e2,d1
            sub.l   d1,d0
            rts

index_adj_3bit:
            dc.w    -1, -1,  1,  2, -1, -1,  1,  2
index_adj_2bit:
            dc.w    -1,  2, -1,  2
""" + gen_table(3) + "\n" + gen_table(2) + "\n            END\n"
target = proj / "experiments/decoder_3_2ms_signed.s"
if not target.exists():
    target.write_text(src32)

# 2. decoder_4_2ms_signed.s
src42 = """            SECTION CODE,CODE
            XDEF    _decode_4_2bit_ms_asm

_decode_4_2bit_ms_asm:
            movem.l d2-d7/a2-a6,-(a7)
            movec   ccc,d0
            move.l  d0,e1
            move.l  48(a7),a0
            move.l  52(a7),a1
            move.l  56(a7),a5
            move.l  60(a7),a3
            move.l  (a5),d2
            move.w  4(a5),d3
            ext.l   d3
            move.l  (a3),d4
            move.w  4(a3),d5
            ext.l   d5
            lea     delta4_table(pc),a4
            lea     delta2_table(pc),a2
            lea     index_adj_4bit(pc),a5
            lea     index_adj_2bit(pc),a6
            tst.l   64(a7)
            beq     .ms_done
.ms_block_loop:
            moveq   #4,d6
.ms_chunk_loop:
            movex.l (a0),e0
            addq.l  #3,a0
            moveq   #3,d7
.ms_pair_loop:
            move.l  e0,d0
            andi.l  #15,d0
            lsr.l   #4,e0
            move.w  d3,d1
            lsl.w   #4,d1
            add.w   d0,d1
            move.l  (a4,d1.w*4),d1
            add.l   d1,d2
            cmp.l   #32767,d2
            ble.s   .mmin
            move.l  #32767,d2
            bra.s   .mq
.mmin:
            cmp.l   #-32768,d2
            bge.s   .mq
            move.l  #-32768,d2
.mq:
            andi.w  #7,d0
            add.w   (a5,d0.w*2),d3
            tst.w   d3
            bge.s   .mmax
            moveq   #0,d3
            bra.s   .mi
.mmax:
            cmp.w   #88,d3
            ble.s   .mi
            move.w  #88,d3
.mi:
            move.l  e0,d0
            andi.l  #3,d0
            lsr.l   #2,e0
            move.w  d5,d1
            lsl.w   #2,d1
            add.w   d0,d1
            move.l  (a2,d1.w*4),d1
            add.l   d1,d4
            cmp.l   #32767,d4
            ble.s   .smin
            move.l  #32767,d4
            bra.s   .sq
.smin:
            cmp.l   #-32768,d4
            bge.s   .sq
            move.l  #-32768,d4
.sq:
            add.w   (a6,d0.w*2),d5
            tst.w   d5
            bge.s   .smax
            moveq   #0,d5
            bra.s   .si
.smax:
            cmp.w   #88,d5
            ble.s   .si
            move.w  #88,d5
.si:
            move.l  d2,d0
            add.l   d4,d0
            move.l  d2,d1
            sub.l   d4,d1
            cmp.l   #32767,d0
            ble.s   .ms_chk_min_L
            move.l  #32767,d0
            bra.s   .ms_clamp_R
.ms_chk_min_L:
            cmp.l   #-32768,d0
            bge.s   .ms_clamp_R
            move.l  #-32768,d0
.ms_clamp_R:
            cmp.l   #32767,d1
            ble.s   .ms_chk_min_R
            move.l  #32767,d1
            bra.s   .ms_pack
.ms_chk_min_R:
            cmp.l   #-32768,d1
            bge.s   .ms_pack
            move.l  #-32768,d1
.ms_pack:
            swap    d0
            move.w  d1,d0
            move.l  d0,(a1)+
            dbf     d7,.ms_pair_loop
            dbf     d6,.ms_chunk_loop
            subq.l  #1,64(a7)
            bne     .ms_block_loop
.ms_done:
            move.l  56(a7),a0
            move.l  d2,(a0)
            move.w  d3,4(a0)
            move.l  60(a7),a0
            move.l  d4,(a0)
            move.w  d5,4(a0)
            movem.l (a7)+,d2-d7/a2-a6
            movec   ccc,d0
            move.l  e1,d1
            sub.l   d1,d0
            rts

index_adj_4bit:
            dc.w    -1, -1, -1, -1,  2,  4,  6,  8
index_adj_2bit:
            dc.w    -1,  2, -1,  2
""" + gen_table(4) + "\n" + gen_table(2) + "\n            END\n"
target = proj / "experiments/decoder_4_2ms_signed.s"
if not target.exists():
    target.write_text(src42)

# 3. decoder_5_3ms_signed.s
src53 = """            SECTION CODE,CODE
            XDEF    _decode_5_3bit_ms_asm

DEC_PAIR_5_3 MACRO
            move.l  e0,d0
            andi.l  #31,d0
            lsr.l   #5,e0
            move.w  d3,d1
            lsl.w   #5,d1
            add.w   d0,d1
            move.l  (a4,d1.w*4),d1
            add.l   d1,d2
            cmp.l   #32767,d2
            ble.s   .mmin\\@
            move.l  #32767,d2
            bra.s   .mq\\@
.mmin\\@:
            cmp.l   #-32768,d2
            bge.s   .mq\\@
            move.l  #-32768,d2
.mq\\@:
            andi.w  #15,d0
            add.w   (a5,d0.w*2),d3
            tst.w   d3
            bge.s   .mmax\\@
            moveq   #0,d3
            bra.s   .mi\\@
.mmax\\@:
            cmp.w   #88,d3
            ble.s   .mi\\@
            move.w  #88,d3
.mi\\@:
            move.l  e0,d0
            andi.l  #7,d0
            lsr.l   #3,e0
            move.w  d5,d1
            lsl.w   #3,d1
            add.w   d0,d1
            move.l  (a2,d1.w*4),d1
            add.l   d1,d4
            cmp.l   #32767,d4
            ble.s   .smin\\@
            move.l  #32767,d4
            bra.s   .sq\\@
.smin\\@:
            cmp.l   #-32768,d4
            bge.s   .sq\\@
            move.l  #-32768,d4
.sq\\@:
            andi.w  #3,d0
            add.w   (a6,d0.w*2),d5
            tst.w   d5
            bge.s   .smax\\@
            moveq   #0,d5
            bra.s   .si\\@
.smax\\@:
            cmp.w   #88,d5
            ble.s   .si\\@
            move.w  #88,d5
.si\\@:
            move.l  d2,d0
            add.l   d4,d0
            move.l  d2,d1
            sub.l   d4,d1
            cmp.l   #32767,d0
            ble.s   .cl1\\@
            move.l  #32767,d0
            bra.s   .cr\\@
.cl1\\@:
            cmp.l   #-32768,d0
            bge.s   .cr\\@
            move.l  #-32768,d0
.cr\\@:
            cmp.l   #32767,d1
            ble.s   .cr1\\@
            move.l  #32767,d1
            bra.s   .pk\\@
.cr1\\@:
            cmp.l   #-32768,d1
            bge.s   .pk\\@
            move.l  #-32768,d1
.pk\\@:
            swap    d0
            move.w  d1,d0
            move.l  d0,(a1)+
        ENDM

_decode_5_3bit_ms_asm:
            movem.l d2-d7/a2-a6,-(a7)
            movec   ccc,d0
            move.l  d0,e2
            move.l  48(a7),a0
            move.l  52(a7),a1
            move.l  56(a7),a5
            move.l  60(a7),a3
            move.l  (a5),d2
            move.w  4(a5),d3
            ext.l   d3
            move.l  (a3),d4
            move.w  4(a3),d5
            ext.l   d5
            lea     delta5_table(pc),a4
            lea     delta3_table(pc),a2
            lea     index_adj_5bit(pc),a5
            lea     index_adj_3bit(pc),a6
            move.l  64(a7),d6
            beq     .ms_done
            move.l  d6,d7
            lsr.l   #2,d7
            beq     .ms_byte_loop_init
.ms_chunk_loop:
            movex.l (a0)+,e0
            REPT    4
            DEC_PAIR_5_3
            ENDR
            subq.l  #1,d7
            bne     .ms_chunk_loop
.ms_byte_loop_init:
            andi.l  #3,d6
            beq     .ms_done
.ms_byte_loop:
            moveq   #0,e0
            move.b  (a0)+,e0
            DEC_PAIR_5_3
            subq.l  #1,d6
            bne     .ms_byte_loop
.ms_done:
            move.l  56(a7),a0
            move.l  d2,(a0)
            move.w  d3,4(a0)
            move.l  60(a7),a0
            move.l  d4,(a0)
            move.w  d5,4(a0)
            movem.l (a7)+,d2-d7/a2-a6
            movec   ccc,d0
            move.l  e2,d1
            sub.l   d1,d0
            rts

index_adj_5bit:
            dc.w    -1, -1, -1, -1, -1, -1, -1, -1
            dc.w     1,  2,  4,  6,  8, 10, 13, 16
index_adj_3bit:
            dc.w    -1, -1, 1, 2
""" + gen_table(5) + "\n" + gen_table(3) + "\n            END\n"
target = proj / "experiments/decoder_5_3ms_signed.s"
if not target.exists():
    target.write_text(src53)
print("Generated 3_2ms, 4_2ms, 5_3ms signed")
