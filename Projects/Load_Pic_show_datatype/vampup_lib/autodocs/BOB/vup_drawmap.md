# NAME

    vup_DrawMap -- Render a tilemap/level chunk using native Camera offsets.

# SYNOPSIS

    vup_DrawMap(mapImage, displayContext)
                  a0        a1

    void vup_DrawMap(struct vup_Image *, struct vup_DisplayContext *);

# FUNCTION

    Safely extracts a perfectly constrained view array directly from a massive 
    background image into the globally sized `displayContext` back buffer. Computes exact 
    start offsets naturally mapped to `vdc_CameraX` and `vdc_CameraY`, and 
    invokes the fast underlying AMMX copy logic.
    
    The physical hardware render constraints are evaluated organically from 
    `vdc_Width` and `vdc_Height` in the target context.

# INPUTS

    mapImage      - The source vup_Image containing the full level map.
    displayContext - Display context tracking active Camera constraints.

# RESULTS

    None.

# SEE ALSO

    vup_DrawBOB()

