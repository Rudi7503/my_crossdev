; seq/BLT-038.i - 24-Bit RGB888 Fill (24 Bytes = 8 pixels, 3x store with rotated RGB pattern)
; Uses: e2, e3, e4 (preloaded RGBRGBRG, BRGBRGBR, GBRGBRGB), a1
; MUST NOT touch: d5, d6, d7, a5, a6, a7
        store   e2,(a1)+
        store   e3,(a1)+
        store   e4,(a1)+
