/*
 * vampup_protos.h
 * Inline C wrappers for vampup.library using m68k register-constrained
 * inline asm for the m68k-amigaos GCC cross compiler.
 *
 * AmigaOS calling convention:
 *   Library base is always in a6.
 *   Callee-saved by the library: d2-d7, a2-a5.
 *   Scratch (may be trashed): d0, d1, a0, a1.
 *   Return value: d0.
 *
 * Usage: All wrapper functions take VampupBase as the first parameter.
 */

#ifndef VAMPUP_PROTOS_H
#define VAMPUP_PROTOS_H

#include <exec/types.h>
#include <exec/libraries.h>
#include "vampup.h"

/* =========================================================================
 * Graphics Engine  (LVO bias -30)
 * ======================================================================= */

/*
 * vup_OpenDisplay(width, height, bpp)(d0, d1, d2)
 * Returns: vup_DisplayContext* in d0, or NULL on failure.
 */
static inline struct vup_DisplayContext *
VUP_OpenDisplay(struct Library *base, ULONG width, ULONG height, ULONG bpp)
{
    register ULONG _d0 __asm("d0") = width;
    register ULONG _d1 __asm("d1") = height;
    register ULONG _d2 __asm("d2") = bpp;
    register APTR  _a6 __asm("a6") = (APTR)base;
    __asm volatile ("jsr a6@(-30:w)"
        : "+r"(_d0)
        : "r"(_a6), "r"(_d1), "r"(_d2)
        : "cc", "memory", "d1", "a0", "a1");
    return (struct vup_DisplayContext *)_d0;
}

/*
 * vup_FlipDisplay(context)(a0)
 * Returns: APTR to the new (hidden) back buffer in d0.
 */
static inline APTR
VUP_FlipDisplay(struct Library *base, struct vup_DisplayContext *ctx)
{
    register APTR  _a0 __asm("a0") = (APTR)ctx;
    register APTR  _a6 __asm("a6") = (APTR)base;
    register ULONG _d0 __asm("d0");
    __asm volatile ("jsr a6@(-36:w)"
        : "=r"(_d0)
        : "r"(_a6), "r"(_a0)
        : "cc", "memory", "d1", "a1");
    return (APTR)_d0;
}

/*
 * vup_CloseDisplay(context)(a0)
 */
static inline void
VUP_CloseDisplay(struct Library *base, struct vup_DisplayContext *ctx)
{
    register APTR  _a0 __asm("a0") = (APTR)ctx;
    register APTR  _a6 __asm("a6") = (APTR)base;
    __asm volatile ("jsr a6@(-42:w)"
        :
        : "r"(_a6), "r"(_a0)
        : "cc", "memory", "d0", "d1", "a0", "a1");
}

/* =========================================================================
 * BOB Engine  (LVO bias -180)
 * ======================================================================= */

/*
 * vup_CreateBOB(pixelData, width, height, maxFrames, depth)(a0, d0, d1, d2, d3)
 * Returns: vup_BOB* in d0, or NULL on failure.
 */
static inline struct vup_BOB *
VUP_CreateBOB(struct Library *base,
              APTR pixelData, ULONG width, ULONG height,
              ULONG maxFrames, ULONG depth)
{
    register APTR  _a0 __asm("a0") = pixelData;
    register ULONG _d0 __asm("d0") = width;
    register ULONG _d1 __asm("d1") = height;
    register ULONG _d2 __asm("d2") = maxFrames;
    register ULONG _d3 __asm("d3") = depth;
    register APTR  _a6 __asm("a6") = (APTR)base;
    __asm volatile ("jsr a6@(-180:w)"
        : "+r"(_d0)
        : "r"(_a6), "r"(_a0), "r"(_d1), "r"(_d2), "r"(_d3)
        : "cc", "memory", "d1", "a0", "a1");
    return (struct vup_BOB *)_d0;
}

/*
 * vup_FreeBOB(bob)(a0)
 * Note: does NOT free vb_FrameData pixel buffer.
 */
static inline void
VUP_FreeBOB(struct Library *base, struct vup_BOB *bob)
{
    register APTR  _a0 __asm("a0") = (APTR)bob;
    register APTR  _a6 __asm("a6") = (APTR)base;
    __asm volatile ("jsr a6@(-186:w)"
        :
        : "r"(_a6), "r"(_a0)
        : "cc", "memory", "d0", "d1", "a0", "a1");
}

/*
 * vup_DrawBOB(bob, displayContext)(a0, a1)
 * Draws bob into the active back buffer of ctx.
 */
static inline void
VUP_DrawBOB(struct Library *base,
            struct vup_BOB *bob, struct vup_DisplayContext *ctx)
{
    register APTR  _a0 __asm("a0") = (APTR)bob;
    register APTR  _a1 __asm("a1") = (APTR)ctx;
    register APTR  _a6 __asm("a6") = (APTR)base;
    __asm volatile ("jsr a6@(-234:w)"
        :
        : "r"(_a6), "r"(_a0), "r"(_a1)
        : "cc", "memory", "d0", "d1", "a0", "a1");
}

/* =========================================================================
 * Utility & Memory  (LVO bias -330)
 * ======================================================================= */

/*
 * vup_FastMemClear(dest, size)(a0, d0)
 * Clears size bytes at dest to zero using 68080 pipelining.
 */
static inline void
VUP_FastMemClear(struct Library *base, APTR dest, ULONG size)
{
    register APTR  _a0 __asm("a0") = dest;
    register ULONG _d0 __asm("d0") = size;
    register APTR  _a6 __asm("a6") = (APTR)base;
    __asm volatile ("jsr a6@(-348:w)"
        :
        : "r"(_a6), "r"(_a0), "r"(_d0)
        : "cc", "memory", "d0", "d1", "a0", "a1");
}

/*
 * vup_LoadImage(filename, bpp)(a0, d0)
 * Loads image via datatypes and converts to requested BPP.
 *   bpp: 2=16-bit RGB565, 4=32-bit ARGB
 * Returns: vup_Image* in d0, or NULL on failure.
 *
 * Note: The shipped .fd and the autodoc disagree on the second argument
 * register for this early library drop. To stay compatible with both,
 * this wrapper writes bpp to d0 and mirrors it into a1.
 */
static inline struct vup_Image *
VUP_LoadImage(struct Library *base, STRPTR filename, ULONG bpp)
{
    register APTR  _a0 __asm("a0") = (APTR)filename;
    register ULONG _d0 __asm("d0") = bpp;
    register APTR  _a1 __asm("a1") = (APTR)bpp;
    register APTR  _a6 __asm("a6") = (APTR)base;
    __asm volatile ("jsr a6@(-384:w)"
        : "+r"(_d0)
        : "r"(_a6), "r"(_a0), "r"(_a1)
        : "cc", "memory", "d1", "a0", "a1");
    return (struct vup_Image *)_d0;
}

/*
 * vup_FreeImage(image)(a0)
 * Frees a vup_Image and its pixel buffer allocated by vup_LoadImage.
 */
static inline void
VUP_FreeImage(struct Library *base, struct vup_Image *image)
{
    register APTR  _a0 __asm("a0") = (APTR)image;
    register APTR  _a6 __asm("a6") = (APTR)base;
    __asm volatile ("jsr a6@(-390:w)"
        :
        : "r"(_a6), "r"(_a0)
        : "cc", "memory", "d0", "d1", "a0", "a1");
}

#endif /* VAMPUP_PROTOS_H */
