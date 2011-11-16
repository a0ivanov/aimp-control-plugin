//ToDo: Check methods with const parameters

/* ******************************************** */
/*                                              */
/*                AIMP Plugins API              */
/*             v3.00.943 (26.10.2011)           */
/*                Addons Plugins                */
/*                                              */
/*              (c) Artem Izmaylov              */
/*                 www.aimp.ru                  */
/*             Mail: artem@aimp.ru              */
/*              ICQ: 345-908-513                */
/*                                              */
/* ******************************************** */

// WARNING! Addons API functions doesn't support calling from non-main threads

#ifndef AIMPSDKAddonsH
#define AIMPSDKAddonsH

#include <windows.h>
#include <unknwn.h>
#include "AIMPSDKCore.h"
#include "AIMPSDKCommon.h"

const int WM_AIMP_LANG_CHANGED = WM_USER + 101;

// IAIMPAddonPlugin.GetPluginFlags
const int AIMP_ADDON_FLAGS_HAS_DIALOG = 1;

// TAIMPMenuItemInfo.Flags
const int AIMP_MENUITEM_CHECKBOX   = 1;
const int AIMP_MENUITEM_CHECKED    = 2;
const int AIMP_MENUITEM_ENABLED    = 4;
const int AIMP_MENUITEM_RADIOITEM  = 8;

// IAIMPAddonsMenuManager.MenuCreate, MenuIDs
const int AIMP_MENUID_MAIN_OPTIONS   = 0;
const int AIMP_MENUID_MAIN_FUNCTION  = 2;
const int AIMP_MENUID_MAIN_CONFIGS   = 3;
const int AIMP_MENUID_UTILITIES      = 4;
const int AIMP_MENUID_PLS_ADD        = 5;
const int AIMP_MENUID_PLS_JUMP       = 6;
const int AIMP_MENUID_PLS_FNC        = 7;
const int AIMP_MENUID_PLS_SEND       = 8;
const int AIMP_MENUID_PLS_DEL        = 9;
const int AIMP_MENUID_ADD            = 10;
const int AIMP_MENUID_DEL            = 11;
const int AIMP_MENUID_SORT           = 13;
const int AIMP_MENUID_MISC           = 14;
const int AIMP_MENUID_PLS            = 15;
const int AIMP_MENUID_TRAY           = 17;
const int AIMP_MENUID_EQ_LIB         = 18;

// PropertyIDs for IAIMPAddonsPlaylistManager.EntryPropertyGetValue / EntryPropertySetValue
const int AIMP_PLAYLIST_ENTRY_PROPERTY_DISPLAYTEXT                = 0;  // READONLY! ABuffer: Pointer to array of WideChar, ABufferSize: size of array in Bytes
const int AIMP_PLAYLIST_ENTRY_PROPERTY_FILENAME                   = 1;  // ABuffer: Pointer to array of WideChar, ABufferSize: size of array in Bytes
const int AIMP_PLAYLIST_ENTRY_PROPERTY_INFO                       = 2;  // ABuffer: Pointer to TAIMPFileInfo, ABufferSize: size of TAIMPFileInfo structure
const int AIMP_PLAYLIST_ENTRY_PROPERTY_PLAYINGSWITCH              = 3;  // ABuffer: Pointer to LongBool (32-bit Boolean), ABufferSize: SizeOf(LongBool)
const int AIMP_PLAYLIST_ENTRY_PROPERTY_STATISTICS                 = 4;  // READONLY! ABuffer: Pointer to TAIMPFileStatisticsInfo, ABufferSize: size of TAIMPFileStatisticsInfo structure
const int AIMP_PLAYLIST_ENTRY_PROPERTY_MARK                       = 5;  // ABuffer: Pointer to Integer, ABufferSize: SizeOf(Integer)
const int AIMP_PLAYLIST_ENTRY_PROPERTY_INDEX                      = 6;  // ABuffer: Pointer to Integer, ABufferSize: SizeOf(Integer)
const int AIMP_PLAYLIST_ENTRY_PROPERTY_SELECTED                   = 7;  // ABuffer: Pointer to LongBool (32-bit Boolean), ABufferSize: SizeOf(LongBool)

// PropertyIDs for IAIMPAddonsPlaylistManager.StoragePropertyGetValue / StoragePropertySetValue
const int AIMP_PLAYLIST_STORAGE_PROPERTY_NAME                     = 0;  // ABuffer: Pointer to array of WideChar, ABufferSize: size of array in Bytes
const int AIMP_PLAYLIST_STORAGE_PROPERTY_READONLY                 = 1;  // ABuffer: Pointer to LongBool (32-bit Boolean), ABufferSize: SizeOf(LongBool)
const int AIMP_PLAYLIST_STORAGE_PROPERTY_GROUPPING                = 10; // ABuffer: Pointer to LongBool (32-bit Boolean), ABufferSize: SizeOf(LongBool)
const int AIMP_PLAYLIST_STORAGE_PROPERTY_GROUPPING_OVERRIDEN      = 11; // ABuffer: Pointer to LongBool (32-bit Boolean), ABufferSize: SizeOf(LongBool)
const int AIMP_PLAYLIST_STORAGE_PROPERTY_GROUPPING_TEMPLATE       = 12; // ABuffer: Pointer to array of WideChar, ABufferSize: size of array in Bytes
const int AIMP_PLAYLIST_STORAGE_PROPERTY_GROUPPING_AUTOMERGING    = 13; // ABuffer: Pointer to LongBool (32-bit Boolean), ABufferSize: SizeOf(LongBool)
const int AIMP_PLAYLIST_STORAGE_PROPERTY_FORMATING_OVERRIDEN      = 20; // ABuffer: Pointer to LongBool (32-bit Boolean), ABufferSize: SizeOf(LongBool)
const int AIMP_PLAYLIST_STORAGE_PROPERTY_FORMATING_LINE1_TEMPLATE = 21; // ABuffer: Pointer to array of WideChar, ABufferSize: size of array in Bytes
const int AIMP_PLAYLIST_STORAGE_PROPERTY_FORMATING_LINE2_TEMPLATE = 22; // ABuffer: Pointer to array of WideChar, ABufferSize: size of array in Bytes
const int AIMP_PLAYLIST_STORAGE_PROPERTY_VIEW_OVERRIDEN           = 30; // ABuffer: Pointer to LongBool (32-bit Boolean), ABufferSize: SizeOf(LongBool)
const int AIMP_PLAYLIST_STORAGE_PROPERTY_VIEW_DURATION            = 31; // ABuffer: Pointer to LongBool (32-bit Boolean), ABufferSize: SizeOf(LongBool)
const int AIMP_PLAYLIST_STORAGE_PROPERTY_VIEW_EXPAND_BUTTONS      = 32; // ABuffer: Pointer to LongBool (32-bit Boolean), ABufferSize: SizeOf(LongBool)
const int AIMP_PLAYLIST_STORAGE_PROPERTY_VIEW_MARKS               = 33; // ABuffer: Pointer to LongBool (32-bit Boolean), ABufferSize: SizeOf(LongBool)
const int AIMP_PLAYLIST_STORAGE_PROPERTY_VIEW_NUMBERS             = 34; // ABuffer: Pointer to LongBool (32-bit Boolean), ABufferSize: SizeOf(LongBool)
const int AIMP_PLAYLIST_STORAGE_PROPERTY_VIEW_NUMBERS_ABSOLUTE    = 35; // ABuffer: Pointer to LongBool (32-bit Boolean), ABufferSize: SizeOf(LongBool)
const int AIMP_PLAYLIST_STORAGE_PROPERTY_VIEW_SECOND_LINE         = 36; // ABuffer: Pointer to LongBool (32-bit Boolean), ABufferSize: SizeOf(LongBool)
const int AIMP_PLAYLIST_STORAGE_PROPERTY_VIEW_SWITCHES            = 37; // ABuffer: Pointer to LongBool (32-bit Boolean), ABufferSize: SizeOf(LongBool)
const int AIMP_PLAYLIST_STORAGE_PROPERTY_FOCUSINDEX               = 50; // ABuffer: Pointer to Integer, ABufferSize: SizeOf(Integer)
const int AIMP_PLAYLIST_STORAGE_PROPERTY_TRACKINGINDEX            = 51; // ABuffer: Pointer to Integer, ABufferSize: SizeOf(Integer)
const int AIMP_PLAYLIST_STORAGE_PROPERTY_DURATION                 = 52; // READONLY! ABuffer: Pointer to Int64 (64-bit Integer), ABufferSize: SizeOf(Int64)
const int AIMP_PLAYLIST_STORAGE_PROPERTY_SIZE                     = 53; // READONLY! ABuffer: Pointer to Int64 (64-bit Integer), ABufferSize: SizeOf(Int64)

// IAIMPAddonsPlaylistManager.StorageSort
const int AIMP_PLAYLIST_SORT_TYPE_TITLE      = 1;
const int AIMP_PLAYLIST_SORT_TYPE_FILENAME   = 2;
const int AIMP_PLAYLIST_SORT_TYPE_DURATION   = 3;
const int AIMP_PLAYLIST_SORT_TYPE_ARTIST     = 4;
const int AIMP_PLAYLIST_SORT_TYPE_INVERSE    = 5;
const int AIMP_PLAYLIST_SORT_TYPE_RANDOMIZE  = 6;

// IAIMPAddonsPlaylistManager.StorageGetFiles:
const int AIMP_PLAYLIST_GETFILES_VISIBLE  = 1; // Files from collapsed groups will be excluded from list
const int AIMP_PLAYLIST_GETFILES_SELECTED = 2; // Only selected files will be processed

// IAIMPAddonsPlaylistManager.FormatString Flags
const int AIMP_PLAYLIST_FORMAT_MODE_PREVIEW  = 1;
const int AIMP_PLAYLIST_FORMAT_MODE_FILEINFO = 2;
const int AIMP_PLAYLIST_FORMAT_MODE_CURRENT  = 3;

// PropertyIDs for IAIMPAddonsPlaylistManager.GroupPropertyGetValue / GroupPropertySetValue
const int AIMP_PLAYLIST_GROUP_PROPERTY_NAME                       = 0;  // READONLY! ABuffer: Pointer to array of WideChar, ABufferSize: size of array in Bytes
const int AIMP_PLAYLIST_GROUP_PROPERTY_EXPANDED                   = 1;  // ABuffer: Pointer to LongBool (32-bit Boolean), ABufferSize: SizeOf(LongBool)
const int AIMP_PLAYLIST_GROUP_PROPERTY_DURATION                   = 2;  // READONLY! ABuffer: Pointer to Int64 (64-bit Integer), ABufferSize: SizeOf(Int64)

// Flags for IAIMPAddonsPlaylistManagerListener.StorageChanged
const int AIMP_PLAYLIST_NOTIFY_NAME           = 1;
const int AIMP_PLAYLIST_NOTIFY_SELECTION      = 2;
const int AIMP_PLAYLIST_NOTIFY_TRACKINGINDEX  = 4;
const int AIMP_PLAYLIST_NOTIFY_PLAYINDEX      = 8;
const int AIMP_PLAYLIST_NOTIFY_FOCUSINDEX     = 16;
const int AIMP_PLAYLIST_NOTIFY_CONTENT        = 32;
const int AIMP_PLAYLIST_NOTIFY_ENTRYINFO      = 64;
const int AIMP_PLAYLIST_NOTIFY_STATISTICS     = 128;
const int AIMP_PLAYLIST_NOTIFY_PLAYINGSWITCHS = 256;

// AConfigPathIDs for IAIMPAddonsPlayerManager.ConfigGetPath
const int AIMP_CFG_PATH_PROFILE  = 0;
const int AIMP_CFG_PATH_PLS      = 1;
const int AIMP_CFG_PATH_LNG      = 2;
const int AIMP_CFG_PATH_SKINS    = 3;
const int AIMP_CFG_PATH_PLUGINS  = 4;
const int AIMP_CFG_PATH_ICONS    = 5;
const int AIMP_CFG_PATH_ML       = 6;
const int AIMP_CFG_PATH_MODULES  = 8;
const int AIMP_CFG_PATH_HELP     = 9;
const int AIMP_CFG_PATH_ML_PLS   = 10;

// AFlags for IAIMPAddonsPlayerManager.SupportsExts
const int AIMP_SUPPORTS_EXTS_FORMAT_AUDIO    = 2;
const int AIMP_SUPPORTS_EXTS_FORMAT_PLAYLIST = 1;

DEFINE_GUID(IID_IAIMPAddonsConfigFile, 		0x41494D50, 0x0033, 0x434F, 0x52, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10);
DEFINE_GUID(IID_IAIMPAddonsCoverArtManager,	0x41494D50, 0x0033, 0x434F, 0x52, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11);
DEFINE_GUID(IID_IAIMPAddonsLanguageFile,	0x41494D50, 0x0033, 0x434F, 0x52, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x12);
DEFINE_GUID(IID_IAIMPAddonsMenuManager,		0x41494D50, 0x0033, 0x434F, 0x52, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13);
DEFINE_GUID(IID_IAIMPAddonsOptionsDialog, 	0x41494D50, 0x0033, 0x434F, 0x52, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14);
DEFINE_GUID(IID_IAIMPAddonsPlayerManager, 	0x41494D50, 0x0033, 0x434F, 0x52, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x15);
DEFINE_GUID(IID_IAIMPAddonsPlaylistManager, 0x41494D50, 0x0033, 0x434F, 0x52, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16);
DEFINE_GUID(IID_IAIMPAddonsPlaylistManagerListener, 0x41494D50, 0x0033, 0x434F, 0x52, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x17);
DEFINE_GUID(IID_IAIMPAddonsPlaylistStrings, 0x41494D50, 0x0033, 0x434F, 0x52, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18);
DEFINE_GUID(IID_IAIMPAddonsSkinsManager, 	0x41494D50, 0x0033, 0x434F, 0x52, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x19);
DEFINE_GUID(IID_IAIMPAddonsProxySettings, 	0x41494D50, 0x0033, 0x434F, 0x52, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20);

typedef void *HPLS;
typedef void *HPLSENTRY;
typedef void *HPLSGROUP;
typedef void *HAIMPMENU;

// Universal Handle for Active Playlist
#define ActivePlaylistHandle HPLS(-1)

typedef void (WINAPI *TAIMPMenuProc)(void *UserData, HAIMPMENU Handle);
typedef int  (WINAPI *TAIMPAddonsPlaylistManagerCompareProc)(TAIMPFileInfo *AFileInfo1, TAIMPFileInfo *AFileInfo2, void *UserData);
typedef BOOL (WINAPI *TAIMPAddonsPlaylistManagerDeleteProc) (TAIMPFileInfo *AFileInfo, void *AUserData);

#pragma pack(push, 1)

struct TAIMPMenuItemInfo
{
    DWORD StructSize; // SizeOf(TAIMPMenuItemInfo)
    HBITMAP Bitmap;   // 0 - no glyph
    PWCHAR Caption;
    DWORD Flags;   // Combination of AIMP_MENUITEM_XXX flags
    TAIMPMenuProc Proc;
    DWORD ShortCut;
    void *UserData;
};

struct TAIMPFileStatisticsInfo
{
    int Mark;
    double AddDate;
    int PlayCount;
    double PlayDate;
};

struct TAIMPSkinInfo
{
    DWORD  StructSize; // SizeOf(TAIMPSkinInfo)
    PWCHAR AuthorBuffer;
    DWORD  AuthorBufferLength;
    PWCHAR InfoBuffer;
    DWORD  InfoBufferLength;
    PWCHAR NameBuffer;
	DWORD  NameBufferLength;
    HBITMAP Preview;
};


typedef TAIMPMenuItemInfo *PAIMPMenuItemInfo;
typedef TAIMPFileStatisticsInfo *PAIMPFileStatisticsInfo;
typedef TAIMPSkinInfo *PAIMPSkinInfo;
 
#pragma pack(pop)

/* IAIMPAddonsOptionsDialogFrame */
// Must be implemented by Plugin
class IAIMPAddonsOptionsDialogFrame: public IUnknown
{
	public:
		virtual HWND   WINAPI FrameCreate(HWND AParentWindow);
		virtual void * WINAPI FrameData(); // unused
		virtual int    WINAPI FrameFlags(); // unused
		virtual PWCHAR WINAPI FrameName();
		virtual void   WINAPI FrameFree(HWND AWindow);
		virtual void   WINAPI FrameLoadConfigNotify();
		virtual void   WINAPI FrameSaveConfigNotify();
};

/* IAIMPAddonsOptionsDialog */
class IAIMPAddonsOptionsDialog: public IUnknown
{
	public:
		// Add custom frame
		virtual HRESULT WINAPI FrameAdd(IAIMPAddonsOptionsDialogFrame *AFrame);
		// Remove custom frame
		virtual HRESULT WINAPI FrameRemove(IAIMPAddonsOptionsDialogFrame *AFrame);
		// Call this method, when something changed on your frame
		virtual HRESULT WINAPI FrameModified(IAIMPAddonsOptionsDialogFrame *AFrame);
		// AForceShow - Execute Options Dialog, if dialog is not shown
		virtual HRESULT WINAPI FrameShow(IAIMPAddonsOptionsDialogFrame *AFrame, BOOL AForceShow);
};

/* IAIMPAddonsFileInfoRepository */
// Must be implemented by Plugin
class IAIMPAddonsFileInfoRepository: public IUnknown
{
	public:
		// You can fill custom info for AFile using AInfo property
		// If you was processed request, return S_OK
		virtual HRESULT WINAPI GetInfo(PWCHAR AFile, TAIMPFileInfo *AInfo) = 0;
};

/* IAIMPAddonsPlayingQueueController */
// Must be implemented by Plugin
class IAIMPAddonsPlayingQueueController: public IUnknown
{
	public:
		// [In\Out] params
		// In: Current Playlist ID & EntryIndex
		// Out: Next / Previous Playlist ID & EntryIndex
		// Notes:
		//   If previous controller has processed request, you don't receive it
		//   return S_OK if you was processed request or E_FAIL otherwise
		// Priorities:
		//   1) Queue
		//   2) Plugins
		//   3) Shuffle Manager
		//   4) Playlist
		virtual HRESULT WINAPI GetFile(HPLS *ID, int *AEntryIndex, BOOL ANext);
};

/* IAIMPAddonsConfigFile */
class IAIMPAddonsConfigFile: public IUnknown
{
	public:
		// functions returns S_OK, if value exists in configuration file
		virtual HRESULT WINAPI ReadString(PWCHAR ASectionName, PWCHAR AItemName, PWCHAR AValueBuffer,
			int ASectionNameSizeInChars, int AItemNameSizeInChars, int AValueBufferSizeInChars);
		virtual HRESULT WINAPI ReadInteger(PWCHAR ASectionName, PWCHAR AItemName, 
			int ASectionNameSizeInChars, int AItemNameSizeInChars, int *AValue);
		virtual HRESULT WINAPI WriteString(PWCHAR ASectionName, PWCHAR AItemName, PWCHAR AValueBuffer,
			int ASectionNameSizeInChars, int AItemNameSizeInChars, int AValueBufferSizeInChars);
		virtual HRESULT WINAPI WriteInteger(PWCHAR ASectionName, PWCHAR AItemName,
			int ASectionNameSizeInChars, int AItemNameSizeInChars, int AValue);
		virtual HRESULT WINAPI SectionExists(PWCHAR ASectionName, int ASectionNameSizeInChars);
		virtual HRESULT WINAPI SectionRemove(PWCHAR ASectionName, int ASectionNameSizeInChars);
};


/* IAIMPAddonsLanguageFile */
class IAIMPAddonsLanguageFile: public IUnknown
{
	public:
		virtual int WINAPI Version();
		virtual int WINAPI CurrentFile(PWCHAR ABuffer, int ABufferSizeInChars);
		virtual HRESULT WINAPI SectionExists(PWCHAR ASectionName, int ASectionNameSizeInChars);
		virtual HRESULT WINAPI ReadString(PWCHAR ASectionName, PWCHAR AItemName, PWCHAR AValueBuffer,
			int ASectionNameSizeInChars, int AItemNameSizeInChars, int AValueBufferSizeInChars);
		// When Language changed AIMP will send to WndHandle "WM_AIMP_LANG_CHANGED" message
		virtual HRESULT WINAPI Notification(HWND AWndHandle, BOOL ARegister);
};

/* IAIMPAddonsMenuManager */
class IAIMPAddonsMenuManager: public IUnknown
{
	public:
	    // AMenuID: see AIMP_MENUID_XXX
		virtual HAIMPMENU WINAPI MenuCreate(DWORD AMenuID, PAIMPMenuItemInfo AItemInfo);
		virtual HAIMPMENU WINAPI MenuCreateEx(HAIMPMENU AParentMenu, PAIMPMenuItemInfo AItemInfo);
		virtual DWORD WINAPI MenuTextToShortCut(PWCHAR ABuffer, int ABufferSizeInChars);
		virtual HRESULT WINAPI MenuRemove(HAIMPMENU AHandle);
		virtual HRESULT WINAPI MenuUpdate(HAIMPMENU AHandle, PAIMPMenuItemInfo AItemInfo);
};

/* IAIMPAddonsProxySettings */
class IAIMPAddonsProxySettings: public IUnknown
{
	public:
		// Receiving Proxy Server params in "server:port" format
		// Returns E_FAIL, if proxy doesn't use, and S_OK otherwise
		virtual HRESULT WINAPI GetProxyParams(PWCHAR AServerBuffer, int AServerBufferSizeInChars);
		// Receiving Proxy Server user autorization params
		// Returns E_FAIL, if proxy server or user authorization doesn't use
		virtual HRESULT WINAPI GetProxyAuthorizationParams(
			PWCHAR AUserNameBuffer, int AUserNameBufferSizeInChars,
			PWCHAR AUserPassBuffer, int AUserPassBufferSizeInChars);
};

/* IAIMPAddonsPlaylistStrings */  
class IAIMPAddonsPlaylistStrings: public IUnknown
{
	public:
		virtual HRESULT WINAPI ItemAdd(PWCHAR AFileName, TAIMPFileInfo *AInfo);
		virtual int     WINAPI ItemGetCount();
		virtual HRESULT WINAPI ItemGetFileName(int AIndex, PWCHAR ABuffer, int ABufferSizeInChars);
		virtual HRESULT WINAPI ItemGetInfo(int AIndex, TAIMPFileInfo *AInfo);
};

/* IAIMPAddonsPlaylistManagerListener */
// must be implemented by plugin
class IAIMPAddonsPlaylistManagerListener: public IUnknown
{
	public:
		virtual void WINAPI StorageActivated(HPLS ID);
		virtual void WINAPI StorageAdded(HPLS ID);
		virtual void WINAPI StorageChanged(HPLS ID, DWORD AFlags);
		virtual void WINAPI StorageRemoved(HPLS ID);
};

/* IAIMPAddonsPlaylistManager */
// See ActivePlaylistHandle constant
class IAIMPAddonsPlaylistManager: public IUnknown
{
	public:
		// AIMP_PLAYLIST_FORMAT_MODE_PREVIEW  - template will be expand as preview
		// AIMP_PLAYLIST_FORMAT_MODE_FILEINFO - put to AData param pointer to TAIMPFileInfo struct
		// AIMP_PLAYLIST_FORMAT_MODE_CURRENT  - format current file info
		// AString buffer will be automaticly freed, you must make copy
		virtual HRESULT WINAPI FormatString(PWCHAR ATemplate, int ATemplateSizeInChars,	int AMode, void *AData, PWCHAR *AString);
		// Listeners
		virtual HRESULT WINAPI ListenerAdd(IAIMPAddonsPlaylistManagerListener *AListener);
		virtual HRESULT WINAPI ListenerRemove(IAIMPAddonsPlaylistManagerListener *AListener);
		// See AIMP_PLAYLIST_ENTRY_PROPERTY_XXX
		virtual HRESULT WINAPI EntryDelete(HPLSENTRY AEntry);
		virtual HRESULT WINAPI EntryGetGroup(HPLSENTRY AEntry, HPLSGROUP *AGroup);
		virtual HRESULT WINAPI EntryPropertyGetValue(HPLSENTRY AEntry, int APropertyID, void *ABuffer, int ABufferSize);
		virtual HRESULT WINAPI EntryPropertySetValue(HPLSENTRY AEntry, int APropertyID, void *ABuffer, int ABufferSize);
		virtual HRESULT WINAPI EntryReloadInfo(HPLSENTRY AEntry);
		// See AIMP_PLAYLIST_GROUP_PROPERTY_XXX
		virtual HRESULT WINAPI GroupPropertyGetValue(HPLSGROUP AGroup, int APropertyID, void *ABuffer, int ABufferSize);
		virtual HRESULT WINAPI GroupPropertySetValue(HPLSGROUP AGroup, int APropertyID, void *ABuffer, int ABufferSize);
		//
		virtual HPLS 	WINAPI StorageActiveGet();
		virtual HRESULT WINAPI StorageActiveSet(HPLS ID);
		virtual HRESULT WINAPI StorageAddEntries(HPLS ID, IAIMPAddonsPlaylistStrings *AObjects); // Add Objects to playlist. "Objects" can contains: shortcuts, files, folder, playlists
		virtual HPLS 	WINAPI StorageCreate(PWCHAR AName, BOOL AActivate);
		virtual HPLS 	WINAPI StorageCreateFromFile(PWCHAR AFileName, BOOL AActivate, BOOL AStartPlay);
		virtual HPLS 	WINAPI StoragePlayingGet();
		//
		virtual HPLS 	  WINAPI StorageGet(int AIndex);
		virtual int  	  WINAPI StorageGetCount();
		virtual HPLSENTRY WINAPI StorageGetEntry(HPLS ID, int AEntryIndex);
		virtual int  	  WINAPI StorageGetEntryCount(HPLS ID);
		virtual HRESULT   WINAPI StorageGetFiles(HPLS ID, int AFlags, IAIMPAddonsPlaylistStrings **AFiles); // Flags: Use combination of the AIMP_PLAYLIST_GETFILES_XXX flags
		virtual HPLSGROUP WINAPI StorageGetGroup(HPLS ID, int AGroupIndex);
		virtual	int		  WINAPI StorageGetGroupCount(HPLS ID);
		//
		virtual HRESULT WINAPI StorageDelete(HPLS ID, int AEntryIndex);
		virtual HRESULT WINAPI StorageDeleteAll(HPLS ID);
		virtual HRESULT WINAPI StorageDeleteByFilter(HPLS ID, BOOL APhysically,	TAIMPAddonsPlaylistManagerDeleteProc AFilterProc, void *AUserData);
		virtual HRESULT WINAPI StorageRemove(HPLS ID); // Remove playlist storage from manager (like "close playlist")
		//
		virtual HRESULT WINAPI StorageSort(HPLS ID, int ASortType); // Predefined Sorting, see AIMP_PLAYLIST_SORT_TYPE_XXX
		virtual HRESULT WINAPI StorageSortCustom(HPLS ID, TAIMPAddonsPlaylistManagerCompareProc ACompareProc, void *AUserData);
		virtual HRESULT WINAPI StorageSortTemplate(HPLS ID, PWCHAR ABuffer, int ABufferSizeInChars);
		// Lock / Unlock playlist calculation and drawing
		virtual HRESULT WINAPI StorageBeginUpdate(HPLS ID);
		virtual HRESULT WINAPI StorageEndUpdate(HPLS ID);
		// See AIMP_PLAYLIST_STORAGE_PROPERTY_XXX
		virtual HRESULT WINAPI StoragePropertyGetValue(HPLS ID, int APropertyID, void *ABuffer, int ABufferSize);
		virtual HRESULT WINAPI StoragePropertySetValue(HPLS ID, int APropertyID, void *ABuffer, int ABufferSize);
		// Queue
		virtual HRESULT WINAPI QueueEntryAdd(HPLSENTRY AEntry, BOOL AInsertAtQueueBegining);
		virtual HRESULT WINAPI QueueEntryRemove(HPLSENTRY AEntry);
};

/* IAIMPAddonsCoverArtManager */
class IAIMPAddonsCoverArtManager: public IUnknown
{
	public:
		// Picture will be proportional stretched to ADisplaySize value, if it assigned
		// ACoverArtFileName: can be NULL (todo: doc)
		virtual HBITMAP WINAPI CoverArtGetForFile(PWCHAR AFileName, SIZE_T *ADisplaySize,
			PWCHAR ACoverArtFileNameBuffer, int ACoverArtFileNameBufferSizeInChars);
		// Work with CoverArt of playing file,
		// if file is playing and CoverArt exists, functions returns S_OK
		virtual HRESULT WINAPI CoverArtDraw(HDC DC, RECT *R); // CoverArt will be proportional stretched to R value
		virtual HRESULT WINAPI CoverArtGetFileName(PWCHAR ABuffer, int ABufferSizeInChars);
		virtual HRESULT WINAPI CoverArtGetSize(SIZE_T *ASize);
};

/* IAIMPAddonsPlayerManager */
class IAIMPAddonsPlayerManager: public IUnknown
{
	public:
		virtual HRESULT WINAPI PlayEntry(HPLSENTRY AEntry);
		virtual HRESULT WINAPI PlayFile(PWCHAR AFileName, BOOL AFailIfNotExistsInPlaylist);
		virtual HRESULT WINAPI PlayStorage(HPLS ID, int AEntryIndex /* -1 - default*/);
		// Register / Unregister custom FileInfo repository, you can put own info about the any file
		virtual HRESULT WINAPI FileInfoRepository(IAIMPAddonsFileInfoRepository *ARepository, BOOL ARegister);
		// if AFileName = nil: Get information about currently playing file
		virtual HRESULT WINAPI FileInfoQuery(PWCHAR AFileName, TAIMPFileInfo *AInfo);
		// Register / Unregister custom playing queue controller
		virtual HRESULT WINAPI PlayingQueueController(IAIMPAddonsPlayingQueueController *AController, BOOL ARegister);
		// see AIMP_CFG_PATH_XXX
		virtual HRESULT WINAPI ConfigGetPath(int AConfigPathID, PWCHAR ABuffer, int ABufferSizeInChars);
		// see AIMP_SUPPORTS_EXTS_XXX
		virtual HRESULT WINAPI SupportsExts(DWORD AFlags, PWCHAR ABuffer, int ABufferSizeInChars);
};

/* IAIMPAddonsSkinsManager */
class IAIMPAddonsSkinsManager: public IUnknown
{
	public:
		// AColorHue & AColorHueIntensity - optional
		virtual HRESULT WINAPI GetCurrentSettings(PWCHAR ASkinLocalFileNameBuffer,
			int ASkinLocalFileNameBufferSizeInChars, int *AColorHue, int *AColorHueIntensity);
		// Get Info about Skin
		// + Skin can be placed anywhere
		// + if AFileNameBuffer contains empty string - function return information about build-in skin
		// WARNING!!! You must manually destroy TAIMPSkinInfo.Preview if it no longer need
		virtual HRESULT WINAPI GetSkinInfo(PWCHAR AFileNameBuffer, int AFileNameBufferSizeInChars, PAIMPSkinInfo ASkinInfo);
		// Skin must be placed in %AIMP_Skins% directory (see IAIMPAddonsPlayerManager.ConfigGetPath)
		// AColorHue: [0..255], Default: 0 (don't change hue)
		// AColorHueIntensity [0..100], Default: 100 (don't change Hue Intensity), depends from AColorHue value
		virtual HRESULT WINAPI Select(PWCHAR ASkinLocalFileName, int AColorHue, int AColorHueIntensity);
		// Conversion between HSL and RGB color spaces
		virtual HRESULT WINAPI HSLToRGB(BYTE H, BYTE S, BYTE L, BYTE *R, BYTE *G, BYTE *B);
		virtual HRESULT WINAPI RGBToHSL(BYTE R, BYTE G, BYTE B, BYTE *H, BYTE *S, BYTE *L);
};

/* IAIMPAddonPlugin */
// Must be implemented by Plugin

class IAIMPAddonPlugin: public IUnknown
{
	public:
		virtual PWCHAR WINAPI GetPluginAuthor() = 0;
		virtual PWCHAR WINAPI GetPluginInfo() = 0;
		virtual PWCHAR WINAPI GetPluginName() = 0;
		virtual DWORD  WINAPI GetPluginFlags() = 0; // See AIMP_ADDON_FLAGS_XXX
		virtual HRESULT WINAPI Initialize(IAIMPCoreUnit *ACoreUnit) = 0;
		virtual HRESULT WINAPI Finalize() = 0;
		virtual HRESULT WINAPI ShowSettingsDialog(HWND AParentWindow) = 0;
};

// Export function name: AIMP_QueryAddon3
typedef BOOL (WINAPI *TAIMPAddonPluginHeaderProc)(IAIMPAddonPlugin **AHeader);
#endif