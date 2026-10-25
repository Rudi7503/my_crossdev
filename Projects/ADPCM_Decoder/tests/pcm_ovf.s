; pcm_ovf.s - isolated test of 16-bit PCM add/sub + overflow clamp
; int pcm_ovf(int pcm, int delta, int neg)
;   neg=0: pcm + delta
;   neg=1: pcm - delta
; Returns clamped int16 value in d0. Saves the full callee-saved set.
            xdef    _pcm_ovf
_pcm_ovf:
            movem.l d2-d7/a2-a6,-(a7)
            move.l  48(a7),d2
            move.l  52(a7),d6
            move.l  56(a7),d0
            tst.w   d0
            bne.s   .neg
            add.w   d6,d2
            bra.s   .chk
.neg:
            sub.w   d6,d2
.chk:
            bvc.s   .done
            tst.w   d0
            bne.s   .min
            move.w  #32767,d2
            bra.s   .done
.min:
            move.w  #-32768,d2
.done:
            ext.l   d2
            move.l  d2,d0
            movem.l (a7)+,d2-d7/a2-a6
            rts
