CC = ../../Compilers/GCC-6.50-Dev/bin/m68k-amigaos-gcc
AS = ../../Compilers/GCC-6.50-Dev/bin/vasmm68k_mot
CFLAGS = -noixemul -O2 -m68020 -m68881 -ffast-math -fomit-frame-pointer -I. -I../../Compilers/GCC-6.50-Dev/m68k-amigaos/libnix/include -I../../Compilers/GCC-6.50-Dev/m68k-amigaos/sys-include -I../../Compilers/GCC-6.50-Dev/m68k-amigaos/ndk32-include/Include_H
ASFLAGS = -Fhunk -m68080 -m68881 -opt-speed -spaces

TARGET = MPEG
OBJS = main.o mpeg_c.o mpeg_asm.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -noixemul $(OBJS) -lm -o $(TARGET)

main.o: main.c mpeg.h
	$(CC) -c $(CFLAGS) -o main.o main.c

mpeg_c.o: mpeg_c.c mpeg.h
	$(CC) -c $(CFLAGS) -o mpeg_c.o mpeg_c.c

mpeg_asm.o: mpeg_asm.s
	$(AS) $(ASFLAGS) -o mpeg_asm.o mpeg_asm.s

clean:
	rm -f *.o $(TARGET)
