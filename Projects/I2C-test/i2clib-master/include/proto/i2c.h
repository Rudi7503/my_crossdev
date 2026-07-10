#ifndef PROTO_I2C_H
#define PROTO_I2C_H

#ifndef EXEC_LIBRARIES_H
#include <exec/libraries.h>
#endif

#include <clib/i2c_protos.h>
#ifndef __GNUC__
#include <inline/i2c_vbcc.h>
#else
#include <inline/i2c_gnuc.h>
#endif

#ifndef __NOLIBBASE__
extern struct Library * I2C_Base;
#endif

#endif
