CC = ../../Compilers/GCC-6.50-Dev/bin/m68k-amigaos-gcc
AS = ../../Compilers/GCC-6.50-Dev/bin/vasmm68k_mot
CFLAGS = -noixemul -O2 -m68020 -m68881 -ffast-math -fomit-frame-pointer -I.
ASFLAGS = -Fhunk -m68080 -opt-speed

TARGET = SpriteTransAMMX
OBJS = main.o blit_trans_c.o blit_trans_asm.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -noixemul $(OBJS) -lm -o $(TARGET)

main.o: main.c blit_trans.h
	$(CC) -c $(CFLAGS) -o main.o main.c

blit_trans_c.o: blit_trans_c.c blit_trans.h
	$(CC) -c $(CFLAGS) -o blit_trans_c.o blit_trans_c.c

blit_trans_asm.o: blit_trans_asm.s
	$(AS) $(ASFLAGS) -o blit_trans_asm.o blit_trans_asm.s

clean:
	rm -f *.o $(TARGET)
