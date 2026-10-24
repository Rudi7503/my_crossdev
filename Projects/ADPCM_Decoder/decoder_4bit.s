; ==============================================================================
; EXTREME OPTIMIZED 68080 ADPCM 3-BIT DECODER SUITE
; Enthält: Stereo (L/R), Mid-Side (M/S) und Mono
; Features: Zyklenmessung (E1), 24-Bit Little-Endian Chunk-Loader,
;           EXG-Trick für schnelle D-Register-Shifts.
; ==============================================================================

            SECTION CODE,CODE
            XDEF    _decode_4bit_stereo_asm
            XDEF    _decode_4bit_ms_asm
            XDEF    _decode_4bit_mono_asm


; ==============================================================================
; 1. DECODER: 4-BIT STEREO
; ==============================================================================
; ------------------------------------------------------------------------------
; Stereo pair body: L/R nibbles -> E2/E3, deltas -> E4/E5, then apply.
; Uses E2-E5 as scratch; d6/d7 loop counters stay untouched.
; ------------------------------------------------------------------------------
ST_PAIR MACRO
            move.l  e0,d0
            andi.l  #15,d0
            move.l  d0,e2
            lsr.l   #4,e0
            move.l  e0,d0
            andi.l  #15,d0
            move.l  d0,e3
            lsr.l   #4,e0

            move.w  d3,d0
            lsl.w   #3,d0
            move.l  e2,d1
            and.l   #7,d1
            add.w   d0,d1
            move.w  (a4,d1.w*2),d1
            move.l  d1,e4

            move.w  d5,d0
            lsl.w   #3,d0
            move.l  e3,d1
            and.l   #7,d1
            add.w   d0,d1
            move.w  (a4,d1.w*2),d1
            move.l  d1,e5

            move.l  e2,d0
            move.l  e4,d1
            btst    #3,d0
            beq     .addL\@
            sub.l   d1,d2
            bra     .clampL\@
.addL\@:
            add.l   d1,d2
.clampL\@:
            cmp.l   #32767,d2
            ble     .minL\@
            move.l  #32767,d2
            bra     .idxL\@
.minL\@:
            cmp.l   #-32768,d2
            bge     .idxL\@
            move.l  #-32768,d2
.idxL\@:
            move.w  d0,d1
            andi.w  #7,d1
            add.w   (a6,d1.w*2),d3
            tst.w   d3
            bge     .idxLmax\@
            moveq   #0,d3
            bra     .idxLdone\@
.idxLmax\@:
            cmp.w   #88,d3
            ble     .idxLdone\@
            move.w  #88,d3
.idxLdone\@:

            move.l  e3,d0
            move.l  e5,d1
            btst    #3,d0
            beq     .addR\@
            sub.l   d1,d4
            bra     .clampR\@
.addR\@:
            add.l   d1,d4
.clampR\@:
            cmp.l   #32767,d4
            ble     .minR\@
            move.l  #32767,d4
            bra     .idxR\@
.minR\@:
            cmp.l   #-32768,d4
            bge     .idxR\@
            move.l  #-32768,d4
.idxR\@:
            move.w  d0,d1
            andi.w  #7,d1
            add.w   (a6,d1.w*2),d5
            tst.w   d5
            bge     .idxRmax\@
            moveq   #0,d5
            bra     .idxRdone\@
.idxRmax\@:
            cmp.w   #88,d5
            ble     .idxRdone\@
            move.w  #88,d5
.idxRdone\@:

            move.w  d2,d0
            swap    d0
            move.w  d4,d0
            move.l  d0,(a1)+
        ENDM

_decode_4bit_stereo_asm:
            movem.l d2-d7/a2-a6,-(a7)       ; 11 Register sichern (44 Bytes)

            movec   ccc,d0                  ; Zyklenmessung START
            move.l  d0,e1           

            ; Parameter-Offsets (44 Bytes gesichert + 4 Bytes PC = 48 Basis)
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

            lea     delta_table(pc),a4       ; A4 = Pointer auf Step-Tabelle
            lea     index_adj_table(pc),a6  ; A6 = Pointer auf Adjust-Tabelle

            tst.l   64(a7)                  ; Block Count check
            beq     .st_done

.st_block_loop:
            moveq   #2,d6           
.st_chunk_loop:
            movex.l (a0)+,e0                ; 32-Bit = 8 Nibbles = 4 Stereo-Paare
            moveq   #3,d7                   ; Schleife läuft 4x
.st_pair_loop_long:
            ST_PAIR
            dbf     d7,.st_pair_loop_long
            dbf     d6,.st_chunk_loop

            moveq   #2,d6                   ; 3 Bytes Rest
.st_byte_loop:
            move.l  #0,e0           
            move.b  (a0)+,e0                ; 1 Byte = 2 Nibbles = 1 Stereo-Paar
            moveq   #0,d7                   ; Schleife läuft 1x
.st_pair_loop_byte:
            ST_PAIR
            dbf     d7,.st_pair_loop_byte
            dbf     d6,.st_byte_loop

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
; 2. DECODER: 4-BIT MID-SIDE (M/S MATRIX)
; ==============================================================================
_decode_4bit_ms_asm:
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

            lea     delta_table(pc),a4       
            lea     index_adj_table(pc),a6
            
            tst.l   64(a7)                  
            beq     .ms_done

.ms_block_loop:
            moveq   #2,d6           
.ms_chunk_loop:
            movex.l (a0)+,e0        
            moveq   #3,d7                   ; 4 M/S-Paare
.ms_pair_loop_long:
            move.l  e0,d0                   ; Mid dekodieren
            andi.l  #15,d0           
            bsr     decode_primary          

            lsr.l   #4,e0                   ; Side dekodieren
            move.l  e0,d0           
            andi.l  #15,d0           
            bsr     decode_secondary        

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

            lsr.l   #4,e0           
            dbf     d7,.ms_pair_loop_long
            dbf     d6,.ms_chunk_loop

            moveq   #2,d6                   ; 3 Bytes Rest
.ms_byte_loop:
            move.l  #0,e0           
            move.b  (a0)+,e0        
            moveq   #0,d7                   ; 1 M/S Paar
.ms_pair_loop_byte:
            move.l  e0,d0
            andi.l  #15,d0
            bsr     decode_primary
            
            lsr.l   #4,e0
            move.l  e0,d0
            andi.l  #15,d0
            bsr     decode_secondary

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

            lsr.l   #4,e0
            dbf     d7,.ms_pair_loop_byte
            dbf     d6,.ms_byte_loop

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
; 3. DECODER: 4-BIT MONO
; ==============================================================================
_decode_4bit_mono_asm:
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

            lea     delta_table(pc),a4       
            lea     index_adj_table(pc),a6
            
            tst.l   60(a7)                  ; Mono block_cnt Check
            beq     .mo_done

.mo_block_loop:
            moveq   #2,d6           
.mo_chunk_loop:
            movex.l (a0)+,e0        
            moveq   #7,d7                   ; 8 Mono-Samples pro Longword
.mo_sample_loop_long:
            move.l  e0,d0           
            andi.l  #15,d0           
            bsr     decode_primary          

            move.w  d2,(a1)+                ; Schreibt 16-Bit Mono RAM
            
            lsr.l   #4,e0           
            dbf     d7,.mo_sample_loop_long
            dbf     d6,.mo_chunk_loop

            moveq   #2,d6                   ; 3 Bytes Rest
.mo_byte_loop:
            move.l  #0,e0           
            move.b  (a0)+,e0        
            moveq   #1,d7                   ; 2 Mono-Samples pro Byte
.mo_sample_loop_byte:
            move.l  e0,d0
            andi.l  #15,d0
            bsr     decode_primary

            move.w  d2,(a1)+                ; 16-Bit Mono Write

            lsr.l   #4,e0
            dbf     d7,.mo_sample_loop_byte
            dbf     d6,.mo_byte_loop

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
; ZENTRALE SUBROUTINEN (Standard 4-Bit IMA ADPCM Logik)
; ==============================================================================

; ------------------------------------------------------------------------------
; decode_primary: Arbeitet fix auf D2 (PCM) und D3 (Index)
; ------------------------------------------------------------------------------
decode_primary:
            exg     d6,a2                   ; TRICK: Wir parken D6 in A2, um D6 als Rechenregister frei zu haben!
            
                        ; exact delta = delta_table[index*8 + (nibble&7)]
            move.w  d3,d6
            lsl.w   #3,d6
            move.l  d0,d1
            and.l   #7,d1
            add.w   d6,d1
            move.w  (a4,d1.w*2),d1
            btst    #3,d0           
            beq     .add_delta_P
            sub.l   d1,d2                   ; Negativ: PCM -= delta
            bra     .clamp_pcm_P    
.add_delta_P:
            add.l   d1,d2                   ; Positiv: PCM += delta

.clamp_pcm_P:
            cmp.l   #32767,d2       
            ble     .check_min_P    
            move.l  #32767,d2       
            bra     .update_idx_P   
.check_min_P:
            cmp.l   #-32768,d2      
            bge     .update_idx_P   
            move.l  #-32768,d2      

.update_idx_P:
            move.w  d0,d1                   ; d1 = Nibble
            andi.w  #7,d1                   ; Sign-Bit maskieren (Amplitude 0-7)
            move.w  (a6,d1.w*2),d1          ; Index Adjust aus Tabelle laden
            add.w   d1,d3                   ; Index += Adjust

            tst.w   d3              
            bge     .idx_max_P      
            moveq   #0,d3           
            bra     .idx_done_P                     
.idx_max_P:
            cmp.w   #88,d3          
            ble     .idx_done_P     
            move.w  #88,d3          
.idx_done_P:
            exg     d6,a2                   ; TRICK: Loop-Counter (D6) wiederherstellen!
            rts                     

; ------------------------------------------------------------------------------
; decode_secondary: Arbeitet fix auf D4 (PCM) und D5 (Index)
; ------------------------------------------------------------------------------
decode_secondary:
            exg     d6,a2                   ; TRICK: D6 für Mathe freimachen
            
                        ; exact delta = delta_table[index*8 + (nibble&7)]
            move.w  d5,d6
            lsl.w   #3,d6
            move.l  d0,d1
            and.l   #7,d1
            add.w   d6,d1
            move.w  (a4,d1.w*2),d1
            btst    #3,d0           
            beq     .add_delta_S
            sub.l   d1,d4           
            bra     .clamp_pcm_S    
.add_delta_S:
            add.l   d1,d4           

.clamp_pcm_S:
            cmp.l   #32767,d4       
            ble     .check_min_S    
            move.l  #32767,d4       
            bra     .update_idx_S   
.check_min_S:
            cmp.l   #-32768,d4      
            bge     .update_idx_S   
            move.l  #-32768,d4      

.update_idx_S:
            move.w  d0,d1
            andi.w  #7,d1
            move.w  (a6,d1.w*2),d1  
            add.w   d1,d5           

            tst.w   d5              
            bge     .idx_max_S      
            moveq   #0,d5           
            bra     .idx_done_S                     
.idx_max_S:
            cmp.w   #88,d5          
            ble     .idx_done_S     
            move.w  #88,d5          
.idx_done_S:
            exg     d6,a2                   ; TRICK: Loop-Counter wiederherstellen!
            rts                     

; ------------------------------------------------------------------------------
; STATISCHE TABELLEN (Shared)
; ------------------------------------------------------------------------------

; ADPCM 4-Bit Index Adjustment Table (für Amplituden 0 bis 7)
index_adj_table:
            dc.w    -1, -1, -1, -1, 2, 4, 6, 8

; ADPCM Step Table
step_table:
            dc.w    7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31
            dc.w    34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143
            dc.w    157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658
            dc.w    724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024
            dc.w    3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899
            dc.w    15289,16818,18500,20350,22385,24623,27086,29794,32767
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