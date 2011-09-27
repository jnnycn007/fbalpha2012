// Video Output - (calls all the Vid Out plugins)
#include "burner.h"
#include "highcol.h"

#undef ENABLE_PREVIEW

static InterfaceInfo VidInfo = { NULL, };

unsigned int nVidSelect = 2;					// Which video output is selected
static unsigned int nVidActive = 0;				// Which video output is actived

bool bVidOkay = false;


int nVidDepth = 16;
int nVidRefresh = 0;
int nVidFullscreen = 0;
int bVidFullStretch = 0;						// 1 = stretch to fill the entire window/screen
int bVidCorrectAspect = 1;						// 1 = stretch to fill the window/screen while maintaining the correct aspect ratio
int bVidVSync = 0;								// 1 = sync blits/pageflips/presents to the screen
int bVidTripleBuffer = 0;						// 1 = use triple buffering
int nVidRotationAdjust = 0;						// & 1: do not rotate the graphics for vertical games,  & 2: Reverse flipping for vertical games
int bVidForce16bit = 1;							// Emulate the game in 16-bit even when the screen is 32-bit (D3D blitter)
unsigned int vidFilterLinear = 0;				// 1 = linear filter, or point filter
unsigned int vidHardwareVertex = 0;				// 1 = use hardware vertex processing
unsigned int vidMotionBlur = 0;					// 1 = motion blur
unsigned int vidUseFilter = 0;
unsigned int vidForceFilterSize = 0;
int nVidOriginalScrnAspectX;
int nVidOriginalScrnAspectY;

int nVidDXTextureManager = 0;					// How to transfer the game image to video memory and/or a texture --
												//  0 = blit from system memory / use driver/DirectX texture management
												//  1 = copy to a video memory surface, then use bltfast()
unsigned int nVid3DProjection = 0;				// Options for the 3D projection effct
float fVidScreenAngle = 0.174533f;				// The angle at which to tilt the screen backwards (in radians, D3D blitter)
float fVidScreenCurvature = 0.698132f;			// The angle of the maximum screen curvature (in radians, D3D blitter)

int nVidScrnWidth = 0, nVidScrnHeight = 0;		// Actual Screen dimensions (0 if in windowed mode)
int nVidScrnDepth = 0;							// Actual screen depth

int nVidScrnAspectX = 4, nVidScrnAspectY = 3;	// Aspect ratio of the display screen
int nVidScrnAspectMode = ASPECT_RATIO_4_3;
float vidScrnAspect = (float)4 / (float)3;				// Aspect ratio
extern bool autoVidScrnAspect = true;			// Automatical Aspect ratio

unsigned char* pVidImage = NULL;				// Memory buffer
int nVidImageWidth = DEFAULT_IMAGE_WIDTH;		// Memory buffer size
int nVidImageHeight = DEFAULT_IMAGE_HEIGHT;		//
int nVidImageLeft = 0, nVidImageTop = 0;		// Memory buffer visible area offsets
int nVidImagePitch = 0, nVidImageBPP = 0;		// Memory buffer pitch and bytes per pixel
int nVidImageDepth = 0;							// Memory buffer bits per pixel

int effect_depth = 16;

unsigned int nVidAdapter = 0;

unsigned int (__cdecl *VidHighCol) (int r, int g, int b, int i);
static bool bVidRecalcPalette;

static unsigned char* pVidTransImage = NULL;
static unsigned int* pVidTransPalette = NULL;
const int transPaletteSize = 65536;

int nXOffset = 0;
int nYOffset = 0;
int nXScale = 0;
int nYScale = 0;

static unsigned int __cdecl HighCol15(int r, int g, int b, int  /* i */)
{
	unsigned int t;
	t  = (r << 7) & 0x7C00;
	t |= (g << 2) & 0x03E0;
	t |= (b >> 3) & 0x001F;
	return t;
}

	extern struct VidOut VidOutPSGL;

VidOut* VidDriver(unsigned int driver)
{
	if (driver == VID_PSGL)
		return &VidOutPSGL;
}

int VidSelect(unsigned int driver)
{
	nVidSelect = driver;
	return 0;
}

// Forward to VidOut functions
int VidInit()
{
	VidExit();

	int nRet = 1;

	nShowEffect = 0;

	if (bDrvOkay)
	{
		nVidActive = nVidSelect;						 
		if ((nRet = VidDriver(nVidActive)->Init()) == 0)
		{
			nBurnBpp = nVidImageBPP; // Set Burn library Bytes per pixel
			bVidOkay = true;

			if (bDrvOkay && (BurnDrvGetFlags() & BDF_16BIT_ONLY) && nVidImageBPP > 2)
			{
				nBurnBpp = 2;

				pVidTransPalette = (unsigned int*)malloc(transPaletteSize * sizeof(int));
				pVidTransImage = (unsigned char*)malloc(nVidImageWidth * nVidImageHeight * (nVidImageBPP >> 1) * sizeof(short));

				BurnHighCol = HighCol15;

				if (pVidTransPalette == NULL || pVidTransImage == NULL)
				{
					VidExit();
					nRet = 1;
				}
			}
		}
	}

	return nRet;
}

int VidExit()
{
	IntInfoFree(&VidInfo);

	if (!bVidOkay)
		return 1;

	int nRet = VidDriver(nVidActive)->Exit();

	bVidOkay = false;

	nVidImageWidth = DEFAULT_IMAGE_WIDTH;
	nVidImageHeight = DEFAULT_IMAGE_HEIGHT;

	nVidImageBPP = nVidImageDepth = 0;
	nBurnPitch = nBurnBpp = 0;

	free(pVidTransPalette);
	pVidTransPalette = NULL;
	free(pVidTransImage);
	pVidTransImage = NULL;

	return nRet;
}

void CalculateViewports()
{
   extern void CalculateViewport();
   CalculateViewport();
}

#define VidDoFrame(bRedraw) \
if (pVidTransImage) \
{ \
	unsigned short* pSrc = (unsigned short*)pVidTransImage; \
	unsigned char* pDest = pVidImage; \
	\
	if (bVidRecalcPalette) \
	{ \
		uint64_t r = 0; \
		do{ \
			uint64_t g = 0; \
			do{ \
				uint64_t b = 0; \
				do{ \
					uint64_t r_ = r | (r >> 5); \
					uint64_t g_ = g | (g >> 5); \
					uint64_t b_ = b | (b >> 5); \
					pVidTransPalette[(r << 7) | (g << 2) | (b >> 3)] = ARGB(r_,g_,b_); \
					b += 8; \
				}while(b < 256); \
				g += 8; \
			}while(g < 256); \
			r += 8; \
		}while(r < 256); \
		\
		bVidRecalcPalette = false; \
	} \
	\
	pBurnDraw = pVidTransImage; \
	nBurnPitch = nVidImageWidth << 1; \
	\
	extern void _psglRender();  \
	BurnDrvFrame(); \
	_psglRender(); \
	\
	/* set avi buffer, modified by regret */ \
	\
	pBurnDraw = NULL; \
	nBurnPitch = 0; \
	\
	int y = 0; \
	do{ \
		int x = 0; \
		do{ \
			((unsigned int*)pDest)[x] = pVidTransPalette[pSrc[x]]; \
			x++; \
		}while(x < nVidImageWidth); \
		y++; \
		pSrc += nVidImageWidth; \
		pDest += nVidImagePitch; \
	}while(y < nVidImageHeight); \
} \
   else \
   { \
		pBurnDraw = pVidImage; \
		nBurnPitch = nVidImagePitch; \
      \
		extern void _psglRender();  \
		BurnDrvFrame(); \
		_psglRender(); \
      \
		/* set avi buffer, modified by regret */ \
      \
		pBurnDraw = NULL; \
		nBurnPitch = 0; \
	} \
	return 0;

int VidFrame()
{
	VidDoFrame(0);
}

int VidRedraw()
{
	VidDoFrame(1);
}

int VidRecalcPal()
{
	bVidRecalcPalette = true;

	return BurnRecalcPal();
}

// reinit video, added by regret

int VidReinit()
{
	VidInit();

	if (bRunPause || !bDrvOkay)
		VidRedraw();

	CalculateViewports();
	return 0;
}

const TCHAR* VidDriverName(unsigned int driver)
{
	return FBALoadStringEx(1);
}

const TCHAR* VidGetName()
{
	return VidDriverName(nVidActive);
}

InterfaceInfo* VidGetInfo()
{
	if (IntInfoInit(&VidInfo))
	{
		IntInfoFree(&VidInfo);
		return NULL;
	}

	if (bVidOkay) {
		TCHAR szString[MAX_PATH] = _T("");
		RECT rect;

		VidInfo.pszModuleName = VidGetName();

		IntInfoAddStringInterface(&VidInfo, szString);

		_sntprintf(szString, sizearray(szString), _T("Source image %ix%i, %ibpp"), nVidImageWidth, nVidImageHeight, nVidImageDepth);
		IntInfoAddStringInterface(&VidInfo, szString);

		if (pVidTransImage) {
			_sntprintf(szString, sizearray(szString), _T("Using generic software 15->%ibpp wrapper"), nVidImageDepth);
			IntInfoAddStringInterface(&VidInfo, szString);
		}

		if (bVidVSync) {
			_sntprintf(szString, sizearray(szString), _T("VSync enabled"));
			IntInfoAddStringInterface(&VidInfo, szString);
		}

#ifndef SN_TARGET_PS3
		if (vidUseFilter) {
			_sntprintf(szString, sizearray(szString), _T("Using pixel filter: %s (%ix zoom)"), VidFilterGetEffect(nVidFilter), nPreScaleZoom);
			IntInfoAddStringInterface(&VidInfo, szString);
		}
#endif

		if (VidDriver(nVidActive)->GetSetting)
			VidDriver(nVidActive)->GetSetting(&VidInfo);
	}
	else
		IntInfoAddStringInterface(&VidInfo, _T("Video plugin not initialised"));

	return &VidInfo;
}
