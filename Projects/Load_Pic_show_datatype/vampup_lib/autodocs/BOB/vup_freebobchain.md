# NAME

    vup_FreeBOBChain -- Safely destructs an entire dynamically linked array.

# SYNOPSIS

    vup_FreeBOBChain(startingBob)
                       a0

    void vup_FreeBOBChain(struct vup_BOB *);

# FUNCTION

    Starts at the targeted node and securely traverses down the `vb_Next` array,
    sequentially invoking `vup_FreeBOB` on every node mathematically until 
    the chain safely resolves to NULL.

# INPUTS

    startingBob   - Pointer to the first element in the target chain.

# RESULTS

    None.

# SEE ALSO

    vup_FreeBOB()

