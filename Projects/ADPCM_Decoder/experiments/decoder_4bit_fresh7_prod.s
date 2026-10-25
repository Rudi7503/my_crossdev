
            xdef    _decode_4bit_stereo_asm
            xdef    _decode_4bit_ms_asm
            xdef    _decode_4bit_mono_asm

; Fresh independent 4-bit decoder (unrolled 4 pairs, no bsr/E2-E5 scratch).
; Same I/O contract; tables/data copied, control flow rewritten.

F_PAIR MACRO
            move.l  e0,d0
            andi.l  #15,d0
            lsr.l   #4,e0
            move.l  e0,d1
            andi.l  #15,d1
            lsr.l   #4,e0

            moveq   #0,d6
            move.w  d3,d6
            lsl.w   #3,d6
            move.l  d0,d7
            and.l   #7,d7
            add.w   d6,d7
            move.w  (a4,d7.w*2),d6

            btst    #3,d0
            beq     .ladd\@
            sub.l   d6,d2
            bra     .lclamp\@
.ladd\@:
            add.l   d6,d2
.lclamp\@:
            cmp.l   #32767,d2
            ble     .lmin\@
            move.l  #32767,d2
            bra     .lidx\@
.lmin\@:
            cmp.l   #-32768,d2
            bge     .lidx\@
            move.l  #-32768,d2
.lidx\@:
            move.w  d0,d7
            andi.w  #7,d7
            add.w   (a6,d7.w*2),d3
            tst.w   d3
            bge     .lmax\@
            moveq   #0,d3
            bra     .ldone\@
.lmax\@:
            cmp.w   #88,d3
            ble     .ldone\@
            move.w  #88,d3
.ldone\@:

            moveq   #0,d6
            move.w  d5,d6
            lsl.w   #3,d6
            move.l  d1,d7
            and.l   #7,d7
            add.w   d6,d7
            move.w  (a4,d7.w*2),d6

            btst    #3,d1
            beq     .radd\@
            sub.l   d6,d4
            bra     .rclamp\@
.radd\@:
            add.l   d6,d4
.rclamp\@:
            cmp.l   #32767,d4
            ble     .rmin\@
            move.l  #32767,d4
            bra     .ridx\@
.rmin\@:
            cmp.l   #-32768,d4
            bge     .ridx\@
            move.l  #-32768,d4
.ridx\@:
            move.w  d1,d7
            andi.w  #7,d7
            add.w   (a6,d7.w*2),d5
            tst.w   d5
            bge     .rmax\@
            moveq   #0,d5
            bra     .rdone\@
.rmax\@:
            cmp.w   #88,d5
            ble     .rdone\@
            move.w  #88,d5
.rdone\@:

            move.w  d2,d0
            swap    d0
            move.w  d4,d0
            move.l  d0,(a1)+
        ENDM

F_MS_PAIR MACRO
            move.l  e0,d0
            andi.l  #15,d0
            lsr.l   #4,e0
            move.l  e0,d1
            andi.l  #15,d1
            lsr.l   #4,e0

            moveq   #0,d6
            move.w  d3,d6
            lsl.w   #3,d6
            move.l  d0,d7
            and.l   #7,d7
            add.w   d6,d7
            move.w  (a4,d7.w*2),d6
            btst    #3,d0
            beq     .ladd\@
            sub.l   d6,d2
            bra     .lclamp\@
.ladd\@:
            add.l   d6,d2
.lclamp\@:
            cmp.l   #32767,d2
            ble     .lmin\@
            move.l  #32767,d2
            bra     .lidx\@
.lmin\@:
            cmp.l   #-32768,d2
            bge     .lidx\@
            move.l  #-32768,d2
.lidx\@:
            move.w  d0,d7
            andi.w  #7,d7
            add.w   (a6,d7.w*2),d3
            tst.w   d3
            bge     .lmax\@
            moveq   #0,d3
            bra     .ldone\@
.lmax\@:
            cmp.w   #88,d3
            ble     .ldone\@
            move.w  #88,d3
.ldone\@:

            moveq   #0,d6
            move.w  d5,d6
            lsl.w   #3,d6
            move.l  d1,d7
            and.l   #7,d7
            add.w   d6,d7
            move.w  (a4,d7.w*2),d6
            btst    #3,d1
            beq     .radd\@
            sub.l   d6,d4
            bra     .rclamp\@
.radd\@:
            add.l   d6,d4
.rclamp\@:
            cmp.l   #32767,d4
            ble     .rmin\@
            move.l  #32767,d4
            bra     .ridx\@
.rmin\@:
            cmp.l   #-32768,d4
            bge     .ridx\@
            move.l  #-32768,d4
.ridx\@:
            move.w  d1,d7
            andi.w  #7,d7
            add.w   (a6,d7.w*2),d5
            tst.w   d5
            bge     .rmax\@
            moveq   #0,d5
            bra     .rdone\@
.rmax\@:
            cmp.w   #88,d5
            ble     .rdone\@
            move.w  #88,d5
.rdone\@:

            move.w  d2,d0
            add.w   d4,d0
            bvc.s   .mlok\@
            tst.w   d2
            bge.s   .mlmax\@
            move.w  #-32768,d0
            bra.s   .mlok\@
.mlmax\@:
            move.w  #32767,d0
.mlok\@:
            move.w  d2,d1
            sub.w   d4,d1
            bvc.s   .mrok\@
            tst.w   d2
            bge.s   .mrmax\@
            move.w  #-32768,d1
            bra.s   .mrok\@
.mrmax\@:
            move.w  #32767,d1
.mrok\@:
            swap    d0
            move.w  d1,d0
            move.l  d0,(a1)+
        ENDM

F_MONO_NIB MACRO
            move.l  e0,d0
            andi.l  #15,d0
            lsr.l   #4,e0

            moveq   #0,d6
            move.w  d3,d6
            lsl.w   #3,d6
            move.l  d0,d7
            and.l   #7,d7
            add.w   d6,d7
            move.w  (a4,d7.w*2),d6
            btst    #3,d0
            beq     .madd\@
            sub.l   d6,d2
            bra     .mclamp\@
.madd\@:
            add.l   d6,d2
.mclamp\@:
            cmp.l   #32767,d2
            ble     .mmin\@
            move.l  #32767,d2
            bra     .midx\@
.mmin\@:
            cmp.l   #-32768,d2
            bge     .midx\@
            move.l  #-32768,d2
.midx\@:
            move.w  d0,d7
            andi.w  #7,d7
            add.w   (a6,d7.w*2),d3
            tst.w   d3
            bge     .mmax\@
            moveq   #0,d3
            bra     .mdone\@
.mmax\@:
            cmp.w   #88,d3
            ble     .mdone\@
            move.w  #88,d3
.mdone\@:
            move.w  d2,(a1)+
        ENDM

_decode_4bit_stereo_asm:
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
            lea     delta_table,a4
            lea     index_adj_table,a6
            tst.l   64(a7)
            beq     .st_done
.st_block:
            movea.l #3,a2
.st_chunk:
            movex.l (a0)+,e0
            F_PAIR
            F_PAIR
            F_PAIR
            F_PAIR
            subq.l  #1,a2
            cmpa.l  #0,a2
            bne     .st_chunk
            movea.l #3,a2
.st_byte:
            move.l  #0,e0
            move.b  (a0)+,e0
            F_PAIR
            subq.l  #1,a2
            cmpa.l  #0,a2
            bne     .st_byte
            subq.l  #1,64(a7)
            bne     .st_block
.st_done:
            move.l  56(a7),a5
            move.l  d2,(a5)
            move.w  d3,4(a5)
            move.l  60(a7),a3
            move.l  d4,(a3)
            move.w  d5,4(a3)
            movem.l (a7)+,d2-d7/a2-a6
            movec   ccc,d0
            move.l  e1,d1
            sub.l   d1,d0
            rts

_decode_4bit_ms_asm:
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
            lea     delta_table,a4
            lea     index_adj_table,a6
            tst.l   64(a7)
            beq     .ms_done
.ms_block:
            movex.l (a0)+,e0
            F_MS_PAIR
            F_MS_PAIR
            F_MS_PAIR
            F_MS_PAIR
            movex.l (a0)+,e0
            F_MS_PAIR
            F_MS_PAIR
            F_MS_PAIR
            F_MS_PAIR
            movex.l (a0)+,e0
            F_MS_PAIR
            F_MS_PAIR
            F_MS_PAIR
            F_MS_PAIR
            move.l  #0,e0
            move.b  (a0)+,e0
            F_MS_PAIR
            move.l  #0,e0
            move.b  (a0)+,e0
            F_MS_PAIR
            move.l  #0,e0
            move.b  (a0)+,e0
            F_MS_PAIR
            subq.l  #1,64(a7)
            bne     .ms_block
.ms_done:
            move.l  56(a7),a5
            move.l  d2,(a5)
            move.w  d3,4(a5)
            move.l  60(a7),a3
            move.l  d4,(a3)
            move.w  d5,4(a3)
            movem.l (a7)+,d2-d7/a2-a6
            movec   ccc,d0
            move.l  e1,d1
            sub.l   d1,d0
            rts

_decode_4bit_mono_asm:
            movem.l d2-d7/a2-a6,-(a7)
            movec   ccc,d0
            move.l  d0,e1
            move.l  48(a7),a0
            move.l  52(a7),a1
            move.l  56(a7),a5
            move.l  (a5),d2
            move.w  4(a5),d3
            ext.l   d3
            lea     delta_table,a4
            lea     index_adj_table,a6
            tst.l   60(a7)
            beq     .mo_done
.mo_block:
            movex.l (a0)+,e0
            F_MONO_NIB
            F_MONO_NIB
            F_MONO_NIB
            F_MONO_NIB
            F_MONO_NIB
            F_MONO_NIB
            F_MONO_NIB
            F_MONO_NIB
            movex.l (a0)+,e0
            F_MONO_NIB
            F_MONO_NIB
            F_MONO_NIB
            F_MONO_NIB
            F_MONO_NIB
            F_MONO_NIB
            F_MONO_NIB
            F_MONO_NIB
            movex.l (a0)+,e0
            F_MONO_NIB
            F_MONO_NIB
            F_MONO_NIB
            F_MONO_NIB
            F_MONO_NIB
            F_MONO_NIB
            F_MONO_NIB
            F_MONO_NIB
            move.l  #0,e0
            move.b  (a0)+,e0
            F_MONO_NIB
            F_MONO_NIB
            move.l  #0,e0
            move.b  (a0)+,e0
            F_MONO_NIB
            F_MONO_NIB
            move.l  #0,e0
            move.b  (a0)+,e0
            F_MONO_NIB
            F_MONO_NIB
            subq.l  #1,60(a7)
            bne     .mo_block
.mo_done:
            move.l  56(a7),a5
            move.l  d2,(a5)
            move.w  d3,4(a5)
            movem.l (a7)+,d2-d7/a2-a6
            movec   ccc,d0
            move.l  e1,d1
            sub.l   d1,d0
            rts

            section code,code
index_adj_table:
            dc.w    -1, -1, -1, -1, 2, 4, 6, 8
delta_table:
            dc.w    0,1,3,4,7,8,10,11,1,3,5,7,9,11,13,15
            dc.w    1,3,5,7,10,12,14,16,1,3,6,8,11,13,16,18
            dc.w    1,3,6,8,12,14,17,19,1,4,7,10,13,16,19,22
            dc.w    1,4,7,10,14,17,20,23,1,4,8,11,15,18,22,25
            dc.w    2,6,10,14,18,22,26,30,2,6,10,14,19,23,27,31
            dc.w    2,6,11,15,21,25,30,34,2,7,12,17,23,28,33,38
            dc.w    2,7,13,18,25,30,36,41,3,9,15,21,28,34,40,46
            dc.w    3,10,17,24,31,38,45,52,3,10,18,25,34,41,49,56
            dc.w    4,12,21,29,38,46,55,63,4,13,22,31,41,50,59,68
            dc.w    5,15,25,35,46,56,66,76,5,16,27,38,50,61,72,83
            dc.w    6,18,31,43,56,68,81,93,6,19,33,46,61,74,88,101
            dc.w    7,22,37,52,67,82,97,112,8,24,41,57,74,90,107,123
            dc.w    9,27,45,63,82,100,118,136,10,30,50,70,90,110,130,150
            dc.w    11,33,55,77,99,121,143,165,12,36,60,84,109,133,157,181
            dc.w    13,39,66,92,120,146,173,199,14,43,73,102,132,161,191,220
            dc.w    16,48,81,113,146,178,211,243,17,52,88,123,160,195,231,266
            dc.w    19,58,97,136,176,215,254,293,21,64,107,150,194,237,280,323
            dc.w    23,70,118,165,213,260,308,355,26,78,130,182,235,287,339,391
            dc.w    28,85,143,200,258,315,373,430,31,94,157,220,284,347,410,473
            dc.w    34,103,173,242,313,382,452,521,38,114,191,267,345,421,498,574
            dc.w    42,126,210,294,379,463,547,631,46,138,231,323,417,509,602,694
            dc.w    51,153,255,357,459,561,663,765,56,168,280,392,505,617,729,841
            dc.w    61,184,308,431,555,678,802,925,68,204,340,476,612,748,884,1020
            dc.w    74,223,373,522,672,821,971,1120,82,246,411,575,740,904,1069,1233
            dc.w    90,271,452,633,814,995,1176,1357,99,298,497,696,895,1094,1293,1492
            dc.w    109,328,547,766,985,1204,1423,1642,120,360,601,841,1083,1323,1564,1804
            dc.w    132,397,662,927,1192,1457,1722,1987,145,436,728,1019,1311,1602,1894,2185
            dc.w    160,480,801,1121,1442,1762,2083,2403,176,528,881,1233,1587,1939,2292,2644
            dc.w    194,582,970,1358,1746,2134,2522,2910,213,639,1066,1492,1920,2346,2773,3199
            dc.w    234,703,1173,1642,2112,2581,3051,3520,258,774,1291,1807,2324,2840,3357,3873
            dc.w    284,852,1420,1988,2556,3124,3692,4260,312,936,1561,2185,2811,3435,4060,4684
            dc.w    343,1030,1717,2404,3092,3779,4466,5153,378,1134,1890,2646,3402,4158,4914,5670
            dc.w    415,1246,2078,2909,3742,4573,5405,6236,457,1372,2287,3202,4117,5032,5947,6862
            dc.w    503,1509,2516,3522,4529,5535,6542,7548,553,1660,2767,3874,4981,6088,7195,8302
            dc.w    608,1825,3043,4260,5479,6696,7914,9131,669,2008,3348,4687,6027,7366,8706,10045
            dc.w    736,2209,3683,5156,6630,8103,9577,11050,810,2431,4052,5673,7294,8915,10536,12157
            dc.w    891,2674,4457,6240,8023,9806,11589,13372,980,2941,4902,6863,8825,10786,12747,14708
            dc.w    1078,3235,5393,7550,9708,11865,14023,16180,1186,3559,5932,8305,10679,13052,15425,17798
            dc.w    1305,3915,6526,9136,11747,14357,16968,19578,1435,4306,7178,10049,12922,15793,18665,21536
            dc.w    1579,4737,7896,11054,14214,17372,20531,23689,1737,5211,8686,12160,15636,19110,22585,26059
            dc.w    1911,5733,9555,13377,17200,21022,24844,28666,2102,6306,10511,14715,18920,23124,27329,31533
            dc.w    2312,6937,11562,16187,20812,25437,30062,34687,2543,7630,12718,17805,22893,27980,33068,38155
            dc.w    2798,8394,13990,19586,25183,30779,36375,41971,3077,9232,15388,21543,27700,33855,40011,46166
            dc.w    3385,10156,16928,23699,30471,37242,44014,50785,3724,11172,18621,26069,33518,40966,48415,55863
            dc.w    4095,12286,20478,28669,36862,45053,53245,61436

            END
