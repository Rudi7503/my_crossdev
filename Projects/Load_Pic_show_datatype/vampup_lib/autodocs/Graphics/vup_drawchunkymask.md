# NAME

    vup_DrawChunkyMask -- 2D chunky pixel blit with transparent color keying.

# SYNOPSIS

    vup_DrawChunkyMask(src,dest,width,height,srcpitch,dstpitch,bytesperpixel,colorkey)
                         a0  a1   d0    d1     d2       d3       d4            d5

    void vup_DrawChunkyMask(UBYTE *, UBYTE *, ULONG, ULONG, ULONG, ULONG, ULONG, ULONG);

# FUNCTION

    Efficiently copies a 2D block of chunky pixels from a source buffer
    to a destination buffer, while skipping source pixels that match the
    supplied transparent `colorkey`.
    
    This function intrinsically handles 8-bit, 16-bit, 24-bit, and 32-bit data streams
    by checking the `bytesperpixel` parameter. For 8 and 16 bit, it harnesses 
    high-bandwith 64-bit AMMX loads alongside the powerful `pcmp` & `bsel` combo 
    to perform rapid Read-Modify-Write blitting at 8 bytes per iteration.
    For 32-bit depth, it drops into an optimized dual-pipe integer copy loop 
    to handle true-color key comparisons safely.

# INPUTS

    src             - Pointer to the top-left pixel of the source chunky buffer.
    dest            - Pointer to the top-left pixel in the destination screen memory.
    width           - The physical width of the area to copy, in pixels.
    height          - The physical height of the area to copy, in rows.
    srcpitch        - The width of the entire source bitmap in bytes (modulo + width).
    dstpitch        - The width of the entire destination bitmap in bytes.
    bytesperpixel   - Must be 1 (8-bit), 2 (16-bit), 3 (24-bit), or 4 (32-bit).
    colorkey        - The transparent color. If a pixel equals this value,
                      it is completely ignored, preserving the destination.

# RESULTS

    None.

# SEE ALSO

    vup_DrawChunky()

