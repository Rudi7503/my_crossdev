#ifndef _INLINE_I2C_H
#define _INLINE_I2C_H

#ifndef __INLINE_MACROS_H
#include <inline/macros.h>
#endif

#ifndef I2C_BASE_NAME
#define I2C_BASE_NAME I2C_Base
#endif

#define AllocI2C(delaytype, name) \
	LP2(0x1e, BYTE, AllocI2C, UBYTE, delaytype, d0, STRPTR, name, a1, \
	, I2C_BASE_NAME)

#define BringBackI2C() \
	LP0(0x54, BYTE, BringBackI2C, \
	, I2C_BASE_NAME)

#define FreeI2C() \
	LP0NR(0x24, FreeI2C, \
	, I2C_BASE_NAME)

#define GetI2COpponent() \
	LP0(0x42, STRPTR, GetI2COpponent, \
	, I2C_BASE_NAME)

#define I2CErrText(errnum) \
	LP1(0x48, STRPTR, I2CErrText, ULONG, errnum, d0, \
	, I2C_BASE_NAME)

#define InitI2C() \
	LP0NR(0x30, InitI2C, \
	, I2C_BASE_NAME)

#define ReceiveI2C(addr, number, data) \
	LP3(0x3c, ULONG, ReceiveI2C, UBYTE, addr, d0, UWORD, number, d1, UBYTE *, data, a1, \
	, I2C_BASE_NAME)

#define SendI2C(addr, number, data) \
	LP3(0x36, ULONG, SendI2C, UBYTE, addr, d0, UWORD, number, d1, UBYTE *, data, a1, \
	, I2C_BASE_NAME)

#define SetI2CDelay(ticks) \
	LP1(0x2a, ULONG, SetI2CDelay, ULONG, ticks, d0, \
	, I2C_BASE_NAME)

#define ShutDownI2C() \
	LP0NR(0x4e, ShutDownI2C, \
	, I2C_BASE_NAME)

#endif /* _INLINE_I2C_H */
