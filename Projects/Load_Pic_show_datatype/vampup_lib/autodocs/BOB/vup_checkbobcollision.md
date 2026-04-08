# NAME

    vup_CheckBOBCollision -- Evaluates overlap bounds between two entities.

# SYNOPSIS

    hit = vup_CheckBOBCollision(bobA, bobB)
    d0                            a0    a1

    LONG vup_CheckBOBCollision(struct vup_BOB *, struct vup_BOB *);

# FUNCTION

    Analyzes overlapping geometric bounds mathematically. It combines the 
    global `vb_WorldX / Y` coordinates with the explicit internal 
    `HitBox Offset / Width / Height` definitions, and natively checks for 
    overlap using short-circuit geometry checks.

# INPUTS

    bobA          - Pointer to the first entity.
    bobB          - Pointer to the second entity.

# RESULTS

    hit           - Returns 1 if the geometric bounds intersect, or 0 if safe.

# SEE ALSO

    vup_UpdateBOB()

