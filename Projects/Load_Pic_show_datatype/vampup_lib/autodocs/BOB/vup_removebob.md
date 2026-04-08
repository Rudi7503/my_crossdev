# NAME

    vup_RemoveBOB -- Disconnects a vup_BOB safely from its sequence.

# SYNOPSIS

    vup_RemoveBOB(bob)
                    a0

    void vup_RemoveBOB(struct vup_BOB *);

# FUNCTION

    Automatically severs the targeted `vup_BOB` structure out of its active
    `vb_Next` / `vb_Prev` traversal chain. The object logically disconnects 
    natively, and its surviving neighbors organically bridge together to prevent
    array fragmentation.
    
    This function simply untangles the pointers; it does not clear memory.

# INPUTS

    bob           - Pointer to the BOB to detach.

# RESULTS

    None.

# SEE ALSO

    vup_AddBOB(), vup_FreeBOB()

