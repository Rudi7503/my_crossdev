# NAME

    vup_AddBOB -- Inserts a vup_BOB safely into a working linked sequence.

# SYNOPSIS

    vup_AddBOB(parentBob, newBob)
                 a0         a1

    void vup_AddBOB(struct vup_BOB *, struct vup_BOB *);

# FUNCTION

    Attaches `newBob` sequentially directly after `parentBob` inside a dynamic
    vup_BOB node chain. It automatically manages the `vb_Next` and
    `vb_Prev` pointer logic natively. If `parentBob` is already attached to 
    trailing neighbors, the boundaries seamlessly dynamically bridge between them.

# INPUTS

    parentBob     - Pointer to the preceding BOB in the sequence. Null is not explicitly tracked.
    newBob        - Pointer to the newly allocated structure.

# RESULTS

    None.

# SEE ALSO

    vup_RemoveBOB(), vup_CreateBOB()

