/* ******************************************** */
/*                                              */
/*                AIMP Plugins API              */
/*             v3.00.943 (26.10.2011)           */
/*                Visual Plugins                */
/*                                              */
/*              (c) Artem Izmaylov              */
/*                 www.aimp.ru                  */
/*             Mail: artem@aimp.ru              */
/*              ICQ: 345-908-513                */
/*                                              */
/* ******************************************** */

#ifndef AIMPSDKVisualH
#define AIMPSDKVisualH

#include <windows.h>
#include <unknwn.h>
#include "AIMPSDKCore.h"

// flags for IAIMPVisualPlugin3.GetPluginFlags
const int  AIMP_VISUAL_FLAGS_RQD_DATA_WAVE       = 1;
const int  AIMP_VISUAL_FLAGS_RQD_DATA_SPECTRUM   = 2;
const int  AIMP_VISUAL_FLAGS_NOT_SUSPEND	     = 4;

typedef   signed short TWaveForm[2][512];
typedef unsigned short TSpectrum[2][256];

struct TAIMPVisualData
{
	int LevelR;
	int LevelL;
	TSpectrum Spectrum;
	TWaveForm WaveForm;
};

typedef TAIMPVisualData *PAIMPVisualData;

/* IAIMPVisualPlugin3 */

class IAIMPVisualPlugin3: public IUnknown
{
	public:
		virtual PWCHAR WINAPI GetPluginAuthor() = 0;
		virtual PWCHAR WINAPI GetPluginInfo() = 0;
		virtual PWCHAR WINAPI GetPluginName() = 0;
		virtual DWORD  WINAPI GetPluginFlags() = 0; // See AIMP_VISUAL_FLAGS_XXX
		virtual HRESULT WINAPI Initialize(IAIMPCoreUnit *ACoreUnit) = 0;
		virtual HRESULT WINAPI Deinitialize() = 0;
		virtual void WINAPI DisplayClick(int X, int Y) = 0;
		virtual void WINAPI DisplayRender(HDC DC, PAIMPVisualData AData) = 0;
		virtual void WINAPI DisplayResize(int AWidth, int AHeight) = 0;
};

// Export function name: AIMP_QueryVisual3
typedef BOOL (WINAPI *TAIMPVisualPluginProc)(IAIMPVisualPlugin3 **AHeader);

#endif