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

            lea     step_table(pc),a4       ; A4 = Pointer auf Step-Tabelle
            lea     index_adj_table(pc),a6  ; A6 = Pointer auf Adjust-Tabelle

            tst.l   64(a7)                  ; Block Count check
            beq     .st_done

.st_block_loop:
            moveq   #2,d6           
.st_chunk_loop:
            movex.l (a0)+,e0                ; 32-Bit = 8 Nibbles = 4 Stereo-Paare
            moveq   #3,d7                   ; Schleife läuft 4x
.st_pair_loop_long:
            move.l  e0,d0                   ; Links dekodieren
            andi.l  #15,d0                  ; 4-Bit Maske
            bsr     decode_primary          

            lsr.l   #4,e0                   ; Bitstream 4 Bits weiter schieben
            move.l  e0,d0                   ; Rechts dekodieren
            andi.l  #15,d0           
            bsr     decode_secondary        

            move.w  d2,d1                   ; 32-Bit Packing [Links | Rechts]
            swap    d1              
            move.w  d4,d1           
            move.l  d1,(a1)+        

            lsr.l   #4,e0                   ; Bitstream 4 Bits weiter schieben
            dbf     d7,.st_pair_loop_long
            dbf     d6,.st_chunk_loop

            moveq   #2,d6                   ; 3 Bytes Rest
.st_byte_loop:
            move.l  #0,e0           
            move.b  (a0)+,e0                ; 1 Byte = 2 Nibbles = 1 Stereo-Paar
            moveq   #0,d7                   ; Schleife läuft 1x
.st_pair_loop_byte:
            move.l  e0,d0
            andi.l  #15,d0
            bsr     decode_primary

            lsr.l   #4,e0
            move.l  e0,d0
            andi.l  #15,d0
            bsr     decode_secondary

            move.w  d2,d1
            swap    d1
            move.w  d4,d1
            move.l  d1,(a1)+

            lsr.l   #4,e0
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

            lea     step_table(pc),a4       
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

            lea     step_table(pc),a4       
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
            move.w  (a4,d3.w*2),d6          ; d6 = step (aus Tabelle)
            ext.l   d6              
            
            move.l  d6,d1                   ; d1 = step kopieren
            lsr.l   #3,d1                   ; d1 = Basis Delta (step / 8)

            ; Delta aufbauen anhand der Bits (2, 1, 0)
            btst    #2,d0           
            beq     .no_bit2_P       
            add.l   d6,d1                   ; Delta += step
.no_bit2_P:
            lsr.l   #1,d6                   ; d6 = step / 2
            btst    #1,d0           
            beq     .no_bit1_P    
            add.l   d6,d1                   ; Delta += step / 2
.no_bit1_P:
            lsr.l   #1,d6                   ; d6 = step / 4
            btst    #0,d0
            beq     .no_bit0_P
            add.l   d6,d1                   ; Delta += step / 4
.no_bit0_P:

            ; Vorzeichen anwenden (Bit 3)
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
            move.w  (a4,d5.w*2),d6  
            ext.l   d6              
            
            move.l  d6,d1           
            lsr.l   #3,d1                   ; Basis Delta (step / 8)

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

            END