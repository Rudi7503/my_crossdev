# NAME

    vup_DrawBOB -- Main pipeline renderer for entities into the double buffer.

# SYNOPSIS

    vup_DrawBOB(bob, displayContext)
                  a0   a1

    void vup_DrawBOB(struct vup_BOB *, struct vup_DisplayContext *);

# FUNCTION

    The core rendering routine for the object subsystem. Automatically executes:
    1. Camera Translation (`ScreenX = WorldX - CameraX`).
    2. Zero-cycle Culling (Aborts natively if completely off-screen).
    3. Memory boundaries clipping (safely limits edges if partially visible).
    4. Pointer evaluation into the active Double Buffer.
    5. Native invocation of the underlying `vup_DrawChunky` or 
       `vup_DrawChunkyMask` layer (based on the `BOB_MASKED` flag).

# INPUTS

    bob           - Pointer to the object to render.
    displayContext - The active display tracker containing the backbuffer setup.

# RESULTS

    None.

# SEE ALSO

    vup_DrawBOBList()

