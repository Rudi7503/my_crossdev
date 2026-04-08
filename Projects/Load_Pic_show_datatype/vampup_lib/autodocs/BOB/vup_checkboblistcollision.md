# NAME

    vup_CheckBOBListCollision -- Scans an entire list for intersection with a target.

# SYNOPSIS

    hitBob = vup_CheckBOBListCollision(sourceBob, targetList)
    d0                                   a0         a1

    struct vup_BOB *vup_CheckBOBListCollision(struct vup_BOB *, struct vup_BOB *);

# FUNCTION

    Takes a standalone BOB object (`sourceBob`) and loops through an active 
    sequence of geometry elements. It executes identical geometric bounding checks 
    using `vup_CheckBOBCollision` against every single node in the list.
    
    If the `sourceBob` is fundamentally part of the `targetList`, the routine
    identifies it by address and safely skips it to avert false-positive
    self-collision. 
    
    Evaluation short-circuits on the first successful hit and immediately
    returns the specific pointer of the contacted geometry object.

# INPUTS

    sourceBob     - Pointer to the structural entity moving into intersection.
    targetList    - Pointer to the root element of the entity chain to evaluate.

# RESULTS

    hitBob        - A direct pointer to the explicitly hit `vup_BOB`, or NULL
                    if no overlapping geometry was detected across the entire 
                    evaluated range.

# SEE ALSO

    vup_CheckBOBCollision()

