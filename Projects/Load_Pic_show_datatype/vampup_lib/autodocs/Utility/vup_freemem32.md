# NAME

    vup_FreeMem32 -- Autonomously free an aligned memory buffer.

# SYNOPSIS

    vup_FreeMem32(memoryBlock)
                    a1

    void vup_FreeMem32(APTR);

# FUNCTION

    Releases a 32-byte aligned buffer constructed by `vup_AllocMem32` seamlessly 
    back to the operating system memory pools.

    Due to the `Vampup` dynamic tracking model, it is impossible for Amiga `FreeMem()` 
    to unpack the true structural dimensions alone. This Utility subroutine mathematically
    determines the hidden original pointer and exact byte size behind the scenes, preventing
    deadlock fragmentation logic loops.

    Note: This completely eliminates the need for developers to remember allocation lengths!

# INPUTS

    memoryBlock     - The aligned memory pointer originating from `vup_AllocMem32()`.
                      (Passing a standard unaligned Exec pointer or NULL yields 
                      catastrophic structural crashes. Do not mix and match allocators.)

# RESULTS

    None.

# SEE ALSO

    vup_AllocMem32(), exec.library/FreeMem()

