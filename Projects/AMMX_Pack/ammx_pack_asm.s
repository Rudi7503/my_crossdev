; ==============================================================================
; AMMX PACKSSWB EMULATION (Signed Saturation Word-to-Byte)
;
; Funktionen:
;   - _pack_signed_words_baseline_asm (8 Worte = 16 Bytes In -> 8 Bytes Out)
;   - _pack_signed_words_opt_asm      (16 Worte = 32 Bytes In -> 16 Bytes Out, 2x Unrolled)
;   - _get_ccc                        (68080 Cycle Counter)
;
; C-Signatur:
;   void pack_signed_words_baseline_asm(const int16_t *in, int8_t *out, uint32_t blocks);
;   void pack_signed_words_opt_asm(const int16_t *in, int8_t *out, uint32_t blocks);
; ==============================================================================

            SECTION CODE,CODE

            XDEF    _pack_signed_words_baseline_asm
            XDEF    _pack_signed_words_opt_asm
            XDEF    _get_ccc

_get_ccc:
            movec   ccc,d0
            rts

; ==============================================================================
; 1. BASELINE: Korrigierter Bias ($0080 / $8080), 8 Worte pro Block
; ==============================================================================
_pack_signed_words_baseline_asm:
            move.l  4(a7),a0                ; a0 = in
            move.l  8(a7),a1                ; a1 = out
            move.l  12(a7),d0               ; d0 = block_count
            beq.s   .base_done

            load.w  #$0080,e2               ; e2 = $0080 0080 0080 0080 (+128 pro Wort)
            load.w  #$8080,e3               ; e3 = $8080 8080 8080 8080 (-128 pro Byte)

.base_loop:
            paddw   (a0)+,e2,e0             ; 4 Worte laden & +128
            paddw   (a0)+,e2,e1             ; 4 Worte laden & +128
            packuswb e0,e1,e0               ; 8 Worte -> 8 unsigned Bytes (0..255)
            psubb   e3,e0,e0                ; -128 -> signed [-128..+127]
            store   e0,(a1)+                ; 8 Bytes speichern

            subq.l  #1,d0
            bne.s   .base_loop

.base_done:
            rts

; ==============================================================================
; 2. OPTIMIERT: 2x Unrolled & Interleaved Pipeline (16 Worte pro Block)
; Verschachtelt zwei unabhängige Blöcke A (e0/e1) und B (e4/e5),
; um RAW-Pipeline-Stalls zwischen paddw, packuswb und psubb zu eliminieren.
; ==============================================================================
_pack_signed_words_opt_asm:
            move.l  4(a7),a0                ; a0 = in
            move.l  8(a7),a1                ; a1 = out
            move.l  12(a7),d0               ; d0 = block_count (1 Block = 16 Worte = 32 Bytes)
            beq.s   .opt_done

            load.w  #$0080,e2
            load.w  #$8080,e3

.opt_loop:
            ; Phase 1: 8 Worte Block A & 8 Worte Block B laden & vorspannen
            paddw   (a0)+,e2,e0             ; Worte 0..3 Block A
            paddw   (a0)+,e2,e1             ; Worte 4..7 Block A
            paddw   (a0)+,e2,e4             ; Worte 0..3 Block B
            paddw   (a0)+,e2,e5             ; Worte 4..7 Block B

            ; Phase 2: Unsigned Saturation Packen (0..255)
            packuswb e0,e1,e0               ; 8 Bytes Block A
            packuswb e4,e5,e4               ; 8 Bytes Block B

            ; Phase 3: Bias abziehen -> Signed Two's Complement (-128..+127)
            psubb   e3,e0,e0
            psubb   e3,e4,e4

            ; Phase 4: 16 Bytes wegschreiben
            store   e0,(a1)+
            store   e4,(a1)+

            subq.l  #1,d0
            bne.s   .opt_loop

.opt_done:
            rts
