# vup_LoadDataFile()

## NAME
`vup_LoadDataFile()` -- Load a known data file directly into an AMMX aligned tracking buffer.

## SYNOPSIS
```assembly
Ptr = vup_LoadDataFile( a0 = FilePath )
```

## DESCRIPTION
Loads a binary file directly from disk into memory. 
This function is explicitly designed to load custom game formats, known archive sets, or structured files where the developer intrinsically knows the data lengths and simply requires the file rapidly shifted into physical RAM natively securely aligned dynamically.

The function uses `dos.library` to open the specified file, calculates its exact size internally, allocates a 32-byte aligned buffer using `vup_AllocMem32()`, reads the file contents directly into this buffer, and closes the file handle.

Because the memory is allocated internally using `vup_AllocMem32()`, the caller must strictly use `vup_FreeMem32()` to free the returned memory buffer when it is absolutely no longer utilized natively.

## INPUTS
- `a0` (Pointer): Null-terminated string containing the absolute or relative file path (e.g., `RAM:assets/data.bin`).

## OUTPUTS
- `d0` (Pointer): Pointer to the loaded file data array. Returns `NULL` (0) if the file could not be read natively or if memory allocation explicitly failed.

## SEE ALSO
* [`vup_AllocMem32()`](vampupallocmem32.md)
* [`vup_FreeMem32()`](vampupfreemem32.md)

