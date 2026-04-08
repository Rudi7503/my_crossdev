# vup_LoadImage()

## NAME
`vup_LoadImage()` -- Natively open and decode an image via standard Datatypes.

## SYNOPSIS
```assembly
vup_Image* = vup_LoadImage( a0 = Filename, d0 = TargetBPP )
```

## DESCRIPTION
Loads an image via `datatypes.library` and returns a `vup_Image` structure containing the pixel data and image dimensions. 

To ensure consistent pixel formats across different Datatypes implementations, this function extracts the image as a 32-bit ARGB array and converts it to the requested `TargetBPP`:
- **32-Bit Target**: Returns the image as a 32-bit ARGB array. If Datatypes returns a packed 24-bit bitmap, the function automatically expands it to 32-bit (0RGB) to ensure memory alignment.
- **24-Bit Target**: Not natively used for hardware blitting. Passing 3 acts as an alias for 4, returning a 32-bit ARGB array instead.
- **16-Bit Target**: Allocates a temporary 32-bit array during extraction, then uses `vup_Convert32To16` to return a 16-bit RGB565 array.
- **8-Bit Target**: Not currently implemented. Converting higher bit-depth images to an 8-bit indexed palette requires color quantization logic which is not present in this function.

## INPUTS
- `a0` (Pointer): Null-terminated string containing the file path.
- `d0` (Unsigned Long): The requested Target Bytes Per Pixel (BPP):
   * `2` = 16-bit RGB565
   * `3` = Promoted to 4-byte ARGB
   * `4` = 32-bit ARGB
   * *(Passing 1 for 8-bit is not supported)*

## OUTPUTS
- `d0` (Pointer): Hardware address to the allocated `vup_Image` structure containing `Width`, `Height`, `Pitch`, and the `vi_PixelData` pointer. Returns `NULL` if execution failed (e.g., file missing, Datatype unsupported, or out of memory).

## SYSTEM REQUIREMENTS
Because of the powerful chunk-mapping requirements bypassing intuition buffers, the underlying framework dynamically binds `PDTM_READPIXELARRAY`. This requires **V43+ datatypes.library** (Amiga OS 3.9 natively, or identically ApolloOS standard bounds).

## SEE ALSO
* [`vup_FreeImage()`](vampupfreeimage.md)
* [`vup_FillChunkyRect()`](vampupfillchunkyrect.md)

