# NAME

    vup_FreeBOB -- Severs and frees a vup_BOB structure.

# SYNOPSIS

    vup_FreeBOB(bob)
                  a0

    void vup_FreeBOB(struct vup_BOB *);

# FUNCTION

    Detaches the specified `vup_BOB` from any linked node chain using
    `vup_RemoveBOB` and frees the explicit 32-bit aligned structure 
    memory block dynamically.
    
    Note: This function does NOT attempt to free the `vb_FrameData` pixel buffer, 
    as pixel buffers are typically shared across multiple structure instances natively.

# INPUTS

    bob           - Pointer to the vup_BOB structure to free.

# RESULTS

    None.

# SEE ALSO

    vup_FreeBOBChain(), vup_RemoveBOB()

