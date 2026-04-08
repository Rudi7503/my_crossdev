# vup_FastMemCopy

## NAME
    vup_FastMemCopy -- Copy a memory block using 68080 integer pipelining.

## SYNOPSIS
    vup_FastMemCopy(Source, Dest, Size)
                      a0      a1    d0

    VOID vup_FastMemCopy(APTR, APTR, ULONG);

## FUNCTION
    Copies a memory block taking advantage of 68080 integer pipeline fusing. 
    It provides 8-byte transfer throughput without strict 16-byte address 
    alignment constraints.

## INPUTS
    Source - Pointer to the beginning of the source data.
    Dest   - Pointer to the destination data block.
    Size   - Memory block size in bytes.

## RESULT
    None.

## NOTES
    This routine evaluates memory in 8-byte fused loops natively. Any 
    remaining trailing bytes are securely handled dynamically via a byte-copy remainder loop.

    **WARNING:** To achieve maximum execution speed, this function performs 
    a strictly forward-marching copy and does not evaluate bounds for overlaps! 
    If the Source and Destination buffers structurally overlap, and the Dest 
    pointer is physically higher than the Source pointer, this routine will 
    overwrite and corrupt the source data before it can be copied. Use 
    `exec.library/CopyMem` natively for overlapping array manipulation.

## SEE ALSO
    vampupfastmemclear()

