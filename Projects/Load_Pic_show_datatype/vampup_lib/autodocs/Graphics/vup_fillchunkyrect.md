# NAME

    vup_FillChunkyRect -- Rapidly flood-fills a 2D chunky pixel rectangle.

# SYNOPSIS

    vup_FillChunkyRect(dest,width,height,dstpitch,bytesperpixel,color)
                         a0   d0    d1     d2       d3            d4

    void vup_FillChunkyRect(UBYTE *, ULONG, ULONG, ULONG, ULONG, ULONG);

# FUNCTION

    Efficiently clears or fills a 2D block of chunky pixels directly into
    destination memory using high-bandwidth 64-bit AMMX loads and stores.
    It supports multiple bit depths (8-bit, 16-bit, 24-bit, 32-bit).

# INPUTS

    dest            - Pointer to the top-left pixel in the destination screen memory.
    width           - The physical width of the area to fill, in pixels.
    height          - The physical height of the area to fill, in rows.
    dstpitch        - The width of the entire destination bitmap in bytes.
    bytesperpixel   - Must be 1 (8-bit), 2 (16-bit), 3 (24-bit), or 4 (32-bit).
    color           - 32-bit ARGB color parameter defining the fill color.

# RESULTS

    None.

# NOTES

    This routine assumes a fast Apollo core memory bus layout. 

    Color Handling Exceptions:
    - 8-bit: Because mapping 32-bit TrueColor to a CyberGraphX screen palette 
      requires external ColorMap lookups, the routine expects the caller to pass 
      the exact 8-bit hardware Pen Index (0-255) in the lowest byte of the `color` (`d4`) 
      argument. It will not attempt to downmix a raw 32-bit RGB value into 8-bit limits.
    - 16-bit: The routine automatically packs the 32-bit ARGB input into
      a 16-bit RGB565 format internally via `PACK3216`.
    - 24-bit: The routine achieves fully unrolled, 100% destructive native AMMX bandwidth
      without needing `bsel` mask cycles, making 24-bit fills incredibly fast.

# SEE ALSO

    vup_DrawChunky(), vup_DrawChunkyMask()

