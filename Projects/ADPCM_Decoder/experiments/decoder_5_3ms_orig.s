; ==============================================================================
; EXTREME OPTIMIZED 68080 ADPCM 5/3-BIT HYBRID M/S DECODER
; 1 Byte = 1 Paar (5 Bits Mid + 3 Bits Side). Perfekt Byte-Aligned!
; LSB-First: Untere 5 Bits = Mid, Obere 3 Bits = Side.
; ==============================================================================

            SECTION CODE,CODE
            
            XDEF    _decode_5_3bit_ms_asm

; ==============================================================================
; DECODER: 5/3-BIT MID-SIDE (M/S MATRIX)
; ==============================================================================
_decode_5_3bit_ms_asm:
            movem.l d2-d7/a2-a6,-(a7)

            movec   ccc,d0
            move.l  d0,e2           

            move.l  48(a7),a0               ; A0 = in_buffer
            move.l  52(a7),a1               ; A1 = out_buffer
            move.l  56(a7),a5               ; A5 = ch Mid
            move.l  60(a7),a3               ; A3 = ch Side

            move.l  (a5),d2                 ; D2 = PCM Mid
            move.w  4(a5),d3        
            ext.l   d3                      ; D3 = Index Mid

            move.l  (a3),d4                 ; D4 = PCM Side
            move.w  4(a3),d5        
            ext.l   d5                      ; D5 = Index Side

            lea     step_table(pc),a4       
            lea     index_adj_5bit(pc),a5   ; A5 = Tabelle Mid (5-Bit)
            lea     index_adj_3bit(pc),a6   ; A6 = Tabelle Side (3-Bit)

            move.l  64(a7),d6               ; D6 = Anzahl M/S-Paare (block_cnt)
            beq     .ms_done

            ; -----------------------------------------------------------
            ; PHASE 1: 32-Bit Blöcke (4 Bytes = 4 Paare gleichzeitig)
            ; -----------------------------------------------------------
            move.l  d6,d7
            lsr.l   #2,d7                   ; D7 = block_cnt / 4
            beq     .ms_byte_loop_init

.ms_chunk_loop:
            movex.l (a0)+,e0                ; 4 Bytes laden (Endian-korrigiert)
            
            REPT    4
            ; --- MID DEKODIEREN (5 BITS) ---
            move.l  e0,d0
            andi.l  #31,d0                  ; Unterste 5 Bits
            bsr     decode_mid_5bit
            lsr.l   #5,e0                   ; Puffer 5 Bits weiterschieben

            ; --- SIDE DEKODIEREN (3 BITS) ---
            move.l  e0,d0
            andi.l  #7,d0                   ; Restliche 3 Bits des Bytes
            bsr     decode_side_3bit
            lsr.l   #3,e0                   ; Puffer 3 Bits weiterschieben -> Byte leer!

            ; --- M/S MATRIX & SPEICHERN ---
            bsr     ms_matrix_store
            ENDR

            subq.l  #1,d7
            bne     .ms_chunk_loop

            ; -----------------------------------------------------------
            ; PHASE 2: Restliche Bytes (Falls block_cnt nicht durch 4 teilbar)
            ; -----------------------------------------------------------
.ms_byte_loop_init:
            andi.l  #3,d6                   ; D6 = block_cnt % 4
            beq     .ms_done

.ms_byte_loop:
            moveq   #0,e0
            move.b  (a0)+,e0                ; 1 Byte laden (1 Paar)

            ; --- MID DEKODIEREN (5 BITS) ---
            move.l  e0,d0
            andi.l  #31,d0
            bsr     decode_mid_5bit
            lsr.l   #5,e0

            ; --- SIDE DEKODIEREN (3 BITS) ---
            move.l  e0,d0
            andi.l  #7,d0
            bsr     decode_side_3bit

            ; --- M/S MATRIX & SPEICHERN ---
            bsr     ms_matrix_store

            subq.l  #1,d6
            bne     .ms_byte_loop

.ms_done:
            move.l  56(a7),a0               ; Status in RAM zurückschreiben
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


; ==============================================================================
; SUBROUTINE: M/S MATRIX HELPER
; ==============================================================================
ms_matrix_store:
            move.l  d2,d0                   ; L = Mid + Side
            add.l   d4,d0                   
            move.l  d2,d1                   ; R = Mid - Side
            sub.l   d4,d1                   

            cmp.l   #32767,d0
            ble     .ms_chk_min_L
            move.l  #32767,d0
            bra     .ms_clamp_R
.ms_chk_min_L:
            cmp.l   #-32768,d0
            bge     .ms_clamp_R
            move.l  #-32768,d0
            
.ms_clamp_R:
            cmp.l   #32767,d1
            ble     .ms_chk_min_R
            move.l  #32767,d1
            bra     .ms_pack
.ms_chk_min_R:
            cmp.l   #-32768,d1
            bge     .ms_pack
            move.l  #-32768,d1

.ms_pack:
            swap    d0
            move.w  d1,d0
            move.l  d0,(a1)+
            rts


; ==============================================================================
; ADPCM LOGIK: MID (5-Bit)
; ==============================================================================
decode_mid_5bit:
            exg     d6,a2                   
            move.w  (a4,d3.w*2),d6          
            ext.l   d6              
            
            move.l  d6,d1                   
            lsr.l   #4,d1                   ; Basis: step / 16

            btst    #3,d0           
            beq     .no_bit3_M       
            add.l   d6,d1                   ; + step
.no_bit3_M:
            lsr.l   #1,d6                   
            btst    #2,d0           
            beq     .no_bit2_M    
            add.l   d6,d1                   ; + step / 2
.no_bit2_M:
            lsr.l   #1,d6                   
            btst    #1,d0           
            beq     .no_bit1_M    
            add.l   d6,d1                   ; + step / 4
.no_bit1_M:
            lsr.l   #1,d6                   
            btst    #0,d0           
            beq     .no_bit0_M    
            add.l   d6,d1                   ; + step / 8
.no_bit0_M:

            btst    #4,d0                   ; Sign-Bit (Bit 4 bei 5-Bit)
            beq     .add_delta_M
            sub.l   d1,d2                   
            bra     .clamp_M    
.add_delta_M:
            add.l   d1,d2                   

.clamp_M:
            cmp.l   #32767,d2       
            ble     .check_min_M    
            move.l  #32767,d2       
            bra     .update_idx_M   
.check_min_M:
            cmp.l   #-32768,d2      
            bge     .update_idx_M   
            move.l  #-32768,d2      

.update_idx_M:
            move.w  d0,d1                   
            andi.w  #15,d1                  ; Amplitude maskieren
            move.w  (a5,d1.w*2),d1          
            add.w   d1,d3                   

            tst.w   d3              
            bge     .idx_max_M      
            moveq   #0,d3           
            bra     .idx_done_M                     
.idx_max_M:
            cmp.w   #88,d3          
            ble     .idx_done_M     
            move.w  #88,d3          
.idx_done_M:
            exg     d6,a2           
            rts                     


; ==============================================================================
; ADPCM LOGIK: SIDE (3-Bit)
; ==============================================================================
decode_side_3bit:
            exg     d6,a2                   
            move.w  (a4,d5.w*2),d6          
            ext.l   d6              
            
            move.l  d6,d1                   
            lsr.l   #2,d1                   ; Basis: step / 4

            btst    #1,d0           
            beq     .no_bit1_S       
            add.l   d6,d1                   ; + step
.no_bit1_S:
            lsr.l   #1,d6                   
            btst    #0,d0           
            beq     .no_bit0_S    
            add.l   d6,d1                   ; + step / 2
.no_bit0_S:

            btst    #2,d0                   ; Sign-Bit (Bit 2 bei 3-Bit)
            beq     .add_delta_S
            sub.l   d1,d4                   
            bra     .clamp_S    
.add_delta_S:
            add.l   d1,d4                   

.clamp_S:
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
            andi.w  #3,d1                   ; Amplitude maskieren
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
            exg     d6,a2           
            rts                     


; ------------------------------------------------------------------------------
; STATISCHE TABELLEN
; ------------------------------------------------------------------------------

index_adj_5bit:
            dc.w    -1, -1, -1, -1, -1, -1, -1, -1
            dc.w     1,  2,  4,  6,  8, 10, 13, 16

index_adj_3bit:
            dc.w    -1, -1, 1, 2

step_table:
            dc.w    7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31
            dc.w    34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143
            dc.w    157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658
            dc.w    724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024
            dc.w    3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899
            dc.w    15289,16818,18500,20350,22385,24623,27086,29794,32767

            END