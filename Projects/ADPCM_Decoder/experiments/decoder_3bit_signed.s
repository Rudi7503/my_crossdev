
; Signed 3-bit decode macros; code in d0 (0..7), index state d3/d5, uses only d1 as scratch
DEC_L3 MACRO
            move.w  d3,d1
            lsl.w   #3,d1
            add.w   d0,d1
            move.l  (a4,d1.w*4),d1
            add.l   d1,d2
            cmp.l   #32767,d2
            ble.s   .lmin\@
            move.l  #32767,d2
            bra.s   .lq\@
.lmin\@:
            cmp.l   #-32768,d2
            bge.s   .lq\@
            move.l  #-32768,d2
.lq\@:
            andi.w  #3,d0
            add.w   (a6,d0.w*2),d3
            tst.w   d3
            bge.s   .lmax\@
            moveq   #0,d3
            bra.s   .li\@
.lmax\@:
            cmp.w   #88,d3
            ble.s   .li\@
            move.w  #88,d3
.li\@:
        ENDM
DEC_R3 MACRO
            move.w  d5,d1
            lsl.w   #3,d1
            add.w   d0,d1
            move.l  (a4,d1.w*4),d1
            add.l   d1,d4
            cmp.l   #32767,d4
            ble.s   .rmin\@
            move.l  #32767,d4
            bra.s   .rq\@
.rmin\@:
            cmp.l   #-32768,d4
            bge.s   .rq\@
            move.l  #-32768,d4
.rq\@:
            andi.w  #3,d0
            add.w   (a6,d0.w*2),d5
            tst.w   d5
            bge.s   .rmax\@
            moveq   #0,d5
            bra.s   .ri\@
.rmax\@:
            cmp.w   #88,d5
            ble.s   .ri\@
            move.w  #88,d5
.ri\@:
        ENDM
DEC_M3 MACRO
            move.w  d3,d1
            lsl.w   #3,d1
            add.w   d0,d1
            move.l  (a4,d1.w*4),d1
            add.l   d1,d2
            cmp.l   #32767,d2
            ble.s   .mmin\@
            move.l  #32767,d2
            bra.s   .mq\@
.mmin\@:
            cmp.l   #-32768,d2
            bge.s   .mq\@
            move.l  #-32768,d2
.mq\@:
            andi.w  #3,d0
            add.w   (a6,d0.w*2),d3
            tst.w   d3
            bge.s   .mmax\@
            moveq   #0,d3
            bra.s   .mi\@
.mmax\@:
            cmp.w   #88,d3
            ble.s   .mi\@
            move.w  #88,d3
.mi\@:
        ENDM

; ==============================================================================
; EXTREME OPTIMIZED 68080 ADPCM 3-BIT DECODER SUITE
; Enthält: Stereo (L/R), Mid-Side (M/S) und Mono
; Features: Zyklenmessung (E1), 32-Bit Load / 24-Bit Process Trick,
;           Unaligned Access (addq.l #3, a0), EXG-Trick für Shifts.
; ==============================================================================

            SECTION CODE,CODE
            XDEF    _decode_3bit_stereo_asm
            XDEF    _decode_3bit_ms_asm
            XDEF    _decode_3bit_mono_asm

; ==============================================================================
; 1. DECODER: 3-BIT STEREO
; ==============================================================================
_decode_3bit_stereo_asm:
            movem.l d2-d7/a2-a6,-(a7)       ; 11 Register sichern (44 Bytes)

            movec   ccc,d0                  ; Zyklenmessung START
            move.l  d0,e1           

            ; Parameter (44 Bytes Offset + 4 Bytes PC = 48 Basis)
            move.l  48(a7),a0               ; A0 = in_buffer
            move.l  52(a7),a1               ; A1 = out_buffer
            move.l  56(a7),a5               ; A5 = chL
            move.l  60(a7),a3               ; A3 = chR
            ; 64(a7) = block_cnt

            move.l  (a5),d2                 ; D2 = PCM Links
            move.w  4(a5),d3        
            ext.l   d3                      ; D3 = Index Links

            move.l  (a3),d4                 ; D4 = PCM Rechts
            move.w  4(a3),d5        
            ext.l   d5                      ; D5 = Index Rechts

            lea     delta3_table(pc),a4       ; A4 = Pointer auf Step-Tabelle
            lea     index_adj_3bit(pc),a6   ; A6 = Pointer auf Adjust-Tabelle

            tst.l   64(a7)                  ; Block Count check
            beq     .st_done

.st_block_loop:
            ; 15 Bytes Block = 5 Durchläufe à 24-Bit (3 Bytes)
            moveq   #4,d6                   ; 5 Durchläufe (4 bis 0)
.st_chunk_loop:
            ; --- DER 32-BIT LOAD / 24-BIT PROCESS TRICK ---
            movex.l (a0),e0                 ; 32-Bit lesen (inkl. Endian Swap)
            addq.l  #3,a0                   ; Lesezeiger exakt 3 Bytes (24 Bits) weiterrücken!

            ; 24 Bits = 4 Stereo-Paare (4 x 6 Bits)
            moveq   #3,d7                   ; Schleife läuft 4x
.st_pair_loop:
            move.l  e0,d0                   ; Links dekodieren
            andi.l  #7,d0                   ; 3-Bit Maske (Werte 0-7)
            DEC_L3          

            lsr.l   #3,e0                   ; Bitstream 3 Bits weiter schieben
            
            move.l  e0,d0                   ; Rechts dekodieren
            andi.l  #7,d0           
            DEC_R3        

            move.w  d2,d1                   ; 32-Bit Packing [Links | Rechts]
            swap    d1              
            move.w  d4,d1           
            move.l  d1,(a1)+                ; Ins RAM flashen

            lsr.l   #3,e0                   ; Bitstream 3 Bits weiter schieben
            dbf     d7,.st_pair_loop
            
            dbf     d6,.st_chunk_loop       
            
            subq.l  #1,64(a7)               ; block_cnt--
            bne     .st_block_loop

.st_done:
            move.l  56(a7),a5               ; Status in RAM zurückschreiben
            move.l  d2,(a5)         
            move.w  d3,4(a5)        
            move.l  60(a7),a3       
            move.l  d4,(a3)         
            move.w  d5,4(a3)        

            movem.l (a7)+,d2-d7/a2-a6       ; Register wiederherstellen
            
            movec   ccc,d0                  ; Zyklenmessung ENDE
            move.l  e1,d1           
            sub.l   d1,d0                   ; Return Cycles in D0
            rts


; ==============================================================================
; 2. DECODER: 3-BIT MID-SIDE (M/S MATRIX)
; ==============================================================================
_decode_3bit_ms_asm:
            movem.l d2-d7/a2-a6,-(a7)

            movec   ccc,d0                  
            move.l  d0,e1           

            move.l  48(a7),a0               ; A0 = in_buffer
            move.l  52(a7),a1               ; A1 = out_buffer
            move.l  56(a7),a5               ; A5 = chMid
            move.l  60(a7),a3               ; A3 = chSide

            move.l  (a5),d2                 ; D2 = PCM Mid
            move.w  4(a5),d3        
            ext.l   d3                      ; D3 = Index Mid

            move.l  (a3),d4                 ; D4 = PCM Side
            move.w  4(a3),d5        
            ext.l   d5                      ; D5 = Index Side

            lea     delta3_table(pc),a4       
            lea     index_adj_3bit(pc),a6
            
            tst.l   64(a7)                  
            beq     .ms_done

.ms_block_loop:
            moveq   #4,d6                   ; 5 Chunks à 24-Bit (3 Bytes)
.ms_chunk_loop:
            movex.l (a0),e0                 ; 32-Bit lesen
            addq.l  #3,a0                   ; Pointer um 24-Bit anpassen

            moveq   #3,d7                   ; 4 M/S-Paare (24 Bits)
.ms_pair_loop:
            move.l  e0,d0                   ; Mid dekodieren
            andi.l  #7,d0           
            DEC_L3          

            lsr.l   #3,e0                   ; Side dekodieren
            move.l  e0,d0           
            andi.l  #7,d0           
            DEC_R3        

            ; --- MATRIX (L = Mid+Side, R = Mid-Side) ---
            move.l  d2,d0           
            add.l   d4,d0                   ; D0 = Links
            move.l  d2,d1           
            sub.l   d4,d1                   ; D1 = Rechts

            ; Clamping Links (D0)
            cmp.l   #32767,d0
            ble     .ms_chk_min_L
            move.l  #32767,d0
            bra     .ms_clamp_R
.ms_chk_min_L:
            cmp.l   #-32768,d0
            bge     .ms_clamp_R
            move.l  #-32768,d0
            
.ms_clamp_R:
            ; Clamping Rechts (D1)
            cmp.l   #32767,d1
            ble     .ms_chk_min_R
            move.l  #32767,d1
            bra     .ms_pack
.ms_chk_min_R:
            cmp.l   #-32768,d1
            bge     .ms_pack
            move.l  #-32768,d1

.ms_pack:
            swap    d0                      ; 32-Bit Packing
            move.w  d1,d0           
            move.l  d0,(a1)+        

            lsr.l   #3,e0           
            dbf     d7,.ms_pair_loop
            dbf     d6,.ms_chunk_loop

            subq.l  #1,64(a7)
            bne     .ms_block_loop

.ms_done:
            move.l  56(a7),a5               ; M/S Status ins RAM zurück
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


; ==============================================================================
; 3. DECODER: 3-BIT MONO
; ==============================================================================
_decode_3bit_mono_asm:
            movem.l d2-d7/a2-a6,-(a7)       

            movec   ccc,d0                  
            move.l  d0,e1           

            ; Mono hat nur 3 Pointer (in, out, chMono), Offset verschiebt sich
            move.l  48(a7),a0               ; A0 = in_buffer
            move.l  52(a7),a1               ; A1 = out_buffer
            move.l  56(a7),a5               ; A5 = chMono
            ; 60(a7) = block_cnt

            move.l  (a5),d2                 ; D2 = PCM Mono
            move.w  4(a5),d3        
            ext.l   d3                      ; D3 = Index Mono

            lea     delta3_table(pc),a4       
            lea     index_adj_3bit(pc),a6
            
            tst.l   60(a7)                  ; Mono block_cnt Check
            beq     .mo_done

.mo_block_loop:
            moveq   #4,d6                   ; 5 Chunks à 24-Bit (3 Bytes)
.mo_chunk_loop:
            movex.l (a0),e0                 ; 32-Bit lesen
            addq.l  #3,a0                   ; Pointer um 24-Bit anpassen

            moveq   #7,d7                   ; 8 Mono-Samples (8 * 3 = 24 Bits)
.mo_sample_loop:
            move.l  e0,d0           
            andi.l  #7,d0                   ; 3-Bit Maske
            DEC_M3          

            move.w  d2,(a1)+                ; Schreibt 16-Bit Mono RAM
            
            lsr.l   #3,e0                   ; 3 Bits weiter schieben
            dbf     d7,.mo_sample_loop
            dbf     d6,.mo_chunk_loop

            subq.l  #1,60(a7)               ; block_cnt--
            bne     .mo_block_loop

.mo_done:
            move.l  56(a7),a5               ; Mono Status ins RAM zurück
            move.l  d2,(a5)         
            move.w  d3,4(a5)        

            movem.l (a7)+,d2-d7/a2-a6       
            
            movec   ccc,d0                  
            move.l  e1,d1           
            sub.l   d1,d0                   
            rts


; ==============================================================================
; ZENTRALE SUBROUTINEN (Spezifische 3-Bit IMA ADPCM Logik)
; ==============================================================================

; ------------------------------------------------------------------------------
;

            even
delta3_table:
            dc.l    1,4,8,11,-1,-4,-8,-11
            dc.l    2,6,10,14,-2,-6,-10,-14
            dc.l    2,6,11,15,-2,-6,-11,-15
            dc.l    2,7,12,17,-2,-7,-12,-17
            dc.l    2,7,13,18,-2,-7,-13,-18
            dc.l    3,9,15,21,-3,-9,-15,-21
            dc.l    3,9,16,22,-3,-9,-16,-22
            dc.l    3,10,17,24,-3,-10,-17,-24
            dc.l    4,12,20,28,-4,-12,-20,-28
            dc.l    4,12,21,29,-4,-12,-21,-29
            dc.l    4,13,23,32,-4,-13,-23,-32
            dc.l    5,15,26,36,-5,-15,-26,-36
            dc.l    5,16,28,39,-5,-16,-28,-39
            dc.l    6,18,31,43,-6,-18,-31,-43
            dc.l    7,21,35,49,-7,-21,-35,-49
            dc.l    7,22,38,53,-7,-22,-38,-53
            dc.l    8,25,42,59,-8,-25,-42,-59
            dc.l    9,27,46,64,-9,-27,-46,-64
            dc.l    10,30,51,71,-10,-30,-51,-71
            dc.l    11,33,56,78,-11,-33,-56,-78
            dc.l    12,37,62,87,-12,-37,-62,-87
            dc.l    13,40,68,95,-13,-40,-68,-95
            dc.l    15,45,75,105,-15,-45,-75,-105
            dc.l    16,49,82,115,-16,-49,-82,-115
            dc.l    18,54,91,127,-18,-54,-91,-127
            dc.l    20,60,100,140,-20,-60,-100,-140
            dc.l    22,66,110,154,-22,-66,-110,-154
            dc.l    24,72,121,169,-24,-72,-121,-169
            dc.l    26,79,133,186,-26,-79,-133,-186
            dc.l    29,88,147,206,-29,-88,-147,-206
            dc.l    32,97,162,227,-32,-97,-162,-227
            dc.l    35,106,178,249,-35,-106,-178,-249
            dc.l    39,117,196,274,-39,-117,-196,-274
            dc.l    43,129,216,302,-43,-129,-216,-302
            dc.l    47,142,237,332,-47,-142,-237,-332
            dc.l    52,156,261,365,-52,-156,-261,-365
            dc.l    57,172,287,402,-57,-172,-287,-402
            dc.l    63,189,316,442,-63,-189,-316,-442
            dc.l    69,208,348,487,-69,-208,-348,-487
            dc.l    76,229,383,536,-76,-229,-383,-536
            dc.l    84,252,421,589,-84,-252,-421,-589
            dc.l    92,277,463,648,-92,-277,-463,-648
            dc.l    102,306,510,714,-102,-306,-510,-714
            dc.l    112,336,561,785,-112,-336,-561,-785
            dc.l    123,370,617,864,-123,-370,-617,-864
            dc.l    136,408,680,952,-136,-408,-680,-952
            dc.l    149,448,747,1046,-149,-448,-747,-1046
            dc.l    164,493,822,1151,-164,-493,-822,-1151
            dc.l    181,543,905,1267,-181,-543,-905,-1267
            dc.l    199,597,995,1393,-199,-597,-995,-1393
            dc.l    219,657,1095,1533,-219,-657,-1095,-1533
            dc.l    240,721,1203,1684,-240,-721,-1203,-1684
            dc.l    265,795,1325,1855,-265,-795,-1325,-1855
            dc.l    291,874,1457,2040,-291,-874,-1457,-2040
            dc.l    320,961,1602,2243,-320,-961,-1602,-2243
            dc.l    352,1057,1763,2468,-352,-1057,-1763,-2468
            dc.l    388,1164,1940,2716,-388,-1164,-1940,-2716
            dc.l    426,1279,2133,2986,-426,-1279,-2133,-2986
            dc.l    469,1408,2347,3286,-469,-1408,-2347,-3286
            dc.l    516,1549,2582,3615,-516,-1549,-2582,-3615
            dc.l    568,1704,2840,3976,-568,-1704,-2840,-3976
            dc.l    624,1873,3123,4372,-624,-1873,-3123,-4372
            dc.l    687,2061,3436,4810,-687,-2061,-3436,-4810
            dc.l    756,2268,3780,5292,-756,-2268,-3780,-5292
            dc.l    831,2494,4158,5821,-831,-2494,-4158,-5821
            dc.l    915,2745,4575,6405,-915,-2745,-4575,-6405
            dc.l    1006,3019,5032,7045,-1006,-3019,-5032,-7045
            dc.l    1107,3321,5535,7749,-1107,-3321,-5535,-7749
            dc.l    1217,3652,6088,8523,-1217,-3652,-6088,-8523
            dc.l    1339,4018,6697,9376,-1339,-4018,-6697,-9376
            dc.l    1473,4420,7367,10314,-1473,-4420,-7367,-10314
            dc.l    1621,4863,8105,11347,-1621,-4863,-8105,-11347
            dc.l    1783,5349,8915,12481,-1783,-5349,-8915,-12481
            dc.l    1961,5883,9806,13728,-1961,-5883,-9806,-13728
            dc.l    2157,6472,10787,15102,-2157,-6472,-10787,-15102
            dc.l    2373,7119,11866,16612,-2373,-7119,-11866,-16612
            dc.l    2610,7831,13052,18273,-2610,-7831,-13052,-18273
            dc.l    2871,8614,14358,20101,-2871,-8614,-14358,-20101
            dc.l    3158,9475,15793,22110,-3158,-9475,-15793,-22110
            dc.l    3474,10423,17373,24322,-3474,-10423,-17373,-24322
            dc.l    3822,11466,19111,26755,-3822,-11466,-19111,-26755
            dc.l    4204,12613,21022,29431,-4204,-12613,-21022,-29431
            dc.l    4625,13875,23125,32375,-4625,-13875,-23125,-32375
            dc.l    5087,15262,25437,35612,-5087,-15262,-25437,-35612
            dc.l    5596,16788,27981,39173,-5596,-16788,-27981,-39173
            dc.l    6155,18466,30778,43089,-6155,-18466,-30778,-43089
            dc.l    6771,20314,33857,47400,-6771,-20314,-33857,-47400
            dc.l    7448,22345,37242,52139,-7448,-22345,-37242,-52139
            dc.l    8191,24574,40958,57341,-8191,-24574,-40958,-57341
index_adj_3bit:
            dc.w    -1,-1,1,2
