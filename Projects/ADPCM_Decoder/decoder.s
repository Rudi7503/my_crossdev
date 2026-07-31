; ==============================================================================
; STANDARD 68000 ADPCM 2-BIT STEREO DECODER & CYCLE COUNTER
; Kompilieren: vasmm68k_mot -m68080 -Fhunkexe decoder.s -o decoder.o
; ==============================================================================

            SECTION CODE,CODE

            XDEF    _get_ccc
            XDEF    _decode_2bit_stereo_ammx_asm

; ------------------------------------------------------------------------------
; uint32_t get_ccc(void);
; Liest den internen Zyklenzähler (Clock Cycle Counter) der Vampire (68080) aus.
; ------------------------------------------------------------------------------
_get_ccc:
            movec   ccc,d0          ; Speichere aktuellen Zählerstand in D0 (Return-Wert für C)
            rts                     ; Zurück zum C-Programm

; ------------------------------------------------------------------------------
; uint32_t decode_2bit_stereo_ammx_asm(const uint8_t *in, int16_t *out, 
;                                      ADPCM_Channel *chL, ADPCM_Channel *chR, 
;                                      uint32_t block_cnt);
; ------------------------------------------------------------------------------
_decode_2bit_stereo_ammx_asm:
            ; 1. Sichern der Callee-Saved Register (Regel des C-Compilers)
            ; Wir sichern 11 Register (D2-D7 und A2-A6). 11 * 4 Bytes = 44 Bytes auf dem Stack.
            movem.l d2-d7/a2-a6,-(a7)

            ; 2. Parameter vom Stack lesen
            ; Der Stack-Pointer (a7) steht jetzt auf: 44 Bytes (unsere Register) + 4 Bytes (Rücksprungadresse) = 48.
            ; Ab Offset 48 liegen die 5 Parameter (jeweils 32-Bit / 4 Bytes groß), die C übergeben hat:
            move.l  48(a7),a0     ; Parameter 1: Pointer auf in_buffer (Start des 15-Byte Blocks)
            move.l  52(a7),a1     ; Parameter 2: Pointer auf out_buffer (Ziel für PCM-Daten)
            move.l  56(a7),a2     ; Parameter 3: Pointer auf chL (Status-Struct Kanal Links)
            move.l  60(a7),a3     ; Parameter 4: Pointer auf chR (Status-Struct Kanal Rechts)
            move.l  64(a7),d6     ; Parameter 5: block_cnt (Anzahl der 15-Byte-Blöcke)

            ; Lade die Basisadresse unserer Schrittweiten-Tabelle (step_table) in Register A4
            lea     step_table(pc),a4

            ; --- START GESCHWINDIGKEITSMESSUNG ---
            movec   ccc,a6        ; Speichere die aktuellen Zyklen vor der Schleife in A6

            ; Prüfe, ob block_cnt (D6) gleich 0 ist. Wenn ja, springe sofort zum Ende.
            tst.l   d6
            beq     .done

.block_loop:
            ; Ein Block besteht aus 15 Bytes. Für den DBF-Befehl zählen wir von 14 bis 0.
            moveq   #14,d7        
.byte_loop:
            ; Hole 1 Byte aus dem in_buffer (A0) und erhöhe den Pointer A0 direkt um 1 (Post-Increment).
            move.b  (a0)+,d5      

            ; ------------------------------------------------------------------
            ; Ein 2-Bit ADPCM Byte enthält 4 Samples: [R1][L1][R0][L0]
            ; Bits 0-1 = Links 0 | Bits 2-3 = Rechts 0 | Bits 4-5 = Links 1 | Bits 6-7 = Rechts 1
            ; ------------------------------------------------------------------

            ; --- PAAR 1: Links (Bits 0-1) ---
            move.w  d5,d0         ; Kopiere das Byte in D0 (Arbeitsregister für das Nibble)
            and.w   #3,d0         ; Maskiere mit 3 (binär 00000011). Nur Bits 0 und 1 bleiben erhalten.
            move.l  a2,a5         ; Übergebe den Pointer für den LINKEN Kanal (aus A2) an A5
            bsr     .decode_nibble; Springe zur Mathe-Routine (Rückkehrwert PCM liegt danach in D1)
            move.w  d1,(a1)+      ; Schreibe das 16-Bit PCM-Sample in out_buffer (A1) und setze Pointer weiter

            ; --- PAAR 1: Rechts (Bits 2-3) ---
            move.w  d5,d0         ; Kopiere das ursprüngliche Byte erneut nach D0
            lsr.w   #2,d0         ; Schiebe die Bits um 2 Stellen nach rechts (Bits 2-3 sind jetzt auf 0-1)
            and.w   #3,d0         ; Maskiere wieder mit 3
            move.l  a3,a5         ; Übergebe den Pointer für den RECHTEN Kanal (aus A3) an A5
            bsr     .decode_nibble; Dekodieren...
            move.w  d1,(a1)+      ; PCM in Puffer schreiben (damit liegt L und R nun abwechselnd im RAM)

            ; --- PAAR 2: Links (Bits 4-5) ---
            move.w  d5,d0
            lsr.w   #4,d0         ; Schiebe Bits 4-5 ganz nach unten
            and.w   #3,d0
            move.l  a2,a5         ; Wieder LINKER Kanal
            bsr     .decode_nibble
            move.w  d1,(a1)+

            ; --- PAAR 2: Rechts (Bits 6-7) ---
            move.w  d5,d0
            lsr.w   #6,d0         ; Schiebe die obersten Bits 6-7 ganz nach unten
            and.w   #3,d0
            move.l  a3,a5         ; Wieder RECHTER Kanal
            bsr     .decode_nibble
            move.w  d1,(a1)+

            ; Dekrementiere D7. Wenn nicht -1, springe zurück zu .byte_loop (verarbeitet 15 Bytes)
            dbf     d7,.byte_loop

            ; Ein 15-Byte-Block ist fertig. Zähle block_cnt (D6) runter. Wenn nicht 0, nächster Block.
            subq.l  #1,d6
            bne     .block_loop

.done:
            ; --- ENDE GESCHWINDIGKEITSMESSUNG ---
            movec   ccc,d7        ; Aktuelle Zyklen nach der Schleife in D7 holen
            sub.l   a6,d7         ; D7 = D7 (Ende) - A6 (Start) = Benötigte Zyklen für die Blöcke
            move.l  d7,d0         ; Zyklen in D0 verschieben (D0 ist immer das C-Return-Register!)

            ; Register vom Stack wiederherstellen und zurück zu C springen
            movem.l (a7)+,d2-d7/a2-a6
            rts

; ------------------------------------------------------------------------------
; LOKALE SUBROUTINE: .decode_nibble
; Übersetzt das ADPCM-Nibble in ein lineares PCM-Sample. 
; Entspricht exakt der Logik aus der C-Funktion `decode_nibble()`.
; 
; Eingabe : D0 = Nibble (0 bis 3) | A5 = Pointer auf das jeweilige ADPCM_Channel Struct
;           A4 = Pointer auf step_table
; Ausgabe : D1 = fertiges PCM Sample
; ------------------------------------------------------------------------------
.decode_nibble:
            ; Das Struct in C sieht so aus: struct { int32_t pcm; int16_t index; }
            ; Offset 0 ist pcm (4 Bytes), Offset 4 ist index (2 Bytes).
            move.l  (a5),d1       ; Lese aktuelles ch->pcm in D1 (32-Bit)
            move.w  4(a5),d2      ; Lese aktuellen ch->index in D2 (16-Bit)

            ; C: int32_t step = step_table[chan->index];
            move.w  d2,d4         ; Kopiere index nach D4
            add.w   d4,d4         ; Verdopple D4 (Index * 2), da jeder Eintrag in der Tabelle 2 Bytes (Word) groß ist
            move.w  (a4,d4.w),d3  ; Lese den Wert aus der Tabelle an Position (A4 + D4) in D3 (step)

            ; C: delta = step >> 1;
            move.l  d3,d4         ; Kopiere step nach D4 (D4 wird unser delta)
            lsr.l   #1,d4         ; Schiebe D4 um 1 Bit nach rechts (entspricht Teile durch 2)

            ; C: if (nibble & 1) delta += step;
            btst    #0,d0         ; Teste Bit 0 im Nibble (D0)
            beq     .no_add       ; Wenn das Bit 0 ist (beq = Branch if Equal to Zero), überspringe Addition
            add.l   d3,d4         ; Andernfalls: delta = delta + step
.no_add:
            ; C: if (nibble & 2) pcm -= delta; else pcm += delta;
            btst    #1,d0         ; Teste Bit 1 (Wertigkeits-Bit für das Vorzeichen) im Nibble
            beq     .add_delta    ; Wenn 0, ist es positiv -> springe zur Addition
            sub.l   d4,d1         ; Wenn 1, ist es negativ -> pcm = pcm - delta
            bra     .clamp_pcm    ; Springe weiter zum Clamping
.add_delta:
            add.l   d4,d1         ; pcm = pcm + delta

.clamp_pcm:
            ; C: chan->pcm = clamp(chan->pcm, -32768, 32767);
            cmp.l   #32767,d1     ; Vergleiche pcm (D1) mit dem Maximum +32767
            ble     .check_min    ; Wenn kleiner oder gleich (ble), ist das Max ok, prüfe Minimum
            move.l  #32767,d1     ; Wenn größer, kappe harten auf +32767
            bra     .update_idx   ; Gehe weiter
.check_min:
            cmp.l   #-32768,d1    ; Vergleiche pcm (D1) mit dem Minimum -32768
            bge     .update_idx   ; Wenn größer oder gleich (bge), ist das Min ok
            move.l  #-32768,d1    ; Wenn kleiner, kappe auf -32768

.update_idx:
            move.l  d1,(a5)       ; Speichere das neue, ge-clampte pcm zurück in die C-Struktur (Offset 0)

            ; C: chan->index += (nibble & 1) * 3 - 1;
            ; (nibble & 1) ergibt entweder 0 oder 1. 
            ; Bei 0: (0 * 3) - 1 = -1 (Index sinkt). Bei 1: (1 * 3) - 1 = +2 (Index steigt).
            move.w  d0,d4         ; Kopiere Nibble
            and.w   #1,d4         ; Isoliere Bit 0 (Ergebnis in D4 ist nun 0 oder 1)
            move.w  d4,d3         ; Speichere Kopie in D3
            lsl.w   #1,d3         ; Schiebe D3 um 1 nach links (entspricht D3 = D4 * 2)
            add.w   d3,d4         ; D4 = D4 + D3 (entspricht D4 = D4 * 3)
            subq.w  #1,d4         ; D4 = D4 - 1
            add.w   d4,d2         ; Addiere das Ergebnis (-1 oder +2) auf den aktuellen index (D2)

            ; C: chan->index = clamp(chan->index, 0, 88);
            tst.w   d2            ; Prüfe, ob index (D2) kleiner 0 ist
            bge     .idx_max      ; Wenn größer oder gleich 0, prüfe Maximum
            moveq   #0,d2         ; Wenn kleiner, kappe auf 0
            bra     .idx_done
.idx_max:
            cmp.w   #88,d2        ; Prüfe, ob index (D2) größer als 88 ist
            ble     .idx_done     ; Wenn kleiner/gleich, alles gut
            move.w  #88,d2        ; Wenn größer, kappe auf 88
.idx_done:
            move.w  d2,4(a5)      ; Speichere den neuen index zurück in die C-Struktur (Offset 4)
            rts                   ; Zurück zur Aufrufer-Schleife (.byte_loop)

; ------------------------------------------------------------------------------
; ADPCM STEP TABELLE
; Exakte Kopie des C-Arrays 'step_table'. Enthält die 89 möglichen Schrittweiten.
; ------------------------------------------------------------------------------
step_table:
            dc.w    7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31
            dc.w    34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143
            dc.w    157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658
            dc.w    724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024
            dc.w    3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899
            dc.w    15289,16818,18500,20350,22385,24623,27086,29794,32767

            END