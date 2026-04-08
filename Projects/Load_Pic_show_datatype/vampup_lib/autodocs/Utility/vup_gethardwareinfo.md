# vup_GetHardwareInfo

## NAME
    vup_GetHardwareInfo -- Retrieve a pointer to the static vup_Hardware structure.

## SYNOPSIS
    HardwareStruct = vup_GetHardwareInfo()
    d0

    APTR vup_GetHardwareInfo(VOID);

## FUNCTION
    Returns a pointer to the cached `vup_Hardware` structure populated 
    during library initialization (`LibInit`). 

    Because the Apollo Core capabilities are detected during boot via 
    `AttnFlags` and SAGA register reads, this function does not incur 
    dynamic calculation overhead.

    The `vup_Hardware` structure is defined as follows:
    ```c
    struct vup_Hardware {
        UBYTE vh_Is68080;         // Offset 0: 1 if CPU is 68080
        UBYTE vh_IsV2;            // Offset 1: 1 if V2 hardware
        UBYTE vh_IsV4;            // Offset 2: 1 if V4 class (supports Arne Audio)
        UBYTE vh_CardCode;        // Offset 3: Raw Card ID (e.g., 5=V4SA, 8=Unicorn)
        ULONG vh_ClockMultiplier; // Offset 4: Base MHz multiplier
    };
    ```

## INPUTS
    None.

## RESULT
    d0 - A 32-bit APTR pointing to a read-only `vup_Hardware` structure.

## NOTES
    The library validates the current architecture during execution of
    LibInit. These fields are considered static for the session.

## SEE ALSO
    include/vampup.i

