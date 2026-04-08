# vup_Convert32To16

## Description
Converts a 32-bit ARGB array to 16-bit RGB565.

Parses a continuous array of 32-bit pixels natively mapping them down to 16-bit RGB565 formatting. Utilizes lightning-fast 68080 integer logical shifts mapping arrays securely globally minimizing system memory reads securely utilizing robust integer `swap` execution paths cleanly bypassing any hardware saturation clipping explicitly natively.

Note: The routine executes purely on explicit binary bit-shifts. It does NOT possess format auto-detection. The input array MUST be structured as pure ARGB (`PBPAFMT_ARGB` natively). Passing RGBA will result in visual channel mapping crashes natively.

## Synopsis
`vup_Convert32To16(src, dest, pixels)(a0, a1, d0)`

## Inputs
- **src** (`a0`) - Source 32-bit Array Pointer (Must be exactly ARGB)
- **dest** (`a1`) - Destination 16-bit Array Pointer (Receives RGB565)
- **pixels** (`d0`) - Total Number of Pixels to Convert (width * height)

## Results
- **None**. The destination array is fully populated natively.

