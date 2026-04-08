# NAME

    vup_GetChunkyPos -- Calculates a 2D destination pointer dynamically.

# SYNOPSIS

    vup_GetChunkyPos(base,x,y,pitch,bytesperpixel)
                       a0   d0 d1 d2    d3

    UBYTE * vup_GetChunkyPos(UBYTE *, ULONG, ULONG, ULONG, ULONG);

# FUNCTION

    A Utility mathematics function that calculates the exact 
    memory address of any X,Y coordinate inside a chunky pixel buffer. 

    While this math is trivial in higher level languages, this 68080-optimized 
    call relieves Assembly and Basic programmers from manually performing 
    boiler-plate layout calculations for sprite blitting coordinates.

    The math executes strictly using high-speed integer multiplication without 
    destroying any calling arguments (a0, d2 and d3 are preserved), keeping it 
    compliant with standard Amiga architectural guidelines, and allowing code
    re-use without constantly reloading the screen base pointer!

# INPUTS

    base            - The absolute start pointer (Coordinate 0,0) of the target buffer.
    x               - The X coordinate (Pixels).
    y               - The Y coordinate (Rows).
    pitch           - The byte width of a single row in the buffer.
    bytesperpixel   - Must be 1 (8-bit), 2 (16-bit), 3 (24-bit), or 4 (32-bit).

# RESULTS

    d0              - Returns the absolute memory pointer representing the target coordinates.
                      Note: The `base` pointer provided in `a0` is mathematically preserved, 
                      allowing sequential calls without reloading the base buffer pointer!

# SEE ALSO

    vup_DrawChunky(), vup_FastScroll()

