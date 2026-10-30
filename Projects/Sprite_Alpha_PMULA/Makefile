CC = ../../Compilers/GCC-6.50-Latest/bin/m68k-amigaos-gcc
AS = ../../Compilers/GCC-6.50-Latest/bin/vasmm68k_mot
CFLAGS = -noixemul -O2 -m68020 -m68881 -fno-inline
ASFLAGS = -Fhunk -m68080 -opt-speed
TARGET = PMULATest
OBJS = pmula_test.o pmula_asm.o
all: $(TARGET)
$(TARGET): $(OBJS)
	$(CC) -noixemul $(OBJS) -o $(TARGET)
pmula_test.o: pmula_test.c
	$(CC) -c $(CFLAGS) -o pmula_test.o pmula_test.c
pmula_asm.o: pmula_asm.s
	$(AS) $(ASFLAGS) -o pmula_asm.o pmula_asm.s
clean:
	rm -f *.o $(TARGET)
