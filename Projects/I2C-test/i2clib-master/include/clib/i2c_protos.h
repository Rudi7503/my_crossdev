#ifndef  CLIB_I2C_PROTOS_H
#define  CLIB_I2C_PROTOS_H

/*
**      $VER: i2c_protos.h 39.4 (16.03.97)
**
**      C function prototypes
**
*/

#ifndef  EXEC_TYPES_H
#include <exec/types.h>
#endif

BYTE AllocI2C(UBYTE Delay_Type, STRPTR Name);
void FreeI2C(void);
ULONG SetI2CDelay(ULONG ticks);
void InitI2C(void);
ULONG SendI2C(UBYTE addr, UWORD number, UBYTE *i2cdata);
ULONG ReceiveI2C(UBYTE addr, UWORD number, UBYTE *i2cdata);
STRPTR GetI2COpponent(void);
STRPTR I2CErrText(ULONG errnum);
void ShutDownI2C(void);
BYTE BringBackI2C(void);

#endif  /* CLIB_I2C_PROTOS_H */

