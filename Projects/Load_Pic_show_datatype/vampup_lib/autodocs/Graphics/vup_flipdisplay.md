# NAME

    vup_FlipDisplay -- Swaps video buffers natively synced to hardware VBLANK.

# SYNOPSIS

    vup_FlipDisplay(context)
                      a0

    APTR vup_FlipDisplay(APTR);

# FUNCTION

    Executes a high-speed double-buffer hardware toggle without invoking 
    graphics library memory allocations in the inner loop. 
    
    This function utilizes `ChangeScreenBuffer` to push the underlying RTG physical
    base pointer to the video hardware, and immediately stalls the CPU executing
    `WaitTOF` to force absolute synchronization with the monitor's raster beam 
    (eliminating screen tearing entirely).

    The Front and Back pointers inside the `vup_DisplayContext` are mathematically 
    reversed, and the completely hidden back-buffer base pointer is immediately
    returned securely to the CPU for the next frame's asynchronous rendering.

# INPUTS

    context - A valid `vup_DisplayContext` previously returned by `vup_OpenDisplay`.

# RESULTS

    d0      - The explicit 32-bit hardware physical Base Pointer to the new, fully 
              hidden drawing buffer, perfectly safe for destructive 68080 CPU clears 
              and rendering.

# SEE ALSO

    vup_OpenDisplay()

