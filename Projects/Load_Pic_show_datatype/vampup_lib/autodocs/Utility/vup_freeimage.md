# vup_FreeImage()

## NAME
`vup_FreeImage()` -- Deallocate dynamically mapped image arrays cleanly.

## SYNOPSIS
```assembly
vup_FreeImage( a0 = vup_Image* )
```

## DESCRIPTION
Deconstructs a `vup_Image` structure securely instantiated previously by `vup_LoadImage()`. 
This function physically detaches and destroys internal VRAM / Memory chunks via `vup_FreeMem32` cleanly avoiding standard SysBase bottlenecks on 32-bit aligned arrays, and systematically frees the primary tracker structure back to ExecBase.

## INPUTS
- `a0` (Pointer): Hardware bounding pointer returning the exact dynamic structurally mounted `vup_Image*` address returned during loading initialization.

## OUTPUTS
None. System termination routine.

## SEE ALSO
* [`vup_LoadImage()`](vampuploadimage.md)

