; ==============================================================================
; EXTREME OPTIMIZED 68080 ADPCM 6-BIT DECODER SUITE
; Basiert strikt auf dem 24-Bit / 3-Byte Lade-Trick.
; Passgenau für Encoder v18 (15-Byte Blocks, LSB-first Bitpacking).
; Enthält: Stereo (L/R), Mid-Side (M/S) und Mono
; ==============================================================================

            SECTION CODE,CODE
        
            XDEF    _decode_6bit_stereo_asm
            XDEF    _decode_6bit_ms_asm
            XDEF    _decode_6bit_mono_asm

;
; ==============================================================================
; 1. DECODER: 6-BIT STEREO
; ==============================================================================
_decode_6bit_stereo_asm:
            movem.l d2-d7/a2-a6,-(a7)

            movec   ccc,d0
            move.l  d0,e2           

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

            lea     step_table(pc),a4       
            lea     index_adj_6bit(pc),a6   

            tst.l   64(a7)                  
            beq     .st_done

.st_block_loop:
            ; 15 Bytes Block = 5 Chunks à 3 Bytes (24 Bits) -> 20 Samples total
            moveq   #4,d6                   ; 5 Durchläufe (4 bis 0)
.st_chunk_loop:
            ; --- DER 32-BIT LOAD / 24-BIT PROCESS TRICK (LITTLE-ENDIAN FIX) ---
            movex.l (a0),e0
            ror.w   #8,e0                   ; Unteres Word anpassen
            swap    e0                      
            ror.w   #8,e0                   ; Oberes Word anpassen
            
            addq.l  #3,a0                   ; Lesezeiger exakt 3 Bytes weiterrücken!

            ; 24 Bits = 2 Stereo-Paare (2 x 12 Bits = 24 Bits)
            moveq   #1,d7                   ; Schleife läuft 2x
.st_pair_loop:
            move.l  e0,d0                   ; Links dekodieren
            andi.l  #63,d0                  ; 6-Bit Maske (0-63)
            bsr     decode_primary          

            lsr.l   #6,e0                   ; Bitstream 6 Bits weiter schieben
            
            move.l  e0,d0                   ; Rechts dekodieren
            andi.l  #63,d0           
            bsr     decode_secondary        

            move.w  d2,d1                   ; 32-Bit Packing [Links | Rechts]
            swap    d1              
            move.w  d4,d1           
            move.l  d1,(a1)+                ; Ins RAM flashen

            lsr.l   #6,e0                   ; Bitstream 6 Bits weiter schieben
            dbf     d7,.st_pair_loop
            
            dbf     d6,.st_chunk_loop       
            
            subq.l  #1,64(a7)               ; block_cnt--
            bne     .st_block_loop

.st_done:
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


; ==============================================================================
; 2. DECODER: 6-BIT MID-SIDE (M/S MATRIX)
; ==============================================================================
_decode_6bit_ms_asm:
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

            lea     step_table(pc),a4       
            lea     index_adj_6bit(pc),a6
            
            tst.l   64(a7)                  
            beq     .ms_done

.ms_block_loop:
            moveq   #4,d6                   ; 5 Chunks à 3 Bytes
.ms_chunk_loop:
            movex.l (a0),e0
            ror.w   #8,e0
            swap    e0
            ror.w   #8,e0
            addq.l  #3,a0

            moveq   #1,d7                   ; 2 M/S-Paare
.ms_pair_loop:
            move.l  e0,d0                   ; Mid dekodieren
            andi.l  #63,d0           
            bsr     decode_primary          

            lsr.l   #6,e0                   ; Side dekodieren
            move.l  e0,d0           
            andi.l  #63,d0           
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

            lsr.l   #6,e0           
            dbf     d7,.ms_pair_loop
            dbf     d6,.ms_chunk_loop

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


; ==============================================================================
; 3. DECODER: 6-BIT MONO
; ==============================================================================
_decode_6bit_mono_asm:
            movem.l d2-d7/a2-a6,-(a7)       

            movec   ccc,d0                  
            move.l  d0,e2           

            move.l  48(a7),a0               
            move.l  52(a7),a1               
            move.l  56(a7),a5               

            move.l  (a5),d2                 
            move.w  4(a5),d3        
            ext.l   d3                      

            lea     step_table(pc),a4       
            lea     index_adj_6bit(pc),a6
            
            tst.l   60(a7)                  
            beq     .mo_done

.mo_block_loop:
            moveq   #4,d6                   ; 5 Chunks à 3 Bytes
.mo_chunk_loop:
            movex.l (a0),e0
            ror.w   #8,e0
            swap    e0
            ror.w   #8,e0
            addq.l  #3,a0

            moveq   #3,d7                   ; 4 Mono-Samples pro 3-Byte Chunk (4 * 6 = 24 Bits)
.mo_sample_loop:
            move.l  e0,d0           
            andi.l  #63,d0                  ; 6-Bit Maske
            bsr     decode_primary          

            move.w  d2,(a1)+                ; Schreibt 16-Bit Mono RAM
            
            lsr.l   #6,e0                   ; 6 Bits weiter schieben
            dbf     d7,.mo_sample_loop
            dbf     d6,.mo_chunk_loop

            subq.l  #1,60(a7)               ; block_cnt--
            bne     .mo_block_loop

.mo_done:
            move.l  56(a7),a5               
            move.l  d2,(a5)         
            move.w  d3,4(a5)        

            movem.l (a7)+,d2-d7/a2-a6       
            
            movec   ccc,d0                  
            move.l  e2,d1           
            sub.l   d1,d0                   
            rts


; ==============================================================================
; ZENTRALE SUBROUTINEN (6-Bit IMA ADPCM Logik)
; ==============================================================================

decode_primary:
            exg     d6,a2                   
            move.w  (a4,d3.w*2),d6          
            ext.l   d6              
            
            move.l  d6,d1                   
            lsr.l   #5,d1                   ; Basis Delta (step / 32)

            btst    #4,d0           
            beq     .no_bit4_P       
            add.l   d6,d1           
.no_bit4_P:
            lsr.l   #1,d6                   
            btst    #3,d0           
            beq     .no_bit3_P    
            add.l   d6,d1           
.no_bit3_P:
            lsr.l   #1,d6                   
            btst    #2,d0           
            beq     .no_bit2_P    
            add.l   d6,d1           
.no_bit2_P:
            lsr.l   #1,d6                   
            btst    #1,d0           
            beq     .no_bit1_P    
            add.l   d6,d1           
.no_bit1_P:
            lsr.l   #1,d6                   
            btst    #0,d0           
            beq     .no_bit0_P    
            add.l   d6,d1           
.no_bit0_P:

            btst    #5,d0           
            beq     .add_delta_P
            sub.l   d1,d2                   
            bra     .clamp_pcm_P    
.add_delta_P:
            add.l   d1,d2                   

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
            move.w  d0,d1                   
            andi.w  #31,d1                  ; Amplitude 0-31
            move.w  (a6,d1.w*2),d1          
            add.w   d1,d3                   

            tst.w   d3              
            bge     .idx_max_P      
            moveq   #0,d3           
            bra     .idx_done_P                     
.idx_max_P:
            cmp.w   #88,d3          
            ble     .idx_done_P     
            move.w  #88,d3          
.idx_done_P:
            exg     d6,a2           
            rts                     

decode_secondary:
            exg     d6,a2                   
            move.w  (a4,d5.w*2),d6  
            ext.l   d6              
            
            move.l  d6,d1           
            lsr.l   #5,d1                   

            btst    #4,d0           
            beq     .no_bit4_S       
            add.l   d6,d1           
.no_bit4_S:
            lsr.l   #1,d6
            btst    #3,d0           
            beq     .no_bit3_S    
            add.l   d6,d1           
.no_bit3_S:
            lsr.l   #1,d6
            btst    #2,d0           
            beq     .no_bit2_S    
            add.l   d6,d1           
.no_bit2_S:
            lsr.l   #1,d6
            btst    #1,d0           
            beq     .no_bit1_S    
            add.l   d6,d1           
.no_bit1_S:
            lsr.l   #1,d6
            btst    #0,d0           
            beq     .no_bit0_S    
            add.l   d6,d1           
.no_bit0_S:

            btst    #5,d0           
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
            andi.w  #31,d1
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

index_adj_6bit:
            dc.w    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
            dc.w     1,  2,  3,  4,  5,  6,  7,  8, 10, 12, 14, 16, 19, 22, 25, 28

step_table:
            dc.w    7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31
            dc.w    34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143
            dc.w    157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658
            dc.w    724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024
            dc.w    3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899
            dc.w    15289,16818,18500,20350,22385,24623,27086,29794,32767

            END