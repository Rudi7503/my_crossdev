* Copy a selected tile from a tile atlas to a screen buffer line by line
* using destination modulo (stride) for the next output line.
* Variants: 16x16, 32x32, 64x64, 128x128, 256x256 in 32bit ARGB, 24bit RGB, 16bit.
* C call (REGPARM) for all variants:
*   A0 = tile source base address (pointer, 32-bit address)
*   A1 = screen destination address (pointer, 32-bit address)
*   D0 = tile number (lower 16-bit/word used)
*   D1 = destination modulo in bytes per line (32-bit long)
* Clobbers: A0, A1, D0, D1

	XDEF _Copy_Tile_16x16x32bit_to_Screen
	XDEF _Copy_Tile_16x16x24bit_to_Screen
	XDEF _Copy_Tile_16x16x16bit_to_Screen
	XDEF _Copy_Tile_32x32x32bit_to_Screen
	XDEF _Copy_Tile_32x32x24bit_to_Screen
	XDEF _Copy_Tile_32x32x16bit_to_Screen
	XDEF _Copy_Tile_64x64x32bit_to_Screen
	XDEF _Copy_Tile_64x64x24bit_to_Screen
	XDEF _Copy_Tile_64x64x16bit_to_Screen
	XDEF _Copy_Tile_128x128x32bit_to_Screen
	XDEF _Copy_Tile_128x128x24bit_to_Screen
	XDEF _Copy_Tile_128x128x16bit_to_Screen
	XDEF _Copy_Tile_256x256x32bit_to_Screen
	XDEF _Copy_Tile_256x256x24bit_to_Screen
	XDEF _Copy_Tile_256x256x16bit_to_Screen
	CNOP 0,4

* ------------------------------ 16x16 ------------------------------

_Copy_Tile_16x16x32bit_to_Screen:
	mulu.w #1024,d0				* D0 = tile index * 1024 bytes (16*16*4)
	adda.l d0,a0				* A0 = start of selected tile
	sub.l #64,d1				* D1 = modulo - bytes copied per tile line (16*4)
	move.l #15,d0				* 16 lines loop counter for DBRA
.copy_tile_16x16x32_to_screen_loop:
	move16 (a0)+,(a1)+			* Pixels 1-4
	move16 (a0)+,(a1)+			* Pixels 5-8
	move16 (a0)+,(a1)+			* Pixels 9-12
	move16 (a0)+,(a1)+			* Pixels 13-16
	adda.l d1,a1
	dbra.l d0,.copy_tile_16x16x32_to_screen_loop
	rts

_Copy_Tile_16x16x24bit_to_Screen:
	mulu.w #768,d0				* D0 = tile index * 768 bytes (16*16*3)
	adda.l d0,a0
	sub.l #48,d1				* D1 = modulo - bytes copied per tile line (16*3)
	move.l #15,d0
.copy_tile_16x16x24_to_screen_loop:
	move16 (a0)+,(a1)+			* Pixels 1-5 (+1 byte)
	move16 (a0)+,(a1)+			* Pixels 6-10 (+2 bytes)
	move16 (a0)+,(a1)+			* Pixels 11-16
	adda.l d1,a1
	dbra.l d0,.copy_tile_16x16x24_to_screen_loop
	rts

_Copy_Tile_16x16x16bit_to_Screen:
	mulu.w #512,d0				* D0 = tile index * 512 bytes (16*16*2)
	adda.l d0,a0
	sub.l #32,d1				* D1 = modulo - bytes copied per tile line (16*2)
	move.l #15,d0
.copy_tile_16x16x16_to_screen_loop:
	move16 (a0)+,(a1)+			* Pixels 1-8
	move16 (a0)+,(a1)+			* Pixels 9-16
	adda.l d1,a1
	dbra.l d0,.copy_tile_16x16x16_to_screen_loop
	rts

* ------------------------------ 32x32 ------------------------------

_Copy_Tile_32x32x32bit_to_Screen:
	mulu.w #4096,d0				* D0 = tile index * 4096 bytes (32*32*4)
	adda.l d0,a0
	sub.l #128,d1				* D1 = modulo - bytes copied per tile line (32*4)
	move.l #31,d0
.copy_tile_32x32x32_to_screen_loop:
	move16 (a0)+,(a1)+			* Pixels 1-4
	move16 (a0)+,(a1)+			* Pixels 5-8
	move16 (a0)+,(a1)+			* Pixels 9-12
	move16 (a0)+,(a1)+			* Pixels 13-16
	move16 (a0)+,(a1)+			* Pixels 17-20
	move16 (a0)+,(a1)+			* Pixels 21-24
	move16 (a0)+,(a1)+			* Pixels 25-28
	move16 (a0)+,(a1)+			* Pixels 29-32
	adda.l d1,a1
	dbra.l d0,.copy_tile_32x32x32_to_screen_loop
	rts

_Copy_Tile_32x32x24bit_to_Screen:
	mulu.w #3072,d0				* D0 = tile index * 3072 bytes (32*32*3)
	adda.l d0,a0
	sub.l #96,d1				* D1 = modulo - bytes copied per tile line (32*3)
	move.l #31,d0
.copy_tile_32x32x24_to_screen_loop:
	move16 (a0)+,(a1)+			* Pixels 1-5 (+1 byte)
	move16 (a0)+,(a1)+			* Pixels 6-10 (+2 bytes)
	move16 (a0)+,(a1)+			* Pixels 11-16
	move16 (a0)+,(a1)+			* Pixels 17-21 (+1 byte)
	move16 (a0)+,(a1)+			* Pixels 22-26 (+2 bytes)
	move16 (a0)+,(a1)+			* Pixels 27-32
	adda.l d1,a1
	dbra.l d0,.copy_tile_32x32x24_to_screen_loop
	rts

_Copy_Tile_32x32x16bit_to_Screen:
	mulu.w #2048,d0				* D0 = tile index * 2048 bytes (32*32*2)
	adda.l d0,a0
	sub.l #64,d1				* D1 = modulo - bytes copied per tile line (32*2)
	move.l #31,d0
.copy_tile_32x32x16_to_screen_loop:
	move16 (a0)+,(a1)+			* Pixels 1-8
	move16 (a0)+,(a1)+			* Pixels 9-16
	move16 (a0)+,(a1)+			* Pixels 17-24
	move16 (a0)+,(a1)+			* Pixels 25-32
	adda.l d1,a1
	dbra.l d0,.copy_tile_32x32x16_to_screen_loop
	rts

* ------------------------------ 64x64 ------------------------------

_Copy_Tile_64x64x32bit_to_Screen:
	mulu.w #16384,d0				* D0 = tile index * 16384 bytes (64*64*4)
	adda.l d0,a0
	sub.l #256,d1				* D1 = modulo - bytes copied per tile line (64*4)
	move.l #63,d0
.copy_tile_64x64x32_to_screen_loop:
	move16 (a0)+,(a1)+			* Pixels 1-4
	move16 (a0)+,(a1)+			* Pixels 5-8
	move16 (a0)+,(a1)+			* Pixels 9-12
	move16 (a0)+,(a1)+			* Pixels 13-16
	move16 (a0)+,(a1)+			* Pixels 17-20
	move16 (a0)+,(a1)+			* Pixels 21-24
	move16 (a0)+,(a1)+			* Pixels 25-28
	move16 (a0)+,(a1)+			* Pixels 29-32
	move16 (a0)+,(a1)+			* Pixels 33-36
	move16 (a0)+,(a1)+			* Pixels 37-40
	move16 (a0)+,(a1)+			* Pixels 41-44
	move16 (a0)+,(a1)+			* Pixels 45-48
	move16 (a0)+,(a1)+			* Pixels 49-52
	move16 (a0)+,(a1)+			* Pixels 53-56
	move16 (a0)+,(a1)+			* Pixels 57-60
	move16 (a0)+,(a1)+			* Pixels 61-64
	adda.l d1,a1
	dbra.l d0,.copy_tile_64x64x32_to_screen_loop
	rts

_Copy_Tile_64x64x24bit_to_Screen:
	mulu.w #12288,d0				* D0 = tile index * 12288 bytes (64*64*3)
	adda.l d0,a0
	sub.l #192,d1				* D1 = modulo - bytes copied per tile line (64*3)
	move.l #63,d0
.copy_tile_64x64x24_to_screen_loop:
	move16 (a0)+,(a1)+			* Pixels 1-6 (+1 byte)
	move16 (a0)+,(a1)+			* Pixels 6-11 (+2 bytes)
	move16 (a0)+,(a1)+			* Pixels 11-16
	move16 (a0)+,(a1)+			* Pixels 17-22 (+1 byte)
	move16 (a0)+,(a1)+			* Pixels 22-27 (+2 bytes)
	move16 (a0)+,(a1)+			* Pixels 27-32
	move16 (a0)+,(a1)+			* Pixels 33-38 (+1 byte)
	move16 (a0)+,(a1)+			* Pixels 38-43 (+2 bytes)
	move16 (a0)+,(a1)+			* Pixels 43-48
	move16 (a0)+,(a1)+			* Pixels 49-54 (+1 byte)
	move16 (a0)+,(a1)+			* Pixels 54-59 (+2 bytes)
	move16 (a0)+,(a1)+			* Pixels 59-64
	adda.l d1,a1
	dbra.l d0,.copy_tile_64x64x24_to_screen_loop
	rts

_Copy_Tile_64x64x16bit_to_Screen:
	mulu.w #8192,d0				* D0 = tile index * 8192 bytes (64*64*2)
	adda.l d0,a0
	sub.l #128,d1				* D1 = modulo - bytes copied per tile line (64*2)
	move.l #63,d0
.copy_tile_64x64x16_to_screen_loop:
	move16 (a0)+,(a1)+			* Pixels 1-8
	move16 (a0)+,(a1)+			* Pixels 9-16
	move16 (a0)+,(a1)+			* Pixels 17-24
	move16 (a0)+,(a1)+			* Pixels 25-32
	move16 (a0)+,(a1)+			* Pixels 33-40
	move16 (a0)+,(a1)+			* Pixels 41-48
	move16 (a0)+,(a1)+			* Pixels 49-56
	move16 (a0)+,(a1)+			* Pixels 57-64
	adda.l d1,a1
	dbra.l d0,.copy_tile_64x64x16_to_screen_loop
	rts

* ----------------------------- 128x128 -----------------------------

_Copy_Tile_128x128x32bit_to_Screen:
	and.l #$0000ffff,d0			* keep tile index word
	lsl.l #8,d0
	lsl.l #8,d0				* D0 = tile index * 65536 bytes (128*128*4)
	adda.l d0,a0
	sub.l #512,d1				* D1 = modulo - bytes copied per tile line (128*4)
	move.l #127,d0
.copy_tile_128x128x32_to_screen_loop:
	move16 (a0)+,(a1)+			* Pixels 1-4
	move16 (a0)+,(a1)+			* Pixels 5-8
	move16 (a0)+,(a1)+			* Pixels 9-12
	move16 (a0)+,(a1)+			* Pixels 13-16
	move16 (a0)+,(a1)+			* Pixels 17-20
	move16 (a0)+,(a1)+			* Pixels 21-24
	move16 (a0)+,(a1)+			* Pixels 25-28
	move16 (a0)+,(a1)+			* Pixels 29-32
	move16 (a0)+,(a1)+			* Pixels 33-36
	move16 (a0)+,(a1)+			* Pixels 37-40
	move16 (a0)+,(a1)+			* Pixels 41-44
	move16 (a0)+,(a1)+			* Pixels 45-48
	move16 (a0)+,(a1)+			* Pixels 49-52
	move16 (a0)+,(a1)+			* Pixels 53-56
	move16 (a0)+,(a1)+			* Pixels 57-60
	move16 (a0)+,(a1)+			* Pixels 61-64
	move16 (a0)+,(a1)+			* Pixels 65-68
	move16 (a0)+,(a1)+			* Pixels 69-72
	move16 (a0)+,(a1)+			* Pixels 73-76
	move16 (a0)+,(a1)+			* Pixels 77-80
	move16 (a0)+,(a1)+			* Pixels 81-84
	move16 (a0)+,(a1)+			* Pixels 85-88
	move16 (a0)+,(a1)+			* Pixels 89-92
	move16 (a0)+,(a1)+			* Pixels 93-96
	move16 (a0)+,(a1)+			* Pixels 97-100
	move16 (a0)+,(a1)+			* Pixels 101-104
	move16 (a0)+,(a1)+			* Pixels 105-108
	move16 (a0)+,(a1)+			* Pixels 109-112
	move16 (a0)+,(a1)+			* Pixels 113-116
	move16 (a0)+,(a1)+			* Pixels 117-120
	move16 (a0)+,(a1)+			* Pixels 121-124
	move16 (a0)+,(a1)+			* Pixels 125-128
	adda.l d1,a1
	dbra.l d0,.copy_tile_128x128x32_to_screen_loop
	rts

_Copy_Tile_128x128x24bit_to_Screen:
	mulu.w #49152,d0				* D0 = tile index * 49152 bytes (128*128*3)
	adda.l d0,a0
	sub.l #384,d1				* D1 = modulo - bytes copied per tile line (128*3)
	move.l #127,d0
.copy_tile_128x128x24_to_screen_loop:
	move16 (a0)+,(a1)+			* Pixels 1-6 (+1 byte)
	move16 (a0)+,(a1)+			* Pixels 6-11 (+2 bytes)
	move16 (a0)+,(a1)+			* Pixels 11-16
	move16 (a0)+,(a1)+			* Pixels 17-22 (+1 byte)
	move16 (a0)+,(a1)+			* Pixels 22-27 (+2 bytes)
	move16 (a0)+,(a1)+			* Pixels 27-32
	move16 (a0)+,(a1)+			* Pixels 33-38 (+1 byte)
	move16 (a0)+,(a1)+			* Pixels 38-43 (+2 bytes)
	move16 (a0)+,(a1)+			* Pixels 43-48
	move16 (a0)+,(a1)+			* Pixels 49-54 (+1 byte)
	move16 (a0)+,(a1)+			* Pixels 54-59 (+2 bytes)
	move16 (a0)+,(a1)+			* Pixels 59-64
	move16 (a0)+,(a1)+			* Pixels 65-70 (+1 byte)
	move16 (a0)+,(a1)+			* Pixels 70-75 (+2 bytes)
	move16 (a0)+,(a1)+			* Pixels 75-80
	move16 (a0)+,(a1)+			* Pixels 81-86 (+1 byte)
	move16 (a0)+,(a1)+			* Pixels 86-91 (+2 bytes)
	move16 (a0)+,(a1)+			* Pixels 91-96
	move16 (a0)+,(a1)+			* Pixels 97-102 (+1 byte)
	move16 (a0)+,(a1)+			* Pixels 102-107 (+2 bytes)
	move16 (a0)+,(a1)+			* Pixels 107-112
	move16 (a0)+,(a1)+			* Pixels 113-118 (+1 byte)
	move16 (a0)+,(a1)+			* Pixels 118-123 (+2 bytes)
	move16 (a0)+,(a1)+			* Pixels 123-128
	adda.l d1,a1
	dbra.l d0,.copy_tile_128x128x24_to_screen_loop
	rts

_Copy_Tile_128x128x16bit_to_Screen:
	mulu.w #32768,d0				* D0 = tile index * 32768 bytes (128*128*2)
	adda.l d0,a0
	sub.l #256,d1				* D1 = modulo - bytes copied per tile line (128*2)
	move.l #127,d0
.copy_tile_128x128x16_to_screen_loop:
	move16 (a0)+,(a1)+			* Pixels 1-8
	move16 (a0)+,(a1)+			* Pixels 9-16
	move16 (a0)+,(a1)+			* Pixels 17-24
	move16 (a0)+,(a1)+			* Pixels 25-32
	move16 (a0)+,(a1)+			* Pixels 33-40
	move16 (a0)+,(a1)+			* Pixels 41-48
	move16 (a0)+,(a1)+			* Pixels 49-56
	move16 (a0)+,(a1)+			* Pixels 57-64
	move16 (a0)+,(a1)+			* Pixels 65-72
	move16 (a0)+,(a1)+			* Pixels 73-80
	move16 (a0)+,(a1)+			* Pixels 81-88
	move16 (a0)+,(a1)+			* Pixels 89-96
	move16 (a0)+,(a1)+			* Pixels 97-104
	move16 (a0)+,(a1)+			* Pixels 105-112
	move16 (a0)+,(a1)+			* Pixels 113-120
	move16 (a0)+,(a1)+			* Pixels 121-128
	adda.l d1,a1
	dbra.l d0,.copy_tile_128x128x16_to_screen_loop
	rts

* ----------------------------- 256x256 -----------------------------

_Copy_Tile_256x256x32bit_to_Screen:
	and.l #$0000ffff,d0			* keep tile index word
	lsl.l #8,d0
	lsl.l #8,d0
	lsl.l #2,d0				* D0 = tile index * 262144 bytes (256*256*4)
	adda.l d0,a0
	sub.l #1024,d1				* D1 = modulo - bytes copied per tile line (256*4)
	move.l #255,d0
.copy_tile_256x256x32_to_screen_loop:
	move16 (a0)+,(a1)+			* Pixels 1-4
	move16 (a0)+,(a1)+			* Pixels 5-8
	move16 (a0)+,(a1)+			* Pixels 9-12
	move16 (a0)+,(a1)+			* Pixels 13-16
	move16 (a0)+,(a1)+			* Pixels 17-20
	move16 (a0)+,(a1)+			* Pixels 21-24
	move16 (a0)+,(a1)+			* Pixels 25-28
	move16 (a0)+,(a1)+			* Pixels 29-32
	move16 (a0)+,(a1)+			* Pixels 33-36
	move16 (a0)+,(a1)+			* Pixels 37-40
	move16 (a0)+,(a1)+			* Pixels 41-44
	move16 (a0)+,(a1)+			* Pixels 45-48
	move16 (a0)+,(a1)+			* Pixels 49-52
	move16 (a0)+,(a1)+			* Pixels 53-56
	move16 (a0)+,(a1)+			* Pixels 57-60
	move16 (a0)+,(a1)+			* Pixels 61-64
	move16 (a0)+,(a1)+			* Pixels 65-68
	move16 (a0)+,(a1)+			* Pixels 69-72
	move16 (a0)+,(a1)+			* Pixels 73-76
	move16 (a0)+,(a1)+			* Pixels 77-80
	move16 (a0)+,(a1)+			* Pixels 81-84
	move16 (a0)+,(a1)+			* Pixels 85-88
	move16 (a0)+,(a1)+			* Pixels 89-92
	move16 (a0)+,(a1)+			* Pixels 93-96
	move16 (a0)+,(a1)+			* Pixels 97-100
	move16 (a0)+,(a1)+			* Pixels 101-104
	move16 (a0)+,(a1)+			* Pixels 105-108
	move16 (a0)+,(a1)+			* Pixels 109-112
	move16 (a0)+,(a1)+			* Pixels 113-116
	move16 (a0)+,(a1)+			* Pixels 117-120
	move16 (a0)+,(a1)+			* Pixels 121-124
	move16 (a0)+,(a1)+			* Pixels 125-128
	move16 (a0)+,(a1)+			* Pixels 129-132
	move16 (a0)+,(a1)+			* Pixels 133-136
	move16 (a0)+,(a1)+			* Pixels 137-140
	move16 (a0)+,(a1)+			* Pixels 141-144
	move16 (a0)+,(a1)+			* Pixels 145-148
	move16 (a0)+,(a1)+			* Pixels 149-152
	move16 (a0)+,(a1)+			* Pixels 153-156
	move16 (a0)+,(a1)+			* Pixels 157-160
	move16 (a0)+,(a1)+			* Pixels 161-164
	move16 (a0)+,(a1)+			* Pixels 165-168
	move16 (a0)+,(a1)+			* Pixels 169-172
	move16 (a0)+,(a1)+			* Pixels 173-176
	move16 (a0)+,(a1)+			* Pixels 177-180
	move16 (a0)+,(a1)+			* Pixels 181-184
	move16 (a0)+,(a1)+			* Pixels 185-188
	move16 (a0)+,(a1)+			* Pixels 189-192
	move16 (a0)+,(a1)+			* Pixels 193-196
	move16 (a0)+,(a1)+			* Pixels 197-200
	move16 (a0)+,(a1)+			* Pixels 201-204
	move16 (a0)+,(a1)+			* Pixels 205-208
	move16 (a0)+,(a1)+			* Pixels 209-212
	move16 (a0)+,(a1)+			* Pixels 213-216
	move16 (a0)+,(a1)+			* Pixels 217-220
	move16 (a0)+,(a1)+			* Pixels 221-224
	move16 (a0)+,(a1)+			* Pixels 225-228
	move16 (a0)+,(a1)+			* Pixels 229-232
	move16 (a0)+,(a1)+			* Pixels 233-236
	move16 (a0)+,(a1)+			* Pixels 237-240
	move16 (a0)+,(a1)+			* Pixels 241-244
	move16 (a0)+,(a1)+			* Pixels 245-248
	move16 (a0)+,(a1)+			* Pixels 249-252
	move16 (a0)+,(a1)+			* Pixels 253-256
	adda.l d1,a1
	dbra.l d0,.copy_tile_256x256x32_to_screen_loop
	rts

_Copy_Tile_256x256x24bit_to_Screen:
	mulu.w #3,d0				* D0 = tile index * 3
	lsl.l #8,d0
	lsl.l #8,d0				* D0 = tile index * 196608 bytes (256*256*3)
	adda.l d0,a0
	sub.l #768,d1				* D1 = modulo - bytes copied per tile line (256*3)
	move.l #255,d0
.copy_tile_256x256x24_to_screen_loop:
	move16 (a0)+,(a1)+			* Pixels 1-6 (+1 byte)
	move16 (a0)+,(a1)+			* Pixels 6-11 (+2 bytes)
	move16 (a0)+,(a1)+			* Pixels 11-16
	move16 (a0)+,(a1)+			* Pixels 17-22 (+1 byte)
	move16 (a0)+,(a1)+			* Pixels 22-27 (+2 bytes)
	move16 (a0)+,(a1)+			* Pixels 27-32
	move16 (a0)+,(a1)+			* Pixels 33-38 (+1 byte)
	move16 (a0)+,(a1)+			* Pixels 38-43 (+2 bytes)
	move16 (a0)+,(a1)+			* Pixels 43-48
	move16 (a0)+,(a1)+			* Pixels 49-54 (+1 byte)
	move16 (a0)+,(a1)+			* Pixels 54-59 (+2 bytes)
	move16 (a0)+,(a1)+			* Pixels 59-64
	move16 (a0)+,(a1)+			* Pixels 65-70 (+1 byte)
	move16 (a0)+,(a1)+			* Pixels 70-75 (+2 bytes)
	move16 (a0)+,(a1)+			* Pixels 75-80
	move16 (a0)+,(a1)+			* Pixels 81-86 (+1 byte)
	move16 (a0)+,(a1)+			* Pixels 86-91 (+2 bytes)
	move16 (a0)+,(a1)+			* Pixels 91-96
	move16 (a0)+,(a1)+			* Pixels 97-102 (+1 byte)
	move16 (a0)+,(a1)+			* Pixels 102-107 (+2 bytes)
	move16 (a0)+,(a1)+			* Pixels 107-112
	move16 (a0)+,(a1)+			* Pixels 113-118 (+1 byte)
	move16 (a0)+,(a1)+			* Pixels 118-123 (+2 bytes)
	move16 (a0)+,(a1)+			* Pixels 123-128
	move16 (a0)+,(a1)+			* Pixels 129-134 (+1 byte)
	move16 (a0)+,(a1)+			* Pixels 134-139 (+2 bytes)
	move16 (a0)+,(a1)+			* Pixels 139-144
	move16 (a0)+,(a1)+			* Pixels 145-150 (+1 byte)
	move16 (a0)+,(a1)+			* Pixels 150-155 (+2 bytes)
	move16 (a0)+,(a1)+			* Pixels 155-160
	move16 (a0)+,(a1)+			* Pixels 161-166 (+1 byte)
	move16 (a0)+,(a1)+			* Pixels 166-171 (+2 bytes)
	move16 (a0)+,(a1)+			* Pixels 171-176
	move16 (a0)+,(a1)+			* Pixels 177-182 (+1 byte)
	move16 (a0)+,(a1)+			* Pixels 182-187 (+2 bytes)
	move16 (a0)+,(a1)+			* Pixels 187-192
	move16 (a0)+,(a1)+			* Pixels 193-198 (+1 byte)
	move16 (a0)+,(a1)+			* Pixels 198-203 (+2 bytes)
	move16 (a0)+,(a1)+			* Pixels 203-208
	move16 (a0)+,(a1)+			* Pixels 209-214 (+1 byte)
	move16 (a0)+,(a1)+			* Pixels 214-219 (+2 bytes)
	move16 (a0)+,(a1)+			* Pixels 219-224
	move16 (a0)+,(a1)+			* Pixels 225-230 (+1 byte)
	move16 (a0)+,(a1)+			* Pixels 230-235 (+2 bytes)
	move16 (a0)+,(a1)+			* Pixels 235-240
	move16 (a0)+,(a1)+			* Pixels 241-246 (+1 byte)
	move16 (a0)+,(a1)+			* Pixels 246-251 (+2 bytes)
	move16 (a0)+,(a1)+			* Pixels 251-256
	adda.l d1,a1
	dbra.l d0,.copy_tile_256x256x24_to_screen_loop
	rts

_Copy_Tile_256x256x16bit_to_Screen:
	and.l #$0000ffff,d0			* keep tile index word
	lsl.l #8,d0
	lsl.l #8,d0
	lsl.l #1,d0				* D0 = tile index * 131072 bytes (256*256*2)
	adda.l d0,a0
	sub.l #512,d1				* D1 = modulo - bytes copied per tile line (256*2)
	move.l #255,d0
.copy_tile_256x256x16_to_screen_loop:
	move16 (a0)+,(a1)+			* Pixels 1-8
	move16 (a0)+,(a1)+			* Pixels 9-16
	move16 (a0)+,(a1)+			* Pixels 17-24
	move16 (a0)+,(a1)+			* Pixels 25-32
	move16 (a0)+,(a1)+			* Pixels 33-40
	move16 (a0)+,(a1)+			* Pixels 41-48
	move16 (a0)+,(a1)+			* Pixels 49-56
	move16 (a0)+,(a1)+			* Pixels 57-64
	move16 (a0)+,(a1)+			* Pixels 65-72
	move16 (a0)+,(a1)+			* Pixels 73-80
	move16 (a0)+,(a1)+			* Pixels 81-88
	move16 (a0)+,(a1)+			* Pixels 89-96
	move16 (a0)+,(a1)+			* Pixels 97-104
	move16 (a0)+,(a1)+			* Pixels 105-112
	move16 (a0)+,(a1)+			* Pixels 113-120
	move16 (a0)+,(a1)+			* Pixels 121-128
	move16 (a0)+,(a1)+			* Pixels 129-136
	move16 (a0)+,(a1)+			* Pixels 137-144
	move16 (a0)+,(a1)+			* Pixels 145-152
	move16 (a0)+,(a1)+			* Pixels 153-160
	move16 (a0)+,(a1)+			* Pixels 161-168
	move16 (a0)+,(a1)+			* Pixels 169-176
	move16 (a0)+,(a1)+			* Pixels 177-184
	move16 (a0)+,(a1)+			* Pixels 185-192
	move16 (a0)+,(a1)+			* Pixels 193-200
	move16 (a0)+,(a1)+			* Pixels 201-208
	move16 (a0)+,(a1)+			* Pixels 209-216
	move16 (a0)+,(a1)+			* Pixels 217-224
	move16 (a0)+,(a1)+			* Pixels 225-232
	move16 (a0)+,(a1)+			* Pixels 233-240
	move16 (a0)+,(a1)+			* Pixels 241-248
	move16 (a0)+,(a1)+			* Pixels 249-256
	adda.l d1,a1
	dbra.l d0,.copy_tile_256x256x16_to_screen_loop
	rts

	end
