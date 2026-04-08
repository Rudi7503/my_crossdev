# NAME

    vup_OpenDisplay -- Opens a hardware-accelerated CyberGraphics double-buffered screen.

# SYNOPSIS

    vup_OpenDisplay(width, height, bpp)
                      d0     d1      d2

    APTR vup_OpenDisplay(ULONG, ULONG, ULONG);

# FUNCTION

    Initializes an Intuition screen and allocates a native CyberGraphics double-buffer
    chain optimized for high-performance SAGA 68080 rendering. 
    
    This function fundamentally bypasses traditional Amiga planar display paradigms 
    and forces the hardware into a unified linear chunky buffer suitable for pure 
    pixel array manipulation. The active physical hardware tracking pointers 
    are immediately locked into the returned context, exposing zero-overhead
    drawing functionality to the developer.

# INPUTS

    width   - The screen width in pixels (e.g., 640, 1280).
    height  - The screen height in pixels (e.g., 480, 720).
    bpp     - Bytes per pixel (1=8bit, 2=16bit, 3=24bit, 4=32bit).

# RESULTS

    d0      - A pointer to an initialized `vup_DisplayContext` structure, 
              or NULL if VRAM was exhausted or mode was unsupported.

# SEE ALSO

    vup_FlipDisplay(), vup_CloseDisplay()

