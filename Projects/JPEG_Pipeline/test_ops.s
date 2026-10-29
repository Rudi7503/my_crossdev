        machine 68080
        section .text,code
test_shifts:
        psllw   #4,e0,e1
        psrlw   #4,e0,e1
        psraw   #4,e0,e1
        lsl.w   #4,d0
        rts
