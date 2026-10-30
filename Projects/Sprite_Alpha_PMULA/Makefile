CC = ../../Compilers/GCC-6.50-Latest/bin/m68k-amigaos-gcc
AS = ../../Compilers/GCC-6.50-Latest/bin/vasmm68k_mot
CFLAGS = -noixemul -O2 -m68020 -m68881 -fno-inline
ASFLAGS = -Fhunk -m68080 -opt-speed
TARGET = PMULATest
RGB565 = RGB565Test
OBJS = pmula_test.o pmula_asm.o
all: $(TARGET) $(RGB565)
$(TARGET): $(OBJS)
	$(CC) -noixemul $(OBJS) -o $(TARGET)
pmula_test.o: pmula_test.c
	$(CC) -c $(CFLAGS) -o pmula_test.o pmula_test.c
pmula_asm.o: pmula_asm.s
	$(AS) $(ASFLAGS) -o pmula_asm.o pmula_asm.s
clean:
	rm -f *.o $(TARGET) $(RGB565)

$(RGB565): rgb565_test.o
	$(CC) -noixemul rgb565_test.o -o $(RGB565)
rgb565_test.o: rgb565_test.c
	$(CC) -c $(CFLAGS) -o rgb565_test.o rgb565_test.c
