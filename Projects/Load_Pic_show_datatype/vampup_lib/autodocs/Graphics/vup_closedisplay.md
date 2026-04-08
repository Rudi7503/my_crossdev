# NAME

    vup_CloseDisplay -- Safely destructs hardware screen buffers and tracking contexts.

# SYNOPSIS

    vup_CloseDisplay(context)
                       a0

    void vup_CloseDisplay(APTR);

# FUNCTION

    Safely decouples the Front and Back screen buffers from the CyberGraphics Intuition
    subsystem, closes the screen, and immediately frees the 
    `vup_DisplayContext` memory block cleanly back into the system pool.

# INPUTS

    context - A valid `vup_DisplayContext` pointer returned by `vup_OpenDisplay`.

# RESULTS

    None.

# SEE ALSO

    vup_OpenDisplay()

