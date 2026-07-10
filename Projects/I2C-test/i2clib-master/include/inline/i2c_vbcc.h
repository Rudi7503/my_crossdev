#ifndef _INLINE_I2C_H
#define _INLINE_I2C_H

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif

void __FreeI2C(__reg("a6") struct Library *)="\tjsr\t-36(a6)";
#define FreeI2C() __FreeI2C(I2C_Base)

ULONG __SetI2CDelay(__reg("a6") struct Library *, __reg("d0") ULONG ticks)="\tjsr\t-42(a6)";
#define SetI2CDelay(ticks) __SetI2CDelay(I2C_Base, (ticks))

void __InitI2C(__reg("a6") struct Library *)="\tjsr\t-48(a6)";
#define InitI2C() __InitI2C(I2C_Base)

ULONG __SendI2C(__reg("a6") struct Library *, __reg("d0") UBYTE addr, __reg("d1") UWORD number, __reg("a1") void * data)="\tjsr\t-54(a6)";
#define SendI2C(addr, number, data) __SendI2C(I2C_Base, (addr), (number), (void *)(data))

ULONG __ReceiveI2C(__reg("a6") struct Library *, __reg("d0") UBYTE addr, __reg("d1") UWORD number, __reg("a1") void * data)="\tjsr\t-60(a6)";
#define ReceiveI2C(addr, number, data) __ReceiveI2C(I2C_Base, (addr), (number), (void *)(data))

STRPTR __GetI2COpponent(__reg("a6") struct Library *)="\tjsr\t-66(a6)";
#define GetI2COpponent() __GetI2COpponent(I2C_Base)

STRPTR __I2CErrText(__reg("a6") struct Library *, __reg("d0") ULONG errnum)="\tjsr\t-72(a6)";
#define I2CErrText(errnum) __I2CErrText(I2C_Base, (errnum))

void __ShutDownI2C(__reg("a6") struct Library *)="\tjsr\t-78(a6)";
#define ShutDownI2C() __ShutDownI2C(I2C_Base)

BYTE __BringBackI2C(__reg("a6") struct Library *)="\tjsr\t-84(a6)";
#define BringBackI2C() __BringBackI2C(I2C_Base)

#endif /*  _INLINE_I2C_H  */
