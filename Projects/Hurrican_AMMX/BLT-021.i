; seq/BLT-021.i - 512-Bit Full Cacheline Burst Copy (4x MOVE16, 32 RGB565 pixels)
; Uses: a0, a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
; Copies exactly 64 bytes (1 full 68080 cacheline burst) directly via cache controller
        move16  (a0)+,(a1)+
        move16  (a0)+,(a1)+
        move16  (a0)+,(a1)+
        move16  (a0)+,(a1)+
