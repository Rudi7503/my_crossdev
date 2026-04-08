# NAME

    vup_CreateBOB -- Allocates and initializes a vup_BOB structure.

# SYNOPSIS

    bob = vup_CreateBOB(pixelData, width, height, maxFrames, depth)
    d0                    a0         d0     d1      d2         d3

    struct vup_BOB *vup_CreateBOB(APTR, ULONG, ULONG, ULONG, ULONG);

# FUNCTION

    Dynamically allocates a 32-bit aligned vup_BOB structure and natively 
    populates the core geometry tracking fields. It mathematically 
    computes the `vb_FrameSize` by multiplying the dimensions and depth natively.

# INPUTS

    pixelData     - Pointer to the source pixel array for the frames.
    width         - Visual width of a single animation frame.
    height        - Visual height of a single animation frame.
    maxFrames     - The total number of animation frames available.
    depth         - The byte depth of the pixel format (e.g., 4 for 32-bit).

# RESULTS

    bob           - A pointer to the initialized vup_BOB structure, or NULL
                    if memory allocation failed.

# SEE ALSO

    vup_FreeBOB()

