I2CLIB_VARIANT=apollo

INCLUDE_I=/opt/amiga/m68k-amigaos/ndk-include/
INCLUDE_H=/opt/amiga/m68k-amigaos/ndk-include/
ASM68K=vasmm68k_mot
#LD=vlink
LD=vlink -s		#link and strip
RM=rm -f
CC=vc +aos68k
CFLAGS=-O2 -I$(INCLUDE_H) -Iinclude
TARGETS=SendI2C ReceiveI2C I2CScan

ASM68KFLAGS=-m68000 -Fhunk 
ASM68KFLAGS+=-I$(INCLUDE_I) 

all : i2c.library $(TARGETS)

i2c.library : i2c.library.o
	$(LD) -o $@ $<

%.o : src/%.$(I2CLIB_VARIANT).s src/$(I2CLIB_VARIANT).i
	$(ASM68K) $(ASM68KFLAGS) -o $@ $<

SendI2C : src/SendI2C.c
	$(CC) $(CFLAGS) -lamiga -o $@ $<

ReceiveI2C : src/ReceiveI2C.c
	$(CC) $(CFLAGS) -lamiga -o $@ $<

I2CScan : src/I2CScan.c
	$(CC) $(CFLAGS) -lamiga -o $@ $<

clean :
	$(RM) *.o *.library $(TARGETS)

ifndef INCLUDE_I
$(error INCLUDE_I environemnt variable is undefined, set it to AmigaOS NDK assembly includes)
endif

ifndef INCLUDE_H
$(error INCLUDE_H environemnt variable is undefined, set it to AmigaOS NDK header includes)
endif
