# NAME

    vup_AllocMem32 -- Allocate 32-byte cache-aligned system RAM.

# SYNOPSIS

    vup_AllocMem32(byteSize,requirements)
                     d0       d1

    APTR vup_AllocMem32(ULONG, ULONG);

# FUNCTION

    Requests a contiguous block of system memory from the Amiga `exec.library` 
    and automatically calculates and pushes the pointer sequentially until it aligns
    perfectly on a secure 32-byte hardware boundary.

    Passing 32-byte aligned buffers into graphic functions (like `DrawChunkyMask` 
    or `ScaleImage`) enables the 68080 and SAGA architectures to exploit hyper-fast 
    parallel integers and AMMX burst-reads, dramatically increasing throughput compared
    to misaligned random byte pointers. 

    The returned pointer MUST be freed natively with `vup_FreeMem32` instead of 
    calling standard Exec `FreeMem`.

# INPUTS

    byteSize        - The size of the desired data allocation in bytes.
    requirements    - Standard Exec allocation flags (`MEMF_CHIP`, `MEMF_FAST`, `MEMF_CLEAR`).

# RESULTS

    d0              - The 32-byte mathematically aligned pointer ready for data, 
                      or NULL dynamically if memory is fully exhausted.

# SEE ALSO

    vup_FreeMem32(), exec.library/AllocMem()

