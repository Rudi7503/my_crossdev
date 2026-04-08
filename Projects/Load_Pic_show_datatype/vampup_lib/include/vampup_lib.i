**
** vampup.library Library Vector Offsets
**

; -----------------------------------------------------------
; SECTION 1: Graphics Engine (25 Vector Slots. Max: -174)
; -----------------------------------------------------------
_LVOvup_OpenDisplay	EQU	-30
_LVOvup_FlipDisplay	EQU	-36
_LVOvup_CloseDisplay	EQU	-42
_LVOvup_DrawChunky	EQU	-48
_LVOvup_DrawChunkyMask	EQU	-54
_LVOvup_FillChunkyRect	EQU	-60
_LVOvup_FastScroll	EQU	-66
_LVOvup_GetChunkyPos	EQU	-72
_LVOvup_Convert32To16	EQU	-78
_LVOvup_ScaleImage	EQU	-84
_LVOvup_ScaleImageA	EQU	-90
; -- 14 Reserved Padding Spots --

; -----------------------------------------------------------
; SECTION 2: BOB Engine (25 Vector Slots. Max: -324)
; -----------------------------------------------------------
_LVOvup_CreateBOB	EQU	-180
_LVOvup_FreeBOB	EQU	-186
_LVOvup_FreeBOBList	EQU	-192
_LVOvup_AddBOB	EQU	-198
_LVOvup_RemoveBOB	EQU	-204
_LVOvup_UpdateBOB	EQU	-210
_LVOvup_UpdateBOBList	EQU	-216
_LVOvup_CheckBOBCollision	EQU	-222
_LVOvup_CheckBOBListCollision	EQU	-228
_LVOvup_DrawBOB	EQU	-234
_LVOvup_DrawBOBList	EQU	-240
_LVOvup_DrawMap	EQU	-246
; -- 13 Reserved Padding Spots --

; -----------------------------------------------------------
; SECTION 3: Utility & Memory (25 Vector Slots. Max: -474)
; -----------------------------------------------------------
_LVOvup_AllocMem32	EQU	-330
_LVOvup_FreeMem32	EQU	-336
_LVOvup_FastMemCopy	EQU	-342
_LVOvup_FastMemClear	EQU	-348
_LVOvup_AlphaBlend	EQU	-354
_LVOvup_ChunkyToPlanar	EQU	-360
_LVOvup_PlanarToChunky	EQU	-366
_LVOvup_MatrixMul3D	EQU	-372
_LVOvup_GetHardwareInfo	EQU	-378
_LVOvup_LoadImage	EQU	-384
_LVOvup_FreeImage	EQU	-390
_LVOvup_LoadDataFile	EQU	-396
_LVOvup_OpenEasyWindow	EQU	-402
; -- 12 Reserved Padding Spots --

; -----------------------------------------------------------
; SECTION 4: Audio Engine (25 Vector Slots. Max: -624)
; -----------------------------------------------------------
_LVOvup_InitArneTracker	EQU	-480
_LVOvup_ArneAllocChannel	EQU	-486
_LVOvup_ArneFreeChannel	EQU	-492
_LVOvup_ArnePlaySample	EQU	-498
_LVOvup_ArneSetVolumePan	EQU	-504
_LVOvup_AudioResample	EQU	-510
; -- 19 Reserved Padding Spots --
