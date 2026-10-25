; seq/MEM-002.i - isolated TOUCH cost (scheduling distance is documented)
; Uses: d0, a0
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        moveq   #0,d0
        touch   (a0,d0.w)
