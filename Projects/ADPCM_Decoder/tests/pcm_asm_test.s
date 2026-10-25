; pcm_asm_test.s - pure-ASM exhaustive check of add.w/sub.w + overflow clamp
; uint32_t pcm_ovf_check_asm(void)
; Returns number of successful cases (917504) or 0xFFFFFFFF on mismatch.
; Reference is computed in 32-bit ASM, not C.
            xdef    _pcm_ovf_check_asm
_pcm_ovf_check_asm:
            movem.l d2-d7/a2-a6,-(a7)
            lea     delta_tab,a2
            moveq   #0,d3              ; success count
            moveq   #6,d6              ; 7 deltas
.delta_loop:
            moveq   #0,d4
            move.w  (a2)+,d4           ; delta 0..32767
            moveq   #1,d7              ; sign: 1=sub, then 0=add
.sign_loop:
            move.w  #65535,d0          ; 65536 PCM values
.pcm_loop:
            move.w  d0,d2
            sub.w   #32768,d2          ; pcm = -32768..32767

            ; actual: add.w/sub.w + V clamp
            move.w  d2,d5
            tst.w   d7
            bne.s   .aneg
            add.w   d4,d5
            bra.s   .achk
.aneg:
            sub.w   d4,d5
.achk:
            bvc.s   .adone
            tst.w   d7
            bne.s   .amin
            move.w  #32767,d5
            bra.s   .adone
.amin:
            move.w  #-32768,d5
.adone:
            ext.l   d5

            ; expected: 32-bit add/sub + 32-bit clamp
            move.w  d2,d1
            ext.l   d1
            tst.w   d7
            bne.s   .eneg
            add.l   d4,d1
            bra.s   .echk
.eneg:
            sub.l   d4,d1
.echk:
            cmp.l   #32767,d1
            ble.s   .elow
            move.l  #32767,d1
            bra.s   .edone
.elow:
            cmp.l   #-32768,d1
            bge.s   .edone
            move.l  #-32768,d1
.edone:
            cmp.l   d1,d5
            bne.s   .fail
            addq.l  #1,d3
            dbf     d0,.pcm_loop
            dbf     d7,.sign_loop
            dbf     d6,.delta_loop

            move.l  d3,d0
            movem.l (a7)+,d2-d7/a2-a6
            rts
.fail:
            moveq   #-1,d0
            movem.l (a7)+,d2-d7/a2-a6
            rts

            even
delta_tab:
            dc.w    0,1,2,16383,16384,32766,32767
