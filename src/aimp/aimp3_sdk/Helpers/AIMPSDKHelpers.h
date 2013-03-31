/* ******************************************** */
/*                                              */
/*                AIMP Plugins API              */
/*             v3.50.1200 (02.11.2012)          */
/*                Addons Plugins                */
/*                                              */
/*              (c) Artem Izmaylov              */
/*                 www.aimp.ru                  */
/*             Mail: artem@aimp.ru              */
/*              ICQ: 345-908-513                */
/*                                              */
/* ******************************************** */

/*
    C++ port of TAIMPAddonsPlaylistStrings class is made by Alexey Ivanov
*/

#ifndef AIMPSDKHelpersH
#define AIMPSDKHelpersH

#include "..\AIMPSDKAddons.h"
#include <Unknwn.h>
#include <vector>
#include <memory>

//! Helper implements IUnknown interface.
template <typename T>
class IUnknownInterfaceImpl : public T
{
public:

    IUnknownInterfaceImpl()
        : reference_count_(0)
    {}

    virtual ~IUnknownInterfaceImpl() {}

    virtual HRESULT WINAPI QueryInterface(REFIID riid, LPVOID* ppvObj) {
        if (!ppvObj) {
            return E_POINTER;
        }

        if (IID_IUnknown == riid) {
            *ppvObj = this;
            AddRef();
            return S_OK;
        }

        return E_NOINTERFACE;
    }

    virtual ULONG WINAPI AddRef(void)
        { return ++reference_count_; }

    virtual ULONG WINAPI Release(void) {
        ULONG reference_count = --reference_count_;

        if (reference_count == 0) {
            delete this;
        }

        return reference_count;
    }

private:

    ULONG reference_count_;
};

inline bool SafePutStringToBuffer(const std::wstring& src, PWCHAR dst, int dstLength);
inline bool FileInfoIsValid(TAIMPFileInfo* info);

class AIMPFileInfoAdapter
{
public:
    AIMPFileInfoAdapter()
        :
        info_() // fills all fields with 0.
    {
        info_.StructSize = sizeof(info_);
    }

#define RemoveConst(str) const_cast<std::wstring::value_type*>(str)

    AIMPFileInfoAdapter(const TAIMPFileInfo& info)
        :
        info_(info)
    {
		album_.   assign(info.AlbumBuffer,    info.AlbumBufferSizeInChars);
        artist_.  assign(info.ArtistBuffer,   info.ArtistBufferSizeInChars);
        date_.    assign(info.DateBuffer,     info.DateBufferSizeInChars);
        fileName_.assign(info.FileNameBuffer, info.FileNameBufferSizeInChars);
        genre_.   assign(info.GenreBuffer,    info.GenreBufferSizeInChars);
        title_.   assign(info.TitleBuffer,    info.TitleBufferSizeInChars);

        info_.AlbumBuffer    = RemoveConst( album_.c_str() );
        info_.ArtistBuffer   = RemoveConst( artist_.c_str() );
        info_.DateBuffer     = RemoveConst( date_.c_str() );
        info_.FileNameBuffer = RemoveConst( fileName_.c_str() );
        info_.GenreBuffer    = RemoveConst( genre_.c_str() );
        info_.TitleBuffer    = RemoveConst( title_.c_str() );
    }

    void fileName(const PWCHAR filename)
    {
        const size_t length = wcslen(filename);
        fileName_.assign(filename, length);
        info_.FileNameBuffer = RemoveConst( fileName_.c_str() );
        info_.FileNameBufferSizeInChars = length;
    }

#undef RemoveConst

    const std::wstring& fileName() const
    {
        return fileName_;
    }

    bool copy(TAIMPFileInfo* dst) const
    {
        if ( FileInfoIsValid(dst) ) {
            dst->Active      = info_.Active;
            dst->BitRate     = info_.BitRate;
            dst->Channels    = info_.Channels;
            dst->Duration    = info_.Duration;
            dst->FileSize    = info_.FileSize;
            dst->Rating      = info_.Rating;
            dst->SampleRate  = info_.SampleRate;
            dst->TrackNumber = info_.TrackNumber;

            return SafePutStringToBuffer(album_,    dst->AlbumBuffer,    dst->AlbumBufferSizeInChars)
                && SafePutStringToBuffer(artist_,   dst->ArtistBuffer,   dst->ArtistBufferSizeInChars)
                && SafePutStringToBuffer(date_,     dst->DateBuffer,     dst->DateBufferSizeInChars)
                && SafePutStringToBuffer(fileName_, dst->FileNameBuffer, dst->FileNameBufferSizeInChars)
                && SafePutStringToBuffer(genre_,    dst->GenreBuffer,    dst->GenreBufferSizeInChars)
                && SafePutStringToBuffer(title_,    dst->TitleBuffer,    dst->TitleBufferSizeInChars);
        }
        return false;
    }

    const TAIMPFileInfo& info() const 
    {
        return info_;
    }

private:

    TAIMPFileInfo info_;

    std::wstring album_;
    std::wstring artist_;
    std::wstring date_;
    std::wstring fileName_;
    std::wstring genre_;
    std::wstring title_;
};

/*
    Objects of this class must be created by operator new, since it will be destroyed by operator delete.
*/
class TAIMPAddonsPlaylistStrings : public IUnknownInterfaceImpl<IAIMPAddonsPlaylistStrings>
{
public:

    virtual ~TAIMPAddonsPlaylistStrings()
    {
    }

    virtual HRESULT WINAPI ItemAdd(PWCHAR AFileName, TAIMPFileInfo *AInfo)
    {
        HRESULT r = E_POINTER;
        try { // do not propagate exceptions, since it called from AIMP and will not be handled.
            AIMPFileInfoAdapter* addedinfo = NULL;
            if (AInfo) {
                infoList_.push_back(*AInfo); // ctor will do all job.
                addedinfo = &infoList_.back();
                r = S_OK;
            }

            if (AFileName) {
                if (!addedinfo) {
                    infoList_.push_back( AIMPFileInfoAdapter() );
                    addedinfo = &infoList_.back();
                }
                addedinfo->fileName(AFileName);
                r = S_OK;
            }
        } catch (...) {
            r = E_FAIL;
        }
        return r;
    }

    virtual int WINAPI ItemGetCount()
    {
        return infoList_.size();
    }

    virtual HRESULT WINAPI ItemGetFileName(int AIndex, PWCHAR ABuffer, int ABufferSizeInChars)
    {
        HRESULT r = E_INVALIDARG;
        try { // do not propagate exceptions, since it called from AIMP and will not be handled.
            if ( AIndex >= 0 && AIndex < ItemGetCount() ) {
                const AIMPFileInfoAdapter& info = infoList_[AIndex];
                if ( SafePutStringToBuffer(info.fileName(), ABuffer, ABufferSizeInChars) ) {
                    r = S_OK;
                }
            }
        } catch (...) {
            r = E_FAIL;
        }
        return r;
    }

    virtual HRESULT WINAPI ItemGetInfo(int AIndex, TAIMPFileInfo *AInfo)
    {
        HRESULT r = E_INVALIDARG;
        try { // do not propagate exceptions, since it called from AIMP and will not be handled.
            if ( AIndex >= 0 && AIndex < ItemGetCount() ) {
                const AIMPFileInfoAdapter& info = infoList_[AIndex];
                if ( info.copy(AInfo) ) {
                    r = S_OK;
                }
            }
        } catch (...) {
            r = E_FAIL;
        }
        return r;
    }

private:

    typedef std::vector<AIMPFileInfoAdapter> AIMPFileInfoList;
    AIMPFileInfoList infoList_;
};

inline bool SafePutStringToBuffer(const std::wstring& src, PWCHAR dst, int dstLength)
{
    if (dst && dstLength >= 0) {
        // wstring::copy is deprecated without _SCL_SECURE_NO_WARNINGS macro.
        //src.copy(dst, dstLength); // The function does not append a null character after the content copied.

        const size_t dstLength_ = dstLength,
                     copyLength = (std::min)(src.length(), dstLength_);
        //static_assert( sizeof(std::wstring::value_type) == sizeof(WCHAR) );
        memset( dst, 0, dstLength_ * sizeof(WCHAR) );
        memcpy( dst, src.c_str(), copyLength * sizeof(WCHAR) );
        return true;
    }
    return false;
}

inline bool FileInfoIsValid(TAIMPFileInfo* info)
{
    return info && info->StructSize == sizeof(TAIMPFileInfo);
}

#endif
