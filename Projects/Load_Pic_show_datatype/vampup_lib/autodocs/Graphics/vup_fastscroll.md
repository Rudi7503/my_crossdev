# NAME

    vup_FastScroll -- Shifting chunky graphics safely inside overlapping memory.

# SYNOPSIS

    vup_FastScroll(src,dest,width,height,srcpitch,dstpitch,bytesperpixel)
                     a0  a1   d0    d1     d2       d3       d4

    void vup_FastScroll(UBYTE *, UBYTE *, ULONG, ULONG, ULONG, ULONG, ULONG);

# FUNCTION

    Safely copies a 2D block of chunky pixels from anywhere to anywhere, even if the 
    source and destination memory regions overlap. 

    If you try to move a background image down by 5 pixels using `vup_DrawChunky`, 
    it will overwrite the bottom lines of the image before reading them! 
    `vup_FastScroll` acts as a 2D `memmove()`. It intelligently compares the 
    pointers: if they overlap in a way that would destroy pixels, it automatically 
    reverses the drawing direction (bottom-right to top-left) to protect the image.

    This function inherently supports rendering at multiple bit depths 
    without extra penalty.

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

# SEE ALSO

    vup_DrawChunky()

