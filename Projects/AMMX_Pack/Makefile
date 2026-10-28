PREFIX      = ../../Compilers/GCC-6.50-Latest
TARGET      = m68k-amigaos
CC          = $(PREFIX)/bin/m68k-amigaos-gcc
VASM        = $(PREFIX)/bin/vasmm68k_mot
ACP         = ../../Projects/ApolloExplorer/acp/acp
AMIGAHOST   = 192.168.178.50

CFLAGS      = -O2 -m68020 -m68881 -fomit-frame-pointer -noixemul
SFLAGS      = -Fhunk -m68080 -opt-speed -kick1hunks -quiet

TARGET_EXE  = AMMX_Pack

all: $(TARGET_EXE)

$(TARGET_EXE): main.o ammx_pack_c.o ammx_pack_asm.o
	$(CC) $(CFLAGS) -o $@ $^ -lm
	@echo "Build successful: $(TARGET_EXE)"

main.o: main.c ammx_pack.h
	$(CC) $(CFLAGS) -c $< -o $@

ammx_pack_c.o: ammx_pack_c.c ammx_pack.h
	$(CC) $(CFLAGS) -c $< -o $@

ammx_pack_asm.o: ammx_pack_asm.s
	$(VASM) $(SFLAGS) $< -o $@

clean:
	rm -f *.o $(TARGET_EXE)

upload: $(TARGET_EXE)
	@echo "Uploading $(TARGET_EXE) to $(AMIGAHOST):RAM/..."
	$(ACP) $(TARGET_EXE) $(AMIGAHOST):RAM/

run:
	@echo "Running $(TARGET_EXE) on $(AMIGAHOST)..."
	"/mnt/c/Program Files/ApolloExplorer/ash.exe" $(AMIGAHOST) "RAM:$(TARGET_EXE)"

.PHONY: all clean upload run
