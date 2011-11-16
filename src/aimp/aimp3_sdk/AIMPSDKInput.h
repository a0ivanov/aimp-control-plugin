/* ******************************************** */
/*                                              */
/*                AIMP Plugins API              */
/*             v3.00.943 (26.10.2011)           */
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

const int AIMP_INPUT_FLAG_FILE           = 1; // IAIMPInputHeader.CreateDecoder supports
const int AIMP_INPUT_FLAG_ISTREAM        = 2; // IAIMPInputHeader.CreateDecoderEx supports

DEFINE_GUID(IID_IAIMPInputStream,		 0x41494D50, 0x0033, 0x494E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10);
DEFINE_GUID(IID_IAIMPInputPluginDecoder, 0x41494D50, 0x0033, 0x494E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20);
 
/* IAIMPInputStream */

class IAIMPInputStream: public IUnknown
{
	public:
		virtual INT64 WINAPI GetSize();
		virtual INT64 WINAPI GetPosition();
		virtual INT64 WINAPI SetPosition(const INT64 AValue);
		virtual int   WINAPI ReadData(unsigned char *Buffer, int Size);
		virtual INT64 WINAPI Skip(const INT64 ABytes);
};

/* IAIMPInputPluginDecoder */

class IAIMPInputPluginDecoder: public IUnknown
{
	public:
		// Read Info about stream, ABitDepth: See AIMP_INPUT_BITDEPTH_XXX flags
		virtual BOOL WINAPI DecoderGetInfo(int *ASampleRate, int *AChannels, int *ABitDepth);
		// Uncompressed stream position in Bytes
		virtual INT64 WINAPI DecoderGetPosition();
		// Uncompressed stream size in Bytes
		virtual INT64 WINAPI DecoderGetSize(); 
		// Read Info about the file
		virtual BOOL WINAPI DecoderGetTags(TAIMPFileInfo *AFileInfo);
		// Size of Buffer in Bytes
		virtual int  WINAPI DecoderRead(unsigned char *Buffer, int Size);
		// Uncompressed stream position in Bytes
		virtual BOOL WINAPI DecoderSetPosition(const INT64 AValue);
		// Is DecoderSetPosition supports?
		virtual BOOL WINAPI DecoderIsSeekable();
		// Is speed, tempo and etc supports?
		// RealTime streams doesn't supports speed control
		virtual BOOL WINAPI DecoderIsRealTimeStream();
		// Return format type for current file, (MP3, OGG, AAC+, FLAC and etc)
		virtual PWCHAR WINAPI DecoderGetFormatType();
};

/* IAIMPInputPluginHeader */

class IAIMPInputPluginHeader: public IUnknown
{
	public:
		virtual PWCHAR WINAPI GetPluginAuthor();
		virtual PWCHAR WINAPI GetPluginInfo();
		virtual PWCHAR WINAPI GetPluginName();
		// Combination of the flags, See AIMP_INPUT_FLAG_XXX
		virtual DWORD  WINAPI GetPluginFlags(); 
		// Initialize / Finalize Plugin
		virtual BOOL WINAPI Initialize();
		virtual BOOL WINAPI Finalize();
		// Create decoder for the file / Stream
		virtual BOOL WINAPI CreateDecoder(PWCHAR AFileName, IAIMPInputPluginDecoder **ADecoder);
		virtual BOOL WINAPI CreateDecoderEx(IAIMPInputStream *AStream, IAIMPInputPluginDecoder **ADecoder);
		// Get FileInfo
		virtual BOOL WINAPI GetFileInfo(PWCHAR AFileName, TAIMPFileInfo *AFileInfo);
		// Return string format: "My Custom Format1|*.fmt1;*.fmt2;|"
		virtual PWCHAR WINAPI GetSupportsFormats();
};

// Export function name: AIMP_QueryInput
typedef BOOL (WINAPI *TAIMPInputPluginHeaderProc)(IAIMPInputPluginHeader **AHeader);

#endif