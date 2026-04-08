# NAME

    vup_ScaleImage -- High-speed chunky pixel fixed-point 2D shape scaler.
    vup_ScaleImageA -- Struct wrapper variant for C/High-Level Languages.

# SYNOPSIS

    vup_ScaleImage(src,dest,src_w,src_h,dest_w,dest_h,src_p,dst_p,bpp)
                     a0  a1   d0    d1    d2     d3     d4    d5    d6

    void vup_ScaleImage(UBYTE *, UBYTE *, ULONG, ULONG, ULONG, ULONG, ULONG, ULONG, ULONG);

    vup_ScaleImageA(scaleArgStruct)
                      a0

    void vup_ScaleImageA(struct vup_ScaleArgs *);

# FUNCTION

    Resizes a 2D chunky pixel image to any arbitrary width and height 
    using entirely native 68080 Integer pipelines.

    By utilizing purely 16.16 Fixed-Point math, it completely avoids expensive 
    divisional floating point interpolations inside the drawing loops. 
    The scaler inherently performs "Nearest-Neighbor" (Point Sampled) sizing
    to guarantee true pixel-art aesthetics without blur overhead penalty.

    The two function entry points are functionally identical. `vup_ScaleImage` 
    is meant for Assembly coders demanding fast raw registers. `vup_ScaleImageA` allows 
    C/Pascal compilers to pass a single parameter structure (`vup_ScaleArgs`), safely 
    avoiding the "d0-d6 register exhaustion" limits inherent to HLL compilers.

# INPUTS

    src             - Pointer to the top-left pixel of the source image.
    dest            - Pointer to the top-left pixel in the destination screen memory.
    src_w / src_h   - Physical width and height of the original source image.
    dest_w / dest_h - Desired destination width and height requested.
    src_p           - Byte width of the entire source bitmap memory.
    dst_p           - Byte width of the entire destination bitmap memory.
    bpp             - Must be 1 (8-bit), 2 (16-bit), 3 (24-bit), or 4 (32-bit).

# RESULTS

    None.

# SEE ALSO

    vup_DrawChunky()

