# NAME

    vup_UpdateBOB -- Processes entity velocity and animation states.

# SYNOPSIS

    vup_UpdateBOB(bob)
                    a0

    void vup_UpdateBOB(struct vup_BOB *);

# FUNCTION

    Progresses the logical bounds of the entity frame-by-frame. 
    Applies the `vb_VelX` and `vb_VelY` vectors mathematically to the global 
    `vb_WorldX` and `vb_WorldY` coordinates.
    Additionally, it increments the internal `vb_AnimTick` timer against 
    the `vb_AnimSpeed` limit. If the threshold is reached, it securely 
    advances `vb_CurrentFrame`, applying modulo wrapping against `vb_MaxFrames`.

# INPUTS

    bob           - Pointer to the active vup_BOB structure.

# RESULTS

    None.

# SEE ALSO

    vup_DrawBOB()

