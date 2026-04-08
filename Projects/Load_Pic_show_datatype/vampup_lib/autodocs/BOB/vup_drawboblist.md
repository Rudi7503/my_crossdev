# NAME

    vup_DrawBOBList -- Iteratively renders a sequential chain of BOBs.

# SYNOPSIS

    vup_DrawBOBList(startingBob, displayContext)
                      a0           a1

    void vup_DrawBOBList(struct vup_BOB *, struct vup_DisplayContext *);

# FUNCTION

    A fast monolithic execution wrapper. Parses a dynamically chained sequence 
    of elements starting at `startingBob`, and executes `vup_DrawBOB` upon every 
    active structure sequentially until it hits NULL. Rendering matches the chain 
    traversal logically (Painter's Algorithm natively).

# INPUTS

    startingBob    - Pointer to the first element in the BOB sequence.
    displayContext - Pointer to the target DisplayContext.

# RESULTS

    None.

# SEE ALSO

    vup_DrawBOB(), vup_AddBOB()

