; ==============================================================================
; EXTREME OPTIMIZED 68080 ADPCM 5-BIT DECODER SUITE
; Basiert auf dem genialen Rotate-and-Load Trick (ror/rol)
; Keine Bit-Masken beim Mergen nötig! (15 Bytes = 24 Samples = 12 Paare)
; ==============================================================================

            SECTION CODE,CODE
            
            XDEF    _decode_5bit_stereo_asm
            XDEF    _decode_5bit_ms_asm
            XDEF    _decode_5bit_mono_asm

; ==============================================================================
; 1. DECODER: 5-BIT STEREO
; ==============================================================================
_decode_5bit_stereo_asm:
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
            lea     index_adj_5bit(pc),a6   

            tst.l   64(a7)                  
            beq     .st_done

.st_block_loop:
            ; -----------------------------------------------------------
            ; PHASE 1: 4 Bytes laden -> 6 Samples (3 Paare)
            ; -----------------------------------------------------------
            movex.l (a0)+,d7                ; 32 Bits laden
            
            REPT    3
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_primary
            lsr.l   #5,d7
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_secondary
            lsr.l   #5,d7
            move.w  d2,d1
            swap    d1
            move.w  d4,d1
            move.l  d1,(a1)+
            ENDR
            ; d7 hat jetzt exakt 2 Bits Rest!

            ; -----------------------------------------------------------
            ; PHASE 2: 2 Bytes laden -> 3 Samples (1 Paar + 1 Links)
            ; -----------------------------------------------------------
            ror.l   #2,d7                   ; Rest in Bits 31-30 sichern
            movex.w (a0)+,d7                ; Neues Word in Bits 15-0 laden
            rol.l   #2,d7                   ; Wieder runterrotieren -> 18 Bits

            ; Paar 4
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_primary
            lsr.l   #5,d7
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_secondary
            lsr.l   #5,d7
            move.w  d2,d1
            swap    d1
            move.w  d4,d1
            move.l  d1,(a1)+

            ; Sample 9 (Nur Links)
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_primary
            lsr.l   #5,d7
            ; d7 hat jetzt exakt 3 Bits Rest!

            ; -----------------------------------------------------------
            ; PHASE 3: 2 Bytes laden -> 3 Samples (1 Rechts + 1 Paar)
            ; -----------------------------------------------------------
            ror.l   #3,d7                   ; Rest sichern
            movex.w (a0)+,d7                ; Laden
            rol.l   #3,d7                   ; Runterrotieren -> 19 Bits

            ; Sample 10 (Nur Rechts)
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_secondary
            lsr.l   #5,d7
            
            move.w  d2,d1                   ; Paar 5 (Sample 9 & 10) schreiben
            swap    d1
            move.w  d4,d1
            move.l  d1,(a1)+

            ; Paar 6
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_primary
            lsr.l   #5,d7
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_secondary
            lsr.l   #5,d7
            move.w  d2,d1
            swap    d1
            move.w  d4,d1
            move.l  d1,(a1)+
            ; d7 hat jetzt exakt 4 Bits Rest!

            ; -----------------------------------------------------------
            ; PHASE 4: 2 Bytes laden -> 4 Samples (2 Paare)
            ; -----------------------------------------------------------
            ror.l   #4,d7                   ; Rest sichern
            movex.w (a0)+,d7                ; Laden
            rol.l   #4,d7                   ; Runterrotieren -> 20 Bits

            REPT    2
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_primary
            lsr.l   #5,d7
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_secondary
            lsr.l   #5,d7
            move.w  d2,d1
            swap    d1
            move.w  d4,d1
            move.l  d1,(a1)+
            ENDR
            ; d7 hat jetzt exakt 0 Bits Rest! (10 Bytes komplett verbraucht)

            ; -----------------------------------------------------------
            ; PHASE 5: 4 Bytes laden -> 6 Samples (3 Paare)
            ; -----------------------------------------------------------
            movex.l (a0)+,d7                ; Neuer sauberer Start!
            
            REPT    3
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_primary
            lsr.l   #5,d7
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_secondary
            lsr.l   #5,d7
            move.w  d2,d1
            swap    d1
            move.w  d4,d1
            move.l  d1,(a1)+
            ENDR
            ; d7 hat jetzt wieder exakt 2 Bits Rest!

            ; -----------------------------------------------------------
            ; PHASE 6: Letztes Byte laden -> 2 Samples (1 Paar)
            ; -----------------------------------------------------------
            ror.l   #2,d7
            moveq   #0,d1                   
            move.b  (a0)+,d1                ; move.b für das 15. Byte!
            or.l    d1,d7                   ; (Da nur 8 Bit, reicht hier ein kurzes OR, 
                                            ; weil move.b nicht direkt ins obere Word schreibt)
            rol.l   #2,d7                   ; -> 10 Bits

            ; Letztes Paar
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_primary
            lsr.l   #5,d7
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_secondary
            
            move.w  d2,d1
            swap    d1
            move.w  d4,d1
            move.l  d1,(a1)+

            subq.l  #1,64(a7)               
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
; 2. DECODER: 5-BIT MID-SIDE (M/S MATRIX)
; ==============================================================================
_decode_5bit_ms_asm:
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
            lea     index_adj_5bit(pc),a6
            
            tst.l   64(a7)                  
            beq     .ms_done

.ms_block_loop:
            ; -----------------------------------------------------------
            ; PHASE 1: 4 Bytes laden -> 6 Samples (3 Paare)
            ; -----------------------------------------------------------
            movex.l (a0)+,d7
            
            REPT    3
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_primary
            lsr.l   #5,d7
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_secondary
            lsr.l   #5,d7
            bsr     ms_matrix_store
            ENDR

            ; -----------------------------------------------------------
            ; PHASE 2: 2 Bytes laden -> 3 Samples (Paar 4 + Paar 5 Mid)
            ; -----------------------------------------------------------
            ror.l   #2,d7
            movex.w (a0)+,d7
            rol.l   #2,d7

            ; Paar 4 komplett
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_primary
            lsr.l   #5,d7
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_secondary
            lsr.l   #5,d7
            bsr     ms_matrix_store

            ; Paar 5 (Nur Mid dekodieren und in d2 aufheben!)
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_primary
            lsr.l   #5,d7

            ; -----------------------------------------------------------
            ; PHASE 3: 2 Bytes laden -> 3 Samples (Paar 5 Side + Paar 6)
            ; -----------------------------------------------------------
            ror.l   #3,d7
            movex.w (a0)+,d7
            rol.l   #3,d7

            ; Paar 5 (Side dekodieren und mit aufgehobenem Mid verrechnen)
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_secondary
            lsr.l   #5,d7
            bsr     ms_matrix_store

            ; Paar 6 komplett
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_primary
            lsr.l   #5,d7
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_secondary
            lsr.l   #5,d7
            bsr     ms_matrix_store

            ; -----------------------------------------------------------
            ; PHASE 4: 2 Bytes laden -> 4 Samples (2 Paare)
            ; -----------------------------------------------------------
            ror.l   #4,d7
            movex.w (a0)+,d7
            rol.l   #4,d7

            REPT    2
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_primary
            lsr.l   #5,d7
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_secondary
            lsr.l   #5,d7
            bsr     ms_matrix_store
            ENDR

            ; -----------------------------------------------------------
            ; PHASE 5: 4 Bytes laden -> 6 Samples (3 Paare)
            ; -----------------------------------------------------------
            movex.l (a0)+,d7
            
            REPT    3
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_primary
            lsr.l   #5,d7
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_secondary
            lsr.l   #5,d7
            bsr     ms_matrix_store
            ENDR

            ; -----------------------------------------------------------
            ; PHASE 6: Letztes Byte laden -> 2 Samples (1 Paar)
            ; -----------------------------------------------------------
            ror.l   #2,d7
            moveq   #0,d1                   
            move.b  (a0)+,d1
            or.l    d1,d7
            rol.l   #2,d7

            ; Letztes Paar
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_primary
            lsr.l   #5,d7
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_secondary
            bsr     ms_matrix_store

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
; M/S MATRIX HELPER
; (Wird von M/S benötigt. Falls nicht vorhanden, hier am Ende einfügen)
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
; 3. DECODER: 5-BIT MONO (24 Samples)
; ==============================================================================
_decode_5bit_mono_asm:
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
            lea     index_adj_5bit(pc),a6
            
            tst.l   60(a7)                  
            beq     .mo_done

.mo_block_loop:
            
            ; 1. 4-Byte Load -> 6 Samples
            movex.l (a0)+,d7
            REPT    6
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_primary
            move.w  d2,(a1)+
            lsr.l   #5,d7
            ENDR
            ; 2 Bits Rest

            ; 2. 2-Byte Load -> 3 Samples
            ror.l   #2,d7
            movex.w (a0)+,d7
            rol.l   #2,d7
            REPT    3
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_primary
            move.w  d2,(a1)+
            lsr.l   #5,d7
            ENDR
            ; 3 Bits Rest

            ; 3. 2-Byte Load -> 3 Samples
            ror.l   #3,d7
            movex.w (a0)+,d7
            rol.l   #3,d7
            REPT    3
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_primary
            move.w  d2,(a1)+
            lsr.l   #5,d7
            ENDR
            ; 4 Bits Rest

            ; 4. 2-Byte Load -> 4 Samples
            ror.l   #4,d7
            movex.w (a0)+,d7
            rol.l   #4,d7
            REPT    4
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_primary
            move.w  d2,(a1)+
            lsr.l   #5,d7
            ENDR
            ; 0 Bits Rest

            ; 5. 4-Byte Load -> 6 Samples
            movex.l (a0)+,d7
            REPT    6
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_primary
            move.w  d2,(a1)+
            lsr.l   #5,d7
            ENDR
            ; 2 Bits Rest

            ; 6. 1-Byte Load -> 2 Samples (Letztes Byte)
            ror.l   #2,d7
            moveq   #0,d1
            move.b  (a0)+,d1
            or.l    d1,d7
            rol.l   #2,d7
            REPT    2
            move.l  d7,d0
            andi.l  #31,d0
            bsr     decode_primary
            move.w  d2,(a1)+
            lsr.l   #5,d7
            ENDR

            subq.l  #1,60(a7)               
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


; ZENTRALE SUBROUTINEN WIE GEHABT... (Habe ich zur Übersicht hier gekürzt, 
; bleiben aber exakt 1:1 gleich wie in deinem funktionierenden Code!)

; ==============================================================================
; ZENTRALE SUBROUTINEN (5-Bit IMA ADPCM Logik)
; ==============================================================================

decode_primary:
            exg     d6,a2                   
            move.w  (a4,d3.w*2),d6          
            ext.l   d6              
            
            move.l  d6,d1                   
            lsr.l   #4,d1                   

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

            btst    #4,d0                   
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
            andi.w  #15,d1                  
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
            lsr.l   #4,d1                   

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

            btst    #4,d0           
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
            andi.w  #15,d1
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

step_table:
            dc.w    7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31
            dc.w    34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143
            dc.w    157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658
            dc.w    724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024
            dc.w    3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899
            dc.w    15289,16818,18500,20350,22385,24623,27086,29794,32767

            END