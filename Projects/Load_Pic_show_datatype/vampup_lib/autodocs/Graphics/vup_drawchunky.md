# NAME

    vup_DrawChunky -- Blasts a 2D chunky pixel buffer directly to screen memory.

# SYNOPSIS

    vup_DrawChunky(src,dest,width,height,srcpitch,dstpitch,bytesperpixel)
                     a0  a1   d0    d1     d2       d3       d4

    void vup_DrawChunky(UBYTE *, UBYTE *, ULONG, ULONG, ULONG, ULONG, ULONG);

# FUNCTION

    Efficiently copies a 2D block of chunky pixels from a source buffer
    to a destination buffer using high-bandwidth 64-bit AMMX loads and stores.
    The copy uses tightly looped instructions without unrolling to take
    advantage of the 68080's second instruction pipe execution.
    It supports multiple bit depths (8-bit, 16-bit, 24-bit, 32-bit).

# INPUTS

    src             - Pointer to the top-left pixel of the source chunky buffer.
    dest            - Pointer to the top-left pixel in the destination screen memory.
    width           - The physical width of the area to copy, in pixels.
    height          - The physical height of the area to copy, in rows.
    srcpitch        - The width of the entire source bitmap in bytes (modulo + width).
    dstpitch        - The width of the entire destination bitmap in bytes.
    bytesperpixel   - Must be 1 (8-bit), 2 (16-bit), 3 (24-bit), or 4 (32-bit).

# RESULTS

    None.

# NOTES

    This routine assumes a fast Apollo core memory bus layout and leverages
    pipeline fusing. It eliminates stack pushes by temporarily holding loop 
    state variables in the Extended E10-E13 registers, which are preserved.

# SEE ALSO

    vup_FillChunkyRect(), vup_DrawChunkyMask()

