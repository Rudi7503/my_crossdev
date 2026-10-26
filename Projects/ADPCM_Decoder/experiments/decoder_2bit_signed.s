
; Signed 2-bit decode macros, one code in d0 (0..3), uses only d1 as scratch
DEC_L MACRO
            move.w  d3,d1
            lsl.w   #2,d1
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
            add.w   (a2,d0.w*2),d3
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

DEC_R MACRO
            move.w  d5,d1
            lsl.w   #2,d1
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
            add.w   (a2,d0.w*2),d5
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

DEC_M MACRO
            move.w  d3,d1
            lsl.w   #2,d1
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
            add.w   (a2,d0.w*2),d3
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
; EXTREME OPTIMIZED 68080 ADPCM 2-BIT DECODER SUITE
; Enthält: Stereo (L/R), Mid-Side (M/S) und Mono
; Features: Zyklenmessung (E1), Scaled Indexing, D-Register State-Caching,
;           Shared Subroutines (L1-I-Cache optimiert).
; ==============================================================================

            SECTION CODE,CODE

            XDEF    _get_ccc
            XDEF    _decode_2bit_stereo_asm
            XDEF    _decode_2bit_ms_asm
            XDEF    _decode_2bit_mono_asm

; ==============================================================================
; TOOL: get_ccc (Gibt aktuelle Taktzyklen zurück)
; ==============================================================================
_get_ccc:
            movec   ccc,d0
            rts

; ==============================================================================
; 1. DECODER: 2-BIT STEREO
; ==============================================================================
_decode_2bit_stereo_asm:
            movem.l d2-d7/a2-a5,-(a7)       ; 10 Register sichern (40 Bytes)

            movec   ccc,d0                  ; Zyklenmessung START
            move.l  d0,e1           

            move.l  44(a7),a0               ; A0 = in_buffer
            move.l  48(a7),a1               ; A1 = out_buffer
            move.l  52(a7),a5               ; A5 = chL
            move.l  56(a7),a3               ; A3 = chR

            move.l  (a5),d2                 ; D2 = PCM Links
            move.w  4(a5),d3        
            ext.l   d3                      ; D3 = Index Links

            move.l  (a3),d4                 ; D4 = PCM Rechts
            move.w  4(a3),d5        
            ext.l   d5                      ; D5 = Index Rechts

            lea     delta2_table(pc),a4
            lea     idx2_table(pc),a2       ; A4 = Tabelle
            tst.l   60(a7)                  ; Block Count check
            beq     .st_done

.st_block_loop:
            moveq   #2,d6           
.st_chunk_loop:
            movex.l (a0)+,e0                ; 12 Bytes (3x Long) = 24 Samples
            moveq   #7,d7           
.st_pair_loop_long:
            move.l  e0,d0                   ; Links dekodieren
            andi.l  #3,d0           
            DEC_L          

            lsr.l   #2,e0                   ; Rechts dekodieren
            move.l  e0,d0           
            andi.l  #3,d0           
            DEC_R        

            move.w  d2,d1                   ; 32-Bit Packing [Links | Rechts]
            swap    d1              
            move.w  d4,d1           
            move.l  d1,(a1)+        

            lsr.l   #2,e0           
            dbf     d7,.st_pair_loop_long
            dbf     d6,.st_chunk_loop

            moveq   #2,d6                   ; 3 Bytes Rest
.st_byte_loop:
            move.l  #0,e0           
            move.b  (a0)+,e0        
            moveq   #1,d7           
.st_pair_loop_byte:
            move.l  e0,d0
            andi.l  #3,d0
            DEC_L

            lsr.l   #2,e0
            move.l  e0,d0
            andi.l  #3,d0
            DEC_R

            move.w  d2,d1
            swap    d1
            move.w  d4,d1
            move.l  d1,(a1)+

            lsr.l   #2,e0
            dbf     d7,.st_pair_loop_byte
            dbf     d6,.st_byte_loop

            subq.l  #1,60(a7)               ; block_cnt--
            bne     .st_block_loop

.st_done:
            move.l  52(a7),a5               ; Status in RAM zurückschreiben
            move.l  d2,(a5)         
            move.w  d3,4(a5)        
            move.l  56(a7),a3       
            move.l  d4,(a3)         
            move.w  d5,4(a3)        

            movem.l (a7)+,d2-d7/a2-a5       ; Register wiederherstellen
            
            movec   ccc,d0                  ; Zyklenmessung ENDE
            move.l  e1,d1           
            sub.l   d1,d0                   ; Return Cycles in D0
            rts


; ==============================================================================
; 2. DECODER: 2-BIT MID-SIDE (M/S MATRIX)
; ==============================================================================
_decode_2bit_ms_asm:
            movem.l d2-d7/a2-a5,-(a7)

            movec   ccc,d0                  
            move.l  d0,e1           

            move.l  44(a7),a0               ; A0 = in_buffer
            move.l  48(a7),a1               ; A1 = out_buffer
            move.l  52(a7),a5               ; A5 = chMid
            move.l  56(a7),a3               ; A3 = chSide

            move.l  (a5),d2                 ; D2 = PCM Mid
            move.w  4(a5),d3        
            ext.l   d3                      ; D3 = Index Mid

            move.l  (a3),d4                 ; D4 = PCM Side
            move.w  4(a3),d5        
            ext.l   d5                      ; D5 = Index Side

            lea     delta2_table(pc),a4
            lea     idx2_table(pc),a2       
            tst.l   60(a7)                  
            beq     .ms_done

.ms_block_loop:
            moveq   #2,d6           
.ms_chunk_loop:
            movex.l (a0)+,e0        
            moveq   #7,d7           
.ms_pair_loop_long:
            move.l  e0,d0                   ; Mid dekodieren
            andi.l  #3,d0           
            DEC_L          

            lsr.l   #2,e0                   ; Side dekodieren
            move.l  e0,d0           
            andi.l  #3,d0           
            DEC_R        

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

            lsr.l   #2,e0           
            dbf     d7,.ms_pair_loop_long
            dbf     d6,.ms_chunk_loop

            moveq   #2,d6                   ; 3 Bytes Rest
.ms_byte_loop:
            move.l  #0,e0           
            move.b  (a0)+,e0        
            moveq   #1,d7           
.ms_pair_loop_byte:
            move.l  e0,d0
            andi.l  #3,d0
            DEC_L
            
            lsr.l   #2,e0
            move.l  e0,d0
            andi.l  #3,d0
            DEC_R

            move.l  d2,d0           
            add.l   d4,d0                   ; D0 = Links
            move.l  d2,d1           
            sub.l   d4,d1                   ; D1 = Rechts

            cmp.l   #32767,d0
            ble     .ms_chk_min_L2
            move.l  #32767,d0
            bra     .ms_clamp_R2
.ms_chk_min_L2:
            cmp.l   #-32768,d0
            bge     .ms_clamp_R2
            move.l  #-32768,d0
.ms_clamp_R2:
            cmp.l   #32767,d1
            ble     .ms_chk_min_R2
            move.l  #32767,d1
            bra     .ms_pack2
.ms_chk_min_R2:
            cmp.l   #-32768,d1
            bge     .ms_pack2
            move.l  #-32768,d1
.ms_pack2:
            swap    d0
            move.w  d1,d0
            move.l  d0,(a1)+

            lsr.l   #2,e0
            dbf     d7,.ms_pair_loop_byte
            dbf     d6,.ms_byte_loop

            subq.l  #1,60(a7)
            bne     .ms_block_loop

.ms_done:
            move.l  52(a7),a5               ; M/S Status ins RAM zurück
            move.l  d2,(a5)         
            move.w  d3,4(a5)        
            move.l  56(a7),a3       
            move.l  d4,(a3)         
            move.w  d5,4(a3)        

            movem.l (a7)+,d2-d7/a2-a5       
            
            movec   ccc,d0                  ; Cycles berechnen
            move.l  e1,d1           
            sub.l   d1,d0                   
            rts


; ==============================================================================
; 3. DECODER: 2-BIT MONO
; ==============================================================================
_decode_2bit_mono_asm:
            movem.l d2-d7/a2-a5,-(a7)       ; Push (Konsistenz zu Stereo)

            movec   ccc,d0                  
            move.l  d0,e1           

            ; ACHTUNG: Mono hat einen Parameter weniger! Offsets verschieben sich.
            move.l  44(a7),a0               ; A0 = in_buffer
            move.l  48(a7),a1               ; A1 = out_buffer
            move.l  52(a7),a5               ; A5 = chMono
            ; block_cnt liegt bei Mono auf 56(a7)!

            move.l  (a5),d2                 ; D2 = PCM Mono
            move.w  4(a5),d3        
            ext.l   d3                      ; D3 = Index Mono

            lea     delta2_table(pc),a4
            lea     idx2_table(pc),a2       
            tst.l   56(a7)                  ; Mono block_cnt Check
            beq     .mo_done

.mo_block_loop:
            moveq   #2,d6           
.mo_chunk_loop:
            movex.l (a0)+,e0        
            moveq   #15,d7                  ; 16 Mono-Samples pro Longword!
.mo_sample_loop_long:
            move.l  e0,d0           
            andi.l  #3,d0           
            DEC_M          ; Dekodieren

            move.w  d2,(a1)+                ; Schreibt 16-Bit Mono RAM
            
            lsr.l   #2,e0           
            dbf     d7,.mo_sample_loop_long
            dbf     d6,.mo_chunk_loop

            moveq   #2,d6                   ; 3 Bytes Rest
.mo_byte_loop:
            move.l  #0,e0           
            move.b  (a0)+,e0        
            moveq   #3,d7                   ; 4 Mono-Samples pro Byte!
.mo_sample_loop_byte:
            move.l  e0,d0
            andi.l  #3,d0
            DEC_M

            move.w  d2,(a1)+                ; 16-Bit Mono Write

            lsr.l   #2,e0
            dbf     d7,.mo_sample_loop_byte
            dbf     d6,.mo_byte_loop

            subq.l  #1,56(a7)               ; block_cnt--
            bne     .mo_block_loop

.mo_done:
            move.l  52(a7),a5               ; Mono Status ins RAM zurück
            move.l  d2,(a5)         
            move.w  d3,4(a5)        

            movem.l (a7)+,d2-d7/a2-a5       
            
            movec   ccc,d0                  ; Cycles berechnen
            move.l  e1,d1           
            sub.l   d1,d0                   
            rts


; ==============================================================================
; ZENTRALE SUBROUTINEN (Von allen Decodern geteilt)
; ==============================================================================

; ------------------------------------------------------------------------------
;

            even
delta2_table:
            dc.l    3,10,-3,-10,4,12,-4,-12
            dc.l    4,13,-4,-13,5,15,-5,-15
            dc.l    5,16,-5,-16,6,18,-6,-18
            dc.l    6,19,-6,-19,7,21,-7,-21
            dc.l    8,24,-8,-24,8,25,-8,-25
            dc.l    9,28,-9,-28,10,31,-10,-31
            dc.l    11,34,-11,-34,12,37,-12,-37
            dc.l    14,42,-14,-42,15,46,-15,-46
            dc.l    17,51,-17,-51,18,55,-18,-55
            dc.l    20,61,-20,-61,22,67,-22,-67
            dc.l    25,75,-25,-75,27,82,-27,-82
            dc.l    30,90,-30,-90,33,99,-33,-99
            dc.l    36,109,-36,-109,40,120,-40,-120
            dc.l    44,132,-44,-132,48,145,-48,-145
            dc.l    53,160,-53,-160,59,177,-59,-177
            dc.l    65,195,-65,-195,71,214,-71,-214
            dc.l    78,235,-78,-235,86,259,-86,-259
            dc.l    95,285,-95,-285,104,313,-104,-313
            dc.l    115,345,-115,-345,126,379,-126,-379
            dc.l    139,418,-139,-418,153,460,-153,-460
            dc.l    168,505,-168,-505,185,556,-185,-556
            dc.l    204,612,-204,-612,224,673,-224,-673
            dc.l    247,741,-247,-741,272,816,-272,-816
            dc.l    299,897,-299,-897,329,987,-329,-987
            dc.l    362,1086,-362,-1086,398,1194,-398,-1194
            dc.l    438,1314,-438,-1314,481,1444,-481,-1444
            dc.l    530,1590,-530,-1590,583,1749,-583,-1749
            dc.l    641,1923,-641,-1923,705,2116,-705,-2116
            dc.l    776,2328,-776,-2328,853,2560,-853,-2560
            dc.l    939,2817,-939,-2817,1033,3099,-1033,-3099
            dc.l    1136,3408,-1136,-3408,1249,3748,-1249,-3748
            dc.l    1374,4123,-1374,-4123,1512,4536,-1512,-4536
            dc.l    1663,4990,-1663,-4990,1830,5490,-1830,-5490
            dc.l    2013,6039,-2013,-6039,2214,6642,-2214,-6642
            dc.l    2435,7306,-2435,-7306,2679,8037,-2679,-8037
            dc.l    2947,8841,-2947,-8841,3242,9726,-3242,-9726
            dc.l    3566,10698,-3566,-10698,3922,11767,-3922,-11767
            dc.l    4315,12945,-4315,-12945,4746,14239,-4746,-14239
            dc.l    5221,15663,-5221,-15663,5743,17230,-5743,-17230
            dc.l    6317,18952,-6317,-18952,6949,20848,-6949,-20848
            dc.l    7644,22933,-7644,-22933,8409,25227,-8409,-25227
            dc.l    9250,27750,-9250,-27750,10175,30525,-10175,-30525
            dc.l    11192,33577,-11192,-33577,12311,36934,-12311,-36934
            dc.l    13543,40629,-13543,-40629,14897,44691,-14897,-44691
            dc.l    16383,49150,-16383,-49150
idx2_table:
            dc.w    -1,2,-1,2
