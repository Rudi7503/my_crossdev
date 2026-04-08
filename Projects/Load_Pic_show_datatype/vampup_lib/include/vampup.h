/* vampup.library C Header */
#ifndef VAMPUP_LIBRARY_H
#define VAMPUP_LIBRARY_H

#include <exec/types.h>
#include <exec/libraries.h>

struct vup_Hardware {
	UBYTE	vh_Is68080;
	UBYTE	vh_IsV2;
	UBYTE	vh_IsV4;
	UBYTE	vh_CardCode;
	ULONG	vh_ClockMultiplier;
};

struct vup_Base {
	struct Library libNode;
	ULONG   vbas_SegList;
	ULONG   vbas_ArneChannelMask;
	ULONG   vbas_SysBase;
	ULONG   vbas_IntuitionBase;
	ULONG   vbas_GfxBase;
	ULONG   vbas_CGFXBase;
	ULONG   vbas_DatatypesBase;
	struct vup_Hardware vbas_Hardware;
	ULONG   vbas_Reserved[4];
};

struct vup_ScaleArgs {
    APTR  vsa_SrcPtr;
    APTR  vsa_DestPtr;
    ULONG vsa_SrcWidth;
    ULONG vsa_SrcHeight;
    ULONG vsa_DestWidth;
    ULONG vsa_DestHeight;
    ULONG vsa_SrcPitch;
    ULONG vsa_DstPitch;
    ULONG vsa_BytesPerPixel;
};

struct vup_DisplayContext {
	APTR	vdc_Screen;
	APTR	vdc_Window;
	APTR	vdc_BufferFront;
	APTR	vdc_BufferBack;
	APTR	vdc_BufferFrontPtr;
	APTR	vdc_BufferBackPtr;
	ULONG	vdc_Width;
	ULONG	vdc_Height;
	ULONG	vdc_BPP;
	ULONG	vdc_Pitch;
	LONG	vdc_CameraX;
	LONG	vdc_CameraY;
	APTR	vdc_UserData;
	ULONG	vdc_Reserved[4];
	ULONG	vdc_Tags[32]; // 128 bytes of TagItems
};

struct vup_Image {
	APTR	vi_PixelData;
	ULONG	vi_Width;
	ULONG	vi_Height;
	ULONG	vi_Pitch;
	ULONG	vi_BPP;
	APTR	vi_ColorMap;
	APTR	vi_UserData;
	ULONG	vi_Reserved[4];
};

struct vup_BOB {
	APTR	vb_Next;
	APTR	vb_Prev;
	APTR	vb_FrameData;
	ULONG	vb_ColorKey;
	ULONG	vb_FrameSize;
	LONG	vb_WorldX;
	LONG	vb_WorldY;
	ULONG	vb_Width;
	ULONG	vb_Height;
	LONG	vb_HitBoxOffsetX;
	LONG	vb_HitBoxOffsetY;
	ULONG	vb_HitBoxWidth;
	ULONG	vb_HitBoxHeight;
	LONG	vb_VelX;
	LONG	vb_VelY;
	ULONG	vb_MaxFrames;
	ULONG	vb_CurrentFrame;
	ULONG	vb_Flags;
	ULONG	vb_Depth;
	ULONG	vb_AnimSpeed;
	ULONG	vb_AnimTick;
	APTR	vb_UserData;
	ULONG	vb_Reserved[4];
};

#endif
