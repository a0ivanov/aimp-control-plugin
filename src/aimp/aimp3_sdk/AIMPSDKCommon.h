/* ******************************************** */
/*                                              */
/*                AIMP Plugins API              */
/*             v3.00.960 (01.12.2011)           */
/*                Common Objects                */
/*                                              */
/*              (c) Artem Izmaylov              */
/*                 www.aimp.ru                  */
/*             Mail: artem@aimp.ru              */
/*              ICQ: 345-908-513                */
/*                                              */
/* ******************************************** */

#ifndef AIMPSDKCommonH
#define AIMPSDKCommonH

#include <windows.h>
#include <unknwn.h>

#pragma pack(push, 1)
struct TAIMPFileInfo
{
	DWORD StructSize;
	//
	BOOL  Active;
	DWORD BitRate;
	DWORD Channels;
	DWORD Duration;
	INT64 FileSize;
	DWORD Rating;
	DWORD SampleRate;
	DWORD TrackNumber;
	//
	DWORD AlbumBufferSizeInChars;
	DWORD ArtistBufferSizeInChars;
	DWORD DateBufferSizeInChars;
	DWORD FileNameBufferSizeInChars;
	DWORD GenreBufferSizeInChars;
	DWORD TitleBufferSizeInChars;
	//
	PWCHAR AlbumBuffer;
	PWCHAR ArtistBuffer;
	PWCHAR DateBuffer;
	PWCHAR FileNameBuffer;
	PWCHAR GenreBuffer;
	PWCHAR TitleBuffer;
};
#pragma pack(pop)

#endif