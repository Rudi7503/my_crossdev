# vup_FastMemClear

## NAME
    vup_FastMemClear -- Clear a memory block using 68080 pipelining.

## SYNOPSIS
    vup_FastMemClear(Dest, Size)
                       a0    d0

    VOID vup_FastMemClear(APTR, ULONG);

## FUNCTION
    Clears memory to zero using fused instructions targeting the hardware 
    dual-execution bus pipeline.

## INPUTS
    Dest - Pointer to the target memory block.
    Size - Memory block size in bytes.

## RESULT
    None.

## NOTES
    This routine evaluates memory in 8-byte fused loops natively. Any 
    remaining trailing bytes are securely handled dynamically via a remainder wiping loop.

## SEE ALSO
    vampupfastmemcopy()

