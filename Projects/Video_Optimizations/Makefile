CC = ../../Compilers/GCC-6.50-Dev/bin/m68k-amigaos-gcc
AS = ../../Compilers/GCC-6.50-Dev/bin/vasmm68k_mot
CFLAGS = -noixemul -O2 -m68020 -m68881 -ffast-math -fomit-frame-pointer -I. \
         -I../../Compilers/GCC-6.50-Dev/m68k-amigaos/libnix/include \
         -I../../Compilers/GCC-6.50-Dev/m68k-amigaos/sys-include \
         -I../../Compilers/GCC-6.50-Dev/m68k-amigaos/ndk32-include/Include_H
ASFLAGS = -Fhunk -m68080 -m68881 -opt-speed -spaces
LDFLAGS = -noixemul -lm

TARGET = VideoOpt
OBJS = main.o opt_c.o opt_asm.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(AS) $(ASFLAGS) -o $@ $<

clean:
	rm -f *.o $(TARGET)
