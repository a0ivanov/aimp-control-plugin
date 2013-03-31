/* ******************************************** */
/*                                              */
/*                AIMP Plugins API              */
/*             v3.00.960 (01.12.2011)           */
/*                 Input Plugins                */
/*                                              */
/*              (c) Artem Izmaylov              */
/*                 www.aimp.ru                  */
/*             Mail: artem@aimp.ru              */
/*              ICQ: 345-908-513                */
/*                                              */
/* ******************************************** */

#ifndef AIMPSDKInputH
#define AIMPSDKInputH

#include <windows.h>
#include <unknwn.h>
#include "AIMPSDKCommon.h"

const int AIMP_INPUT_BITDEPTH_08BIT      = 1;
const int AIMP_INPUT_BITDEPTH_16BIT      = 2;
const int AIMP_INPUT_BITDEPTH_24BIT      = 3;
const int AIMP_INPUT_BITDEPTH_32BIT      = 4;
const int AIMP_INPUT_BITDEPTH_32BITFLOAT = 5;
const int AIMP_INPUT_BITDEPTH_64BITFLOAT = 6;

const int AIMP_INPUT_FLAG_FILE           = 1; // IAIMPInputHeader.CreateDecoder is supported by plugin
const int AIMP_INPUT_FLAG_ISTREAM        = 2; // IAIMPInputHeader.CreateDecoderEx is supported by plugin

static const GUID IID_IAIMPInputStream = {0x41494D50, 0x0033, 0x494E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10};
static const GUID IID_IAIMPInputPluginDecoder = {0x41494D50, 0x0033, 0x494E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20};
 
/* IAIMPInputStream */

class IAIMPInputStream: public IUnknown
{
	public:
		virtual INT64 WINAPI GetSize() = 0;
		virtual INT64 WINAPI GetPosition() = 0;
		virtual INT64 WINAPI SetPosition(const INT64 AValue) = 0;
		virtual int   WINAPI ReadData(unsigned char *Buffer, int Size) = 0;
		virtual INT64 WINAPI Skip(const INT64 ABytes) = 0;
};

/* IAIMPInputPluginDecoder */

class IAIMPInputPluginDecoder: public IUnknown
{
	public:
		// Read Info about stream, ABitDepth: See AIMP_INPUT_BITDEPTH_XXX flags
		virtual BOOL WINAPI DecoderGetInfo(int *ASampleRate, int *AChannels, int *ABitDepth) = 0;
		// Uncompressed stream position in Bytes
		virtual INT64 WINAPI DecoderGetPosition() = 0;
		// Uncompressed stream size in Bytes
		virtual INT64 WINAPI DecoderGetSize() = 0; 
		// Read Info about the file
		virtual BOOL WINAPI DecoderGetTags(TAIMPFileInfo *AFileInfo) = 0;
		// Size of Buffer in Bytes
		virtual int  WINAPI DecoderRead(unsigned char *Buffer, int Size) = 0;
		// Uncompressed stream position in Bytes
		virtual BOOL WINAPI DecoderSetPosition(const INT64 AValue) = 0;
		// Is DecoderSetPosition supports?
		virtual BOOL WINAPI DecoderIsSeekable() = 0;
		// Is speed, tempo and etc supports?
		// RealTime streams doesn't supports speed control
		virtual BOOL WINAPI DecoderIsRealTimeStream() = 0;
		// Return format type for current file, (MP3, OGG, AAC+, FLAC and etc)
		virtual PWCHAR WINAPI DecoderGetFormatType() = 0;
};

/* IAIMPInputPluginHeader */

class IAIMPInputPluginHeader: public IUnknown
{
	public:
		virtual PWCHAR WINAPI GetPluginAuthor() = 0;
		virtual PWCHAR WINAPI GetPluginInfo() = 0;
		virtual PWCHAR WINAPI GetPluginName() = 0;
		// Combination of the flags, See AIMP_INPUT_FLAG_XXX
		virtual DWORD  WINAPI GetPluginFlags() = 0; 
		// Initialize / Finalize Plugin
		virtual BOOL WINAPI Initialize() = 0;
		virtual BOOL WINAPI Finalize() = 0;
		// Create decoder for the file / Stream
		virtual BOOL WINAPI CreateDecoder(PWCHAR AFileName, IAIMPInputPluginDecoder **ADecoder) = 0;
		virtual BOOL WINAPI CreateDecoderEx(IAIMPInputStream *AStream, IAIMPInputPluginDecoder **ADecoder) = 0;
		// Get FileInfo
		virtual BOOL WINAPI GetFileInfo(PWCHAR AFileName, TAIMPFileInfo *AFileInfo) = 0;
		// Return string format: "My Custom Format1|*.fmt1;*.fmt2;|"
		virtual PWCHAR WINAPI GetSupportsFormats() = 0;
};

// Export function name: AIMP_QueryInput
typedef BOOL (WINAPI *TAIMPInputPluginHeaderProc)(IAIMPInputPluginHeader **AHeader);

#endif