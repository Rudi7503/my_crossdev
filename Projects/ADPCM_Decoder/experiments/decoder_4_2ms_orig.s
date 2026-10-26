; ==============================================================================
; EXTREME OPTIMIZED 68080 ADPCM 4/2-BIT M/S DECODER SUITE
; 4 Bits Mid + 2 Bits Side = 6 Bits pro Paar.
; Nutzt den 24-Bit Alignment-Trick aus dem 3-Bit Decoder (addq.l #3, a0)
; ==============================================================================

            SECTION CODE,CODE
            
            XDEF    _decode_4_2bit_ms_asm

; ==============================================================================
; DECODER: 4/2-BIT MID-SIDE (M/S MATRIX)
; ==============================================================================
_decode_4_2bit_ms_asm:
            movem.l d2-d7/a2-a6,-(a7)

            movec   ccc,d0
            move.l  d0,e1           

            ; 44 Bytes Stack-Offset (11 Register) + 4 Bytes PC = 48 Basis
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
            lea     index_adj_4bit(pc),a5   ; A5 = Tabellen für Mid (4-Bit)
            lea     index_adj_2bit(pc),a6   ; A6 = Tabellen für Side (2-Bit)

            tst.l   64(a7)                  
            beq     .ms_done

.ms_block_loop:
            ; 15 Bytes Block = 5 Durchläufe à 24-Bit (3 Bytes)[cite: 3]
            moveq   #4,d6                   ; 5 Durchläufe (4 bis 0)[cite: 3]
.ms_chunk_loop:
            ; --- DER 32-BIT LOAD / 24-BIT PROCESS TRICK ---[cite: 3]
            movex.l (a0),e0                 ; 32-Bit lesen (inkl. Endian Swap)[cite: 3]
            addq.l  #3,a0                   ; Lesezeiger exakt 3 Bytes (24 Bits) weiterrücken![cite: 3]

            moveq   #3,d7                   ; 4 M/S-Paare (4 x 6 = 24 Bits)
.ms_pair_loop:
            ; --- 1. MID DEKODIEREN (4 BITS) ---
            move.l  e0,d0                   
            andi.l  #15,d0                  ; 4-Bit Maske (Mid)
            bsr     decode_mid_4bit          

            lsr.l   #4,e0                   ; 4 Bits weiter

            ; --- 2. SIDE DEKODIEREN (2 BITS) ---
            move.l  e0,d0           
            andi.l  #3,d0                   ; 2-Bit Maske (Side)
            bsr     decode_side_2bit        

            lsr.l   #2,e0                   ; 2 Bits weiter

            ; --- 3. M/S MATRIX & PACKING ---
            move.l  d2,d0           
            add.l   d4,d0                   ; D0 = Links (Mid + Side)
            move.l  d2,d1           
            sub.l   d4,d1                   ; D1 = Rechts (Mid - Side)

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

            dbf     d7,.ms_pair_loop
            dbf     d6,.ms_chunk_loop

            subq.l  #1,64(a7)               ; block_cnt--
            bne     .ms_block_loop

.ms_done:
            move.l  56(a7),a0               ; M/S Status ins RAM zurück
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


; ==============================================================================
; ADPCM LOGIK: MID (4-Bit)
; ==============================================================================
decode_mid_4bit:
            exg     d6,a2                   ; TRICK: D6 freimachen[cite: 2, 3]
            move.w  (a4,d3.w*2),d6          
            ext.l   d6              
            
            move.l  d6,d1                   
            lsr.l   #3,d1                   ; d1 = step / 8 (Basis)

            btst    #2,d0           
            beq     .no_bit2_M       
            add.l   d6,d1                   ; Delta += step
.no_bit2_M:
            lsr.l   #1,d6                   ; d6 = step / 2
            btst    #1,d0           
            beq     .no_bit1_M    
            add.l   d6,d1                   ; Delta += step / 2
.no_bit1_M:
            lsr.l   #1,d6                   ; d6 = step / 4
            btst    #0,d0           
            beq     .no_bit0_M    
            add.l   d6,d1                   ; Delta += step / 4
.no_bit0_M:

            btst    #3,d0                   ; Sign-Bit (Bit 3)
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
            andi.w  #7,d1                   ; Sign ausmaskieren (0-7)
            move.w  (a5,d1.w*2),d1          ; index_adj_4bit
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
            exg     d6,a2                   ; TRICK: Loop-Counter D6 zurückholen[cite: 2, 3]
            rts                     


; ==============================================================================
; ADPCM LOGIK: SIDE (2-Bit)
; ==============================================================================
decode_side_2bit:
            exg     d6,a2                   
            move.w  (a4,d5.w*2),d6          
            ext.l   d6              
            
            move.l  d6,d1                   
            lsr.l   #1,d1                   ; d1 = step / 2

            btst    #0,d0           
            beq     .no_b0_S       
            add.l   d6,d1                   ; Delta += step
.no_b0_S:

            btst    #1,d0                   ; Sign-Bit (Bit 1)
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
            andi.w  #3,d1                   ; (0-3)
            move.w  (a6,d1.w*2),d1          ; index_adj_2bit
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

index_adj_4bit:
            dc.w    -1, -1, -1, -1,  2,  4,  6,  8

index_adj_2bit:
            dc.w    -1,  2, -1,  2

step_table:
            dc.w    7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31
            dc.w    34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143
            dc.w    157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658
            dc.w    724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024
            dc.w    3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899
            dc.w    15289,16818,18500,20350,22385,24623,27086,29794,32767

            END