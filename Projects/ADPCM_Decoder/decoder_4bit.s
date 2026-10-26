
; ==============================================================================
; EXTREME OPTIMIZED 68080 ADPCM 4-BIT DECODER SUITE – "Fresh15" (Core 11350)
; Enthält: Stereo (L/R), Mid-Side (M/S) und Mono (15-Byte-Block = 30 Nibbles)
;
; Optimierungen (Fresh9 + Fresh14 + Fresh15):
;   1. Vorberechnete Signed-32-Bit-Delta-Tabelle (delta_table, 89 * 16 Longs):
;      Direkter Tabellenzugriff (a4,d7.w*4) ersetzt die komplette 4-Bit-IMA-
;      Shift/Branch-Kette sowie die Vorzeichenverzweigung.
;   2. Vollständig ausgerollte 15-Byte-Blöcke (keine inneren Chunk-/Paar-Loops):
;      - 3 * movex.l (je 4 Bytes = 8 Nibbles = 4 Stereo-Paare bzw. 8 Mono-Samples)
;      - 3 * move.b  (je 1 Byte  = 2 Nibbles = 1 Stereo-Paar  bzw. 2 Mono-Samples)
;   3. 16-Bit-Overflow-Matrix (Fresh7) im M/S-Modus:
;      Nutzt add.w / sub.w + bvc.s (Overflow-Flag V) statt langer 32-Bit-Clamps.
;   4. 64-Byte-Cacheline-Alignment (align 64) vor _decode_4bit_mono_asm (Fresh14).
;   5. Register-Belegung:
;      - d0/d1: aktuelles 4-Bit-Nibble (0..15) für Links/Mid (d0) bzw. Rechts/Side (d1)
;      - d2/d3: PCM-Prädiktor (32-Bit signed) & Step-Index (0..88) Links/Mid/Mono
;      - d4/d5: PCM-Prädiktor (32-Bit signed) & Step-Index (0..88) Rechts/Side
;      - d6/d7: Scratch-Register für Tabellen-Offset und Delta
;      - e0: 32-Bit-Bitstream-Schieberegister (geladen via Endian-Swap movex.l)
;      - e1: Startwert des Hardware-Taktzählers (CCC) für Zyklusmessung
;      - a0/a1: Eingabe-Bitstream / Ausgabe-PCM-Puffer
;      - a4: Zeiger auf delta_table (89 * 16 signed 32-Bit Deltas)
;      - a6: Zeiger auf index_adj_table (8 * 16-Bit Index-Adjust: -1,-1,-1,-1,2,4,6,8)
; ==============================================================================

            xdef    _decode_4bit_stereo_asm
            xdef    _decode_4bit_ms_asm
            xdef    _decode_4bit_mono_asm

; ------------------------------------------------------------------------------
; MACRO: F_PAIR – Dekodiert 1 Stereo-Paar (2 Nibbles = 8 Bit) aus e0
; Schreibt gepacktes 32-Bit-Sample [Links_16 | Rechts_16] nach (a1)+.
; ------------------------------------------------------------------------------
F_PAIR MACRO
            ; --- Nibbles für Links (d0) und Rechts (d1) aus Bitstream (e0) holen ---
            move.l  e0,d0
            andi.l  #15,d0                  ; d0 = Nibble Links (0..15, Bit 3 = Sign)
            lsr.l   #4,e0                   ; Bitstream 4 Bit weiterschieben
            move.l  e0,d1
            andi.l  #15,d1                  ; d1 = Nibble Rechts (0..15, Bit 3 = Sign)
            lsr.l   #4,e0                   ; Bitstream 4 Bit weiterschieben

            ; --- Kanal Links: Signed-32-Bit-Delta laden & auf PCM_L (d2) addieren ---
            moveq   #0,d6
            move.w  d3,d6                   ; d6 = Step-Index Links (0..88)
            lsl.w   #4,d6                   ; d6 = index * 16 (16 Einträge pro Step-Index)
            move.l  d0,d7
            add.w   d6,d7                   ; d7 = index * 16 + nibble (0..15)
            move.l  (a4,d7.w*4),d6          ; d6 = vorberechnetes signed 32-Bit-Delta

            add.l   d6,d2                   ; PCM_L += delta
            cmp.l   #32767,d2               ; 16-Bit Sättigung Links (+32767)
            ble     .lmin\@
            move.l  #32767,d2
            bra     .lidx\@
.lmin\@:
            cmp.l   #-32768,d2              ; 16-Bit Sättigung Links (-32768)
            bge     .lidx\@
            move.l  #-32768,d2
.lidx\@:
            move.w  d0,d7
            andi.w  #7,d7                   ; Vorzeichenbit (Bit 3) ausblenden -> Magnitude (0..7)
            add.w   (a6,d7.w*2),d3          ; Step-Index Links anpassen
            tst.w   d3                      ; Index-Clamping Links (0..88)
            bge     .lmax\@
            moveq   #0,d3
            bra     .ldone\@
.lmax\@:
            cmp.w   #88,d3
            ble     .ldone\@
            move.w  #88,d3
.ldone\@:

            ; --- Kanal Rechts: Signed-32-Bit-Delta laden & auf PCM_R (d4) addieren ---
            moveq   #0,d6
            move.w  d5,d6                   ; d6 = Step-Index Rechts (0..88)
            lsl.w   #4,d6                   ; d6 = index * 16
            move.l  d1,d7
            add.w   d6,d7                   ; d7 = index * 16 + nibble (0..15)
            move.l  (a4,d7.w*4),d6          ; d6 = signed 32-Bit-Delta

            add.l   d6,d4                   ; PCM_R += delta
            cmp.l   #32767,d4               ; 16-Bit Sättigung Rechts (+32767)
            ble     .rmin\@
            move.l  #32767,d4
            bra     .ridx\@
.rmin\@:
            cmp.l   #-32768,d4              ; 16-Bit Sättigung Rechts (-32768)
            bge     .ridx\@
            move.l  #-32768,d4
.ridx\@:
            move.w  d1,d7
            andi.w  #7,d7                   ; Magnitude (0..7) isolieren
            add.w   (a6,d7.w*2),d5          ; Step-Index Rechts anpassen
            tst.w   d5                      ; Index-Clamping Rechts (0..88)
            bge     .rmax\@
            moveq   #0,d5
            bra     .rdone\@
.rmax\@:
            cmp.w   #88,d5
            ble     .rdone\@
            move.w  #88,d5
.rdone\@:

            ; --- 32-Bit Stereo-Ausgabe [Links_16 | Rechts_16] ---
            move.w  d2,d0
            swap    d0
            move.w  d4,d0
            move.l  d0,(a1)+
        ENDM

; ------------------------------------------------------------------------------
; MACRO: F_MS_PAIR – Dekodiert 1 Mid/Side-Paar (2 Nibbles = 8 Bit) aus e0
; Berechnet anschließend L = Mid + Side und R = Mid - Side über die schnelle
; 16-Bit-Overflow-Matrix (add.w/sub.w + bvc.s) und schreibt [L_16 | R_16].
; ------------------------------------------------------------------------------
F_MS_PAIR MACRO
            ; --- Nibbles für Mid (d0) und Side (d1) aus Bitstream (e0) holen ---
            move.l  e0,d0
            andi.l  #15,d0                  ; d0 = Nibble Mid (0..15)
            lsr.l   #4,e0
            move.l  e0,d1
            andi.l  #15,d1                  ; d1 = Nibble Side (0..15)
            lsr.l   #4,e0

            ; --- Kanal Mid (d2/d3) dekodieren ---
            moveq   #0,d6
            move.w  d3,d6                   ; d6 = Step-Index Mid (0..88)
            lsl.w   #4,d6                   ; d6 = index * 16
            move.l  d0,d7
            add.w   d6,d7                   ; d7 = index * 16 + nibble
            move.l  (a4,d7.w*4),d6          ; d6 = signed 32-Bit-Delta Mid
            add.l   d6,d2                   ; PCM_Mid += delta
            cmp.l   #32767,d2               ; Clamping Mid (-32768..32767)
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
            add.w   (a6,d7.w*2),d3          ; Step-Index Mid anpassen & clampen (0..88)
            tst.w   d3
            bge     .lmax\@
            moveq   #0,d3
            bra     .ldone\@
.lmax\@:
            cmp.w   #88,d3
            ble     .ldone\@
            move.w  #88,d3
.ldone\@:

            ; --- Kanal Side (d4/d5) dekodieren ---
            moveq   #0,d6
            move.w  d5,d6                   ; d6 = Step-Index Side (0..88)
            lsl.w   #4,d6                   ; d6 = index * 16
            move.l  d1,d7
            add.w   d6,d7                   ; d7 = index * 16 + nibble
            move.l  (a4,d7.w*4),d6          ; d6 = signed 32-Bit-Delta Side
            add.l   d6,d4                   ; PCM_Side += delta
            cmp.l   #32767,d4               ; Clamping Side (-32768..32767)
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
            add.w   (a6,d7.w*2),d5          ; Step-Index Side anpassen & clampen (0..88)
            tst.w   d5
            bge     .rmax\@
            moveq   #0,d5
            bra     .rdone\@
.rmax\@:
            cmp.w   #88,d5
            ble     .rdone\@
            move.w  #88,d5
.rdone\@:

            ; --- 16-Bit-Overflow-Matrix (Fresh7): L = Mid + Side, R = Mid - Side ---
            move.w  d2,d0
            add.w   d4,d0                   ; d0.w = Mid + Side (setzt V-Flag bei 16-Bit-Überlauf)
            bvc.s   .mlok\@                 ; Kein Überlauf -> direkt weiter
            tst.w   d2                      ; Bei Überlauf entscheidet das Vorzeichen von Mid:
            bge.s   .mlmax\@                ; Mid >= 0 -> positiver Überlauf (+32767)
            move.w  #-32768,d0              ; Mid <  0 -> negativer Unterlauf (-32768)
            bra.s   .mlok\@
.mlmax\@:
            move.w  #32767,d0
.mlok\@:
            move.w  d2,d1
            sub.w   d4,d1                   ; d1.w = Mid - Side (setzt V-Flag bei 16-Bit-Überlauf)
            bvc.s   .mrok\@                 ; Kein Überlauf -> direkt weiter
            tst.w   d2                      ; Bei Überlauf entscheidet das Vorzeichen von Mid:
            bge.s   .mrmax\@                ; Mid >= 0 -> positiver Überlauf (+32767)
            move.w  #-32768,d1              ; Mid <  0 -> negativer Unterlauf (-32768)
            bra.s   .mrok\@
.mrmax\@:
            move.w  #32767,d1
.mrok\@:
            swap    d0                      ; 32-Bit Stereo-Ausgabe [Links_16 | Rechts_16]
            move.w  d1,d0
            move.l  d0,(a1)+
        ENDM

; ------------------------------------------------------------------------------
; MACRO: F_MONO_NIB – Dekodiert 1 Mono-Nibble (4 Bit) aus e0
; Schreibt 16-Bit-Sample nach (a1)+.
; ------------------------------------------------------------------------------
F_MONO_NIB MACRO
            move.l  e0,d0
            andi.l  #15,d0                  ; d0 = Mono-Nibble (0..15)
            lsr.l   #4,e0                   ; Bitstream 4 Bit weiterschieben

            moveq   #0,d6
            move.w  d3,d6                   ; d6 = Step-Index Mono (0..88)
            lsl.w   #4,d6                   ; d6 = index * 16
            move.l  d0,d7
            add.w   d6,d7                   ; d7 = index * 16 + nibble
            move.l  (a4,d7.w*4),d6          ; d6 = signed 32-Bit-Delta
            add.l   d6,d2                   ; PCM_Mono += delta
            cmp.l   #32767,d2               ; Clamping Mono (-32768..32767)
            ble     .mmin\@
            move.l  #32767,d2
            bra     .midx\@
.mmin\@:
            cmp.l   #-32768,d2
            bge     .midx\@
            move.l  #-32768,d2
.midx\@:
            move.w  d0,d7
            andi.w  #7,d7                   ; Magnitude (0..7)
            add.w   (a6,d7.w*2),d3          ; Step-Index Mono anpassen & clampen (0..88)
            tst.w   d3
            bge     .mmax\@
            moveq   #0,d3
            bra     .mdone\@
.mmax\@:
            cmp.w   #88,d3
            ble     .mdone\@
            move.w  #88,d3
.mdone\@:
            move.w  d2,(a1)+                ; 16-Bit Mono-Sample schreiben
        ENDM

; ==============================================================================
; 1. DECODER: 4-BIT STEREO (Voll unrolled: 15 Bytes = 15 Stereo-Paare pro Block)
; ==============================================================================
_decode_4bit_stereo_asm:
            movem.l d2-d7/a2-a6,-(a7)       ; 11 Register sichern (44 Bytes)
            movec   ccc,d0                  ; Zyklenmessung START
            move.l  d0,e1
            move.l  48(a7),a0               ; a0 = in_buffer
            move.l  52(a7),a1               ; a1 = out_buffer
            move.l  56(a7),a5               ; a5 = chL (State Links)
            move.l  60(a7),a3               ; a3 = chR (State Rechts)
            move.l  (a5),d2                 ; d2 = PCM Links
            move.w  4(a5),d3
            ext.l   d3                      ; d3 = Step-Index Links
            move.l  (a3),d4                 ; d4 = PCM Rechts
            move.w  4(a3),d5
            ext.l   d5                      ; d5 = Step-Index Rechts
            lea     delta_table,a4          ; a4 = Signed-32-Bit-Delta-Tabelle
            lea     index_adj_table,a6      ; a6 = Index-Adjust-Tabelle
            tst.l   64(a7)                  ; block_cnt == 0?
            beq     .st_done
.st_block:
            ; --- Chunk 1: 4 Bytes (32 Bit Little-Endian via movex.l) = 4 Paare ---
            movex.l (a0)+,e0
            F_PAIR
            F_PAIR
            F_PAIR
            F_PAIR
            ; --- Chunk 2: 4 Bytes = 4 Paare ---
            movex.l (a0)+,e0
            F_PAIR
            F_PAIR
            F_PAIR
            F_PAIR
            ; --- Chunk 3: 4 Bytes = 4 Paare ---
            movex.l (a0)+,e0
            F_PAIR
            F_PAIR
            F_PAIR
            F_PAIR
            ; --- Restbytes 13..15: je 1 Byte = 1 Paar (zusammen 3 Paare) ---
            move.l  #0,e0
            move.b  (a0)+,e0
            F_PAIR
            move.l  #0,e0
            move.b  (a0)+,e0
            F_PAIR
            move.l  #0,e0
            move.b  (a0)+,e0
            F_PAIR
            subq.l  #1,64(a7)               ; block_cnt--
            bne     .st_block
.st_done:
            move.l  56(a7),a5               ; Kanal-Status Links zurückschreiben
            move.l  d2,(a5)
            move.w  d3,4(a5)
            move.l  60(a7),a3               ; Kanal-Status Rechts zurückschreiben
            move.l  d4,(a3)
            move.w  d5,4(a3)
            movem.l (a7)+,d2-d7/a2-a6
            movec   ccc,d0                  ; Zyklenmessung ENDE
            move.l  e1,d1
            sub.l   d1,d0                   ; Verbrauchte Taktzyklen in d0 zurückgeben
            rts

; ==============================================================================
; 2. DECODER: 4-BIT MID-SIDE (Voll unrolled: 15 Bytes = 15 M/S-Paare pro Block)
; ==============================================================================
_decode_4bit_ms_asm:
            movem.l d2-d7/a2-a6,-(a7)       ; 11 Register sichern (44 Bytes)
            movec   ccc,d0                  ; Zyklenmessung START
            move.l  d0,e1
            move.l  48(a7),a0               ; a0 = in_buffer
            move.l  52(a7),a1               ; a1 = out_buffer
            move.l  56(a7),a5               ; a5 = chMid
            move.l  60(a7),a3               ; a3 = chSide
            move.l  (a5),d2                 ; d2 = PCM Mid
            move.w  4(a5),d3
            ext.l   d3                      ; d3 = Step-Index Mid
            move.l  (a3),d4                 ; d4 = PCM Side
            move.w  4(a3),d5
            ext.l   d5                      ; d5 = Step-Index Side
            lea     delta_table,a4
            lea     index_adj_table,a6
            tst.l   64(a7)                  ; block_cnt == 0?
            beq     .ms_done
.ms_block:
            ; --- 3 * 4 Bytes (je 4 M/S-Paare) + 3 * 1 Byte (je 1 M/S-Paar) ---
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
            subq.l  #1,64(a7)               ; block_cnt--
            bne     .ms_block
.ms_done:
            move.l  56(a7),a5               ; Mid/Side-Status zurückschreiben
            move.l  d2,(a5)
            move.w  d3,4(a5)
            move.l  60(a7),a3
            move.l  d4,(a3)
            move.w  d5,4(a3)
            movem.l (a7)+,d2-d7/a2-a6
            movec   ccc,d0                  ; Zyklenmessung ENDE
            move.l  e1,d1
            sub.l   d1,d0
            rts

; ==============================================================================
; 3. DECODER: 4-BIT MONO (64-Byte aligned, voll unrolled: 15 Bytes = 30 Samples)
; ==============================================================================
            align   64                      ; 68080 I-Cacheline-Alignment (Fresh14)
_decode_4bit_mono_asm:
            movem.l d2-d7/a2-a6,-(a7)       ; 11 Register sichern (44 Bytes)
            movec   ccc,d0                  ; Zyklenmessung START
            move.l  d0,e1
            move.l  48(a7),a0               ; a0 = in_buffer
            move.l  52(a7),a1               ; a1 = out_buffer
            move.l  56(a7),a5               ; a5 = chMono (Hinweis: 1 Pointer weniger als Stereo!)
            move.l  (a5),d2                 ; d2 = PCM Mono
            move.w  4(a5),d3
            ext.l   d3                      ; d3 = Step-Index Mono
            lea     delta_table,a4
            lea     index_adj_table,a6
            tst.l   60(a7)                  ; block_cnt liegt bei Mono auf 60(a7)!
            beq     .mo_done
.mo_block:
            ; --- 3 * 4 Bytes (je 8 Mono-Nibbles) + 3 * 1 Byte (je 2 Mono-Nibbles) ---
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
            subq.l  #1,60(a7)               ; block_cnt--
            bne     .mo_block
.mo_done:
            move.l  56(a7),a5               ; Mono-Status zurückschreiben
            move.l  d2,(a5)
            move.w  d3,4(a5)
            movem.l (a7)+,d2-d7/a2-a6
            movec   ccc,d0                  ; Zyklenmessung ENDE
            move.l  e1,d1
            sub.l   d1,d0
            rts

; ==============================================================================
; LOOKUP-TABELLEN (4-Bit IMA ADPCM)
; - index_adj_table: 8 Wort-Einträge für Magnitude 0..7 (-1,-1,-1,-1, +2,+4,+6,+8)
; - delta_table:     89 Step-Indizes * 16 Nibbles (0..15) = 1424 signed 32-Bit-Deltas
;   Formel: (step>>3) + b0*(step>>2) + b1*(step>>1) + b2*step, negiert wenn b3=1
; ==============================================================================
            section code,code
index_adj_table:
            dc.w    -1, -1, -1, -1, 2, 4, 6, 8
            even
delta_table:
            dc.l    0,1,3,4,7,8,10,11,0,-1,-3,-4,-7,-8,-10,-11
            dc.l    1,3,5,7,9,11,13,15,-1,-3,-5,-7,-9,-11,-13,-15
            dc.l    1,3,5,7,10,12,14,16,-1,-3,-5,-7,-10,-12,-14,-16
            dc.l    1,3,6,8,11,13,16,18,-1,-3,-6,-8,-11,-13,-16,-18
            dc.l    1,3,6,8,12,14,17,19,-1,-3,-6,-8,-12,-14,-17,-19
            dc.l    1,4,7,10,13,16,19,22,-1,-4,-7,-10,-13,-16,-19,-22
            dc.l    1,4,7,10,14,17,20,23,-1,-4,-7,-10,-14,-17,-20,-23
            dc.l    1,4,8,11,15,18,22,25,-1,-4,-8,-11,-15,-18,-22,-25
            dc.l    2,6,10,14,18,22,26,30,-2,-6,-10,-14,-18,-22,-26,-30
            dc.l    2,6,10,14,19,23,27,31,-2,-6,-10,-14,-19,-23,-27,-31
            dc.l    2,6,11,15,21,25,30,34,-2,-6,-11,-15,-21,-25,-30,-34
            dc.l    2,7,12,17,23,28,33,38,-2,-7,-12,-17,-23,-28,-33,-38
            dc.l    2,7,13,18,25,30,36,41,-2,-7,-13,-18,-25,-30,-36,-41
            dc.l    3,9,15,21,28,34,40,46,-3,-9,-15,-21,-28,-34,-40,-46
            dc.l    3,10,17,24,31,38,45,52,-3,-10,-17,-24,-31,-38,-45,-52
            dc.l    3,10,18,25,34,41,49,56,-3,-10,-18,-25,-34,-41,-49,-56
            dc.l    4,12,21,29,38,46,55,63,-4,-12,-21,-29,-38,-46,-55,-63
            dc.l    4,13,22,31,41,50,59,68,-4,-13,-22,-31,-41,-50,-59,-68
            dc.l    5,15,25,35,46,56,66,76,-5,-15,-25,-35,-46,-56,-66,-76
            dc.l    5,16,27,38,50,61,72,83,-5,-16,-27,-38,-50,-61,-72,-83
            dc.l    6,18,31,43,56,68,81,93,-6,-18,-31,-43,-56,-68,-81,-93
            dc.l    6,19,33,46,61,74,88,101,-6,-19,-33,-46,-61,-74,-88,-101
            dc.l    7,22,37,52,67,82,97,112,-7,-22,-37,-52,-67,-82,-97,-112
            dc.l    8,24,41,57,74,90,107,123,-8,-24,-41,-57,-74,-90,-107,-123
            dc.l    9,27,45,63,82,100,118,136,-9,-27,-45,-63,-82,-100,-118,-136
            dc.l    10,30,50,70,90,110,130,150,-10,-30,-50,-70,-90,-110,-130,-150
            dc.l    11,33,55,77,99,121,143,165,-11,-33,-55,-77,-99,-121,-143,-165
            dc.l    12,36,60,84,109,133,157,181,-12,-36,-60,-84,-109,-133,-157,-181
            dc.l    13,39,66,92,120,146,173,199,-13,-39,-66,-92,-120,-146,-173,-199
            dc.l    14,43,73,102,132,161,191,220,-14,-43,-73,-102,-132,-161,-191,-220
            dc.l    16,48,81,113,146,178,211,243,-16,-48,-81,-113,-146,-178,-211,-243
            dc.l    17,52,88,123,160,195,231,266,-17,-52,-88,-123,-160,-195,-231,-266
            dc.l    19,58,97,136,176,215,254,293,-19,-58,-97,-136,-176,-215,-254,-293
            dc.l    21,64,107,150,194,237,280,323,-21,-64,-107,-150,-194,-237,-280,-323
            dc.l    23,70,118,165,213,260,308,355,-23,-70,-118,-165,-213,-260,-308,-355
            dc.l    26,78,130,182,235,287,339,391,-26,-78,-130,-182,-235,-287,-339,-391
            dc.l    28,85,143,200,258,315,373,430,-28,-85,-143,-200,-258,-315,-373,-430
            dc.l    31,94,157,220,284,347,410,473,-31,-94,-157,-220,-284,-347,-410,-473
            dc.l    34,103,173,242,313,382,452,521,-34,-103,-173,-242,-313,-382,-452,-521
            dc.l    38,114,191,267,345,421,498,574,-38,-114,-191,-267,-345,-421,-498,-574
            dc.l    42,126,210,294,379,463,547,631,-42,-126,-210,-294,-379,-463,-547,-631
            dc.l    46,138,231,323,417,509,602,694,-46,-138,-231,-323,-417,-509,-602,-694
            dc.l    51,153,255,357,459,561,663,765,-51,-153,-255,-357,-459,-561,-663,-765
            dc.l    56,168,280,392,505,617,729,841,-56,-168,-280,-392,-505,-617,-729,-841
            dc.l    61,184,308,431,555,678,802,925,-61,-184,-308,-431,-555,-678,-802,-925
            dc.l    68,204,340,476,612,748,884,1020,-68,-204,-340,-476,-612,-748,-884,-1020
            dc.l    74,223,373,522,672,821,971,1120,-74,-223,-373,-522,-672,-821,-971,-1120
            dc.l    82,246,411,575,740,904,1069,1233,-82,-246,-411,-575,-740,-904,-1069,-1233
            dc.l    90,271,452,633,814,995,1176,1357,-90,-271,-452,-633,-814,-995,-1176,-1357
            dc.l    99,298,497,696,895,1094,1293,1492,-99,-298,-497,-696,-895,-1094,-1293,-1492
            dc.l    109,328,547,766,985,1204,1423,1642,-109,-328,-547,-766,-985,-1204,-1423,-1642
            dc.l    120,360,601,841,1083,1323,1564,1804,-120,-360,-601,-841,-1083,-1323,-1564,-1804
            dc.l    132,397,662,927,1192,1457,1722,1987,-132,-397,-662,-927,-1192,-1457,-1722,-1987
            dc.l    145,436,728,1019,1311,1602,1894,2185,-145,-436,-728,-1019,-1311,-1602,-1894,-2185
            dc.l    160,480,801,1121,1442,1762,2083,2403,-160,-480,-801,-1121,-1442,-1762,-2083,-2403
            dc.l    176,528,881,1233,1587,1939,2292,2644,-176,-528,-881,-1233,-1587,-1939,-2292,-2644
            dc.l    194,582,970,1358,1746,2134,2522,2910,-194,-582,-970,-1358,-1746,-2134,-2522,-2910
            dc.l    213,639,1066,1492,1920,2346,2773,3199,-213,-639,-1066,-1492,-1920,-2346,-2773,-3199
            dc.l    234,703,1173,1642,2112,2581,3051,3520,-234,-703,-1173,-1642,-2112,-2581,-3051,-3520
            dc.l    258,774,1291,1807,2324,2840,3357,3873,-258,-774,-1291,-1807,-2324,-2840,-3357,-3873
            dc.l    284,852,1420,1988,2556,3124,3692,4260,-284,-852,-1420,-1988,-2556,-3124,-3692,-4260
            dc.l    312,936,1561,2185,2811,3435,4060,4684,-312,-936,-1561,-2185,-2811,-3435,-4060,-4684
            dc.l    343,1030,1717,2404,3092,3779,4466,5153,-343,-1030,-1717,-2404,-3092,-3779,-4466,-5153
            dc.l    378,1134,1890,2646,3402,4158,4914,5670,-378,-1134,-1890,-2646,-3402,-4158,-4914,-5670
            dc.l    415,1246,2078,2909,3742,4573,5405,6236,-415,-1246,-2078,-2909,-3742,-4573,-5405,-6236
            dc.l    457,1372,2287,3202,4117,5032,5947,6862,-457,-1372,-2287,-3202,-4117,-5032,-5947,-6862
            dc.l    503,1509,2516,3522,4529,5535,6542,7548,-503,-1509,-2516,-3522,-4529,-5535,-6542,-7548
            dc.l    553,1660,2767,3874,4981,6088,7195,8302,-553,-1660,-2767,-3874,-4981,-6088,-7195,-8302
            dc.l    608,1825,3043,4260,5479,6696,7914,9131,-608,-1825,-3043,-4260,-5479,-6696,-7914,-9131
            dc.l    669,2008,3348,4687,6027,7366,8706,10045,-669,-2008,-3348,-4687,-6027,-7366,-8706,-10045
            dc.l    736,2209,3683,5156,6630,8103,9577,11050,-736,-2209,-3683,-5156,-6630,-8103,-9577,-11050
            dc.l    810,2431,4052,5673,7294,8915,10536,12157,-810,-2431,-4052,-5673,-7294,-8915,-10536,-12157
            dc.l    891,2674,4457,6240,8023,9806,11589,13372,-891,-2674,-4457,-6240,-8023,-9806,-11589,-13372
            dc.l    980,2941,4902,6863,8825,10786,12747,14708,-980,-2941,-4902,-6863,-8825,-10786,-12747,-14708
            dc.l    1078,3235,5393,7550,9708,11865,14023,16180,-1078,-3235,-5393,-7550,-9708,-11865,-14023,-16180
            dc.l    1186,3559,5932,8305,10679,13052,15425,17798,-1186,-3559,-5932,-8305,-10679,-13052,-15425,-17798
            dc.l    1305,3915,6526,9136,11747,14357,16968,19578,-1305,-3915,-6526,-9136,-11747,-14357,-16968,-19578
            dc.l    1435,4306,7178,10049,12922,15793,18665,21536,-1435,-4306,-7178,-10049,-12922,-15793,-18665,-21536
            dc.l    1579,4737,7896,11054,14214,17372,20531,23689,-1579,-4737,-7896,-11054,-14214,-17372,-20531,-23689
            dc.l    1737,5211,8686,12160,15636,19110,22585,26059,-1737,-5211,-8686,-12160,-15636,-19110,-22585,-26059
            dc.l    1911,5733,9555,13377,17200,21022,24844,28666,-1911,-5733,-9555,-13377,-17200,-21022,-24844,-28666
            dc.l    2102,6306,10511,14715,18920,23124,27329,31533,-2102,-6306,-10511,-14715,-18920,-23124,-27329,-31533
            dc.l    2312,6937,11562,16187,20812,25437,30062,34687,-2312,-6937,-11562,-16187,-20812,-25437,-30062,-34687
            dc.l    2543,7630,12718,17805,22893,27980,33068,38155,-2543,-7630,-12718,-17805,-22893,-27980,-33068,-38155
            dc.l    2798,8394,13990,19586,25183,30779,36375,41971,-2798,-8394,-13990,-19586,-25183,-30779,-36375,-41971
            dc.l    3077,9232,15388,21543,27700,33855,40011,46166,-3077,-9232,-15388,-21543,-27700,-33855,-40011,-46166
            dc.l    3385,10156,16928,23699,30471,37242,44014,50785,-3385,-10156,-16928,-23699,-30471,-37242,-44014,-50785
            dc.l    3724,11172,18621,26069,33518,40966,48415,55863,-3724,-11172,-18621,-26069,-33518,-40966,-48415,-55863
            dc.l    4095,12286,20478,28669,36862,45053,53245,61436,-4095,-12286,-20478,-28669,-36862,-45053,-53245,-61436
