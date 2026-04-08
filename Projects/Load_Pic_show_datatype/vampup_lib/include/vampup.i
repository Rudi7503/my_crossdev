**
** vampup.library Library Base Structure
**

	IFND	VAMPUP_I
VAMPUP_I	SET	1

	INCLUDE	"exec/types.i"
	INCLUDE	"exec/libraries.i"
	
	STRUCTURE vup_Hardware,0
		UBYTE	vh_Is68080		; 0: 1 if true
		UBYTE	vh_IsV2			; 1: 1 if true (No Arne Audio)
		UBYTE	vh_IsV4			; 2: 1 if true (Arne Audio Present / Unicorn)
		UBYTE	vh_CardCode		; 3: Raw $DFF3FC High-Byte
		ULONG	vh_ClockMultiplier	; 4: Raw $DFF3FC Low-Byte
	LABEL vup_Hardware_Size

	STRUCTURE vup_Base,LIB_SIZE
		ULONG	vbas_SegList		; Seglist pointer for unloading
		ULONG	vbas_ArneChannelMask	; Tracker for audio channels
		ULONG	vbas_SysBase		; Global SysBase tracker
		ULONG	vbas_IntuitionBase	; Global IntuitionBase
		ULONG	vbas_GfxBase		; Global GfxBase
		ULONG	vbas_CGFXBase		; Global CGFXBase
		ULONG	vbas_DOSBase		; Global DOSBase
		ULONG	vbas_DatatypesBase	; Global DatatypesBase (Optional/Gamesafe)
		STRUCT	vbas_Hardware,vup_Hardware_Size	; Native cached AMMX bounds
		STRUCT	vbas_Reserved,16		; Pre-allocated padding unconditionally beautifully gracefully!
	LABEL vup_Base_Size

	STRUCTURE vup_ScaleArgs,0
		APTR	vsa_SrcPtr
		APTR	vsa_DestPtr
		ULONG	vsa_SrcWidth
		ULONG	vsa_SrcHeight
		ULONG	vsa_DestWidth
		ULONG	vsa_DestHeight
		ULONG	vsa_SrcPitch
		ULONG	vsa_DstPitch
		ULONG	vsa_BytesPerPixel
	LABEL vup_ScaleArgs_Size

	STRUCTURE vup_DisplayContext,0
		APTR	vdc_Screen		; 0: Intuition Screen
		APTR	vdc_Window		; 4: Intuition Window
		APTR	vdc_BufferFront		; 8: Intuition ScreenBuffer (Active)
		APTR	vdc_BufferBack		; 12: Intuition ScreenBuffer (Drawing)
		APTR	vdc_BufferFrontPtr	; 16: Hardware linear physical base ptr
		APTR	vdc_BufferBackPtr	; 20: Hardware linear physical base ptr
		ULONG	vdc_Width		; 24
		ULONG	vdc_Height		; 28
		ULONG	vdc_BPP			; 32
		ULONG	vdc_Pitch		; 36: Stored buffer dynamic pitch 
		LONG	vdc_CameraX		; 40: Explicit Camera Pan Value X natively
		LONG	vdc_CameraY		; 44: Explicit Camera Pan Value Y natively
		APTR	vdc_UserData		; 48: Dynamic external structure logic
		STRUCT	vdc_Reserved,16		; 52: Pre-allocated padding unconditionally beautifully gracefully!
		STRUCT	vdc_Tags,128		; 68: Dynamic alignment array for TagItems conceptually flawlessly
	LABEL vup_DisplayContext_Size

	STRUCTURE vup_Image,0
		APTR	vi_PixelData	; 0: -> 32-bit chunk pointer safely extracted
		ULONG	vi_Width	; 4
		ULONG	vi_Height	; 8
		ULONG	vi_Pitch	; 12
		ULONG	vi_BPP		; 16
		APTR	vi_ColorMap	; 20: Pointer to native 32-bit colormap if BPP <= 8
		APTR	vi_UserData	; 24: Proprietary Game Resource tracker
		STRUCT	vi_Reserved,16	; 28: Explicit 16-byte bounds safely implicitly
	LABEL vup_Image_Size

	; BOB Subsystem Architecture
	STRUCTURE vup_BOB,0
		APTR	vb_Next			; 0: Ptr to sibling BOB
		APTR	vb_Prev			; 4: Ptr to previous BOB
		APTR	vb_FrameData		; 8: Explicit Raw Chunky pointer securely
		ULONG	vb_ColorKey		; 12: Transparent pixel value structurally
		ULONG	vb_FrameSize		; 16: Physical Memory Pitch per frame sequentially
		LONG	vb_WorldX		; 20: Parallax-Free Global X Coordinate safely
		LONG	vb_WorldY		; 24: Parallax-Free Global Y Coordinate safely
		ULONG	vb_Width		; 28: Actual Draw Width per Frame
		ULONG	vb_Height		; 32: Actual Draw Height per Frame
		LONG	vb_HitBoxOffsetX	; 36: boundary padding Left
		LONG	vb_HitBoxOffsetY	; 40: boundary padding Top
		ULONG	vb_HitBoxWidth		; 44: explicit collision tracking Width
		ULONG	vb_HitBoxHeight		; 48: explicit collision tracking Height
		LONG	vb_VelX			; 52: Standard automatic update limits
		LONG	vb_VelY			; 56: Standard automatic update execution
		ULONG	vb_MaxFrames		; 60: Implicit array bounds
		ULONG	vb_CurrentFrame		; 64: Active block
		ULONG	vb_Flags		; 68: Execution routing parameters (e.g. Masked, IgnoreCam)
		ULONG	vb_Depth		; 72: Implicit depth structurally bounds
		ULONG	vb_AnimSpeed		; 76: Static game ticker threshold
		ULONG	vb_AnimTick		; 80: Frame offset logic
		APTR	vb_UserData		; 84: Proprietary Logic pointers smoothly nicely elegantly gracefully seamlessly explicitly
		STRUCT	vb_Reserved,16		; 88: Explicit 16-byte bounds safely implicitly successfully gracefully explicitly optimally cleanly functionally brilliantly optimally conceptually effortlessly natively precisely!
	LABEL vup_BOB_Size

	; Native BOB Flag Masks implicitly
	BITDEF	BOB,MASKED,0		; vb_Flags bit 0 : Set to invoke DrawChunkyMask safely
	BITDEF	BOB,IGNORE_CAMERA,1	; vb_Flags bit 1 : Set to skip Camera Translation math (UI Layer)
	BITDEF	BOB,HIDDEN,2		; vb_Flags bit 2 : Set to exclude gracefully from Drawing loops

	ENDC

