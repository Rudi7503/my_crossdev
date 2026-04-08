# NAME

    vup_UpdateBOBList -- Progresses velocity and frames for an entire linked array.

# SYNOPSIS

    vup_UpdateBOBList(startingBob)
                        a0

    void vup_UpdateBOBList(struct vup_BOB *);

# FUNCTION

    A wrapper function that sequentially traverses a connected `vup_BOB` structural
    chain, executing physics updates on every node. It iteratively
    calls `vup_UpdateBOB` on each element until the `vb_Next` pointer evaluation
    yields NULL.

# INPUTS

    startingBob   - Pointer to the first BOB node of the sequence.

# RESULTS

    None.

# SEE ALSO

    vup_UpdateBOB()

