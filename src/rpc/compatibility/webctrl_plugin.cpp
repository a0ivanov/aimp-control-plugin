#include "stdafx.h"
#include "webctrl_plugin.h"
#include "aimp/manager.h"
#include "aimp/manager2.6.h"
#include "aimp/manager3.0.h"
#include "aimp/manager3.6.h"
#include "aimp/aimp3.60_sdk/aimp3_60_sdk.h"
#include "aimp/aimp3.60_sdk/Helpers/AIMPString.h"
#include "plugin/logger.h"

namespace {
using namespace ControlPlugin::PluginLogger;
ModuleLoggerType& logger()
    { return getLogManager().getModuleLogger<Rpc::RequestHandler>(); }
}

namespace AimpRpcMethods
{

void EmulationOfWebCtlPlugin::initMethodNamesMap()
{
    boost::assign::insert(method_names_)
        ("get_playlist_list",  get_playlist_list)
        ("get_version_string", get_version_string)
        ("get_version_number", get_version_number)
        ("get_update_time",    get_update_time)
        ("get_playlist_songs", get_playlist_songs)
        ("get_playlist_crc",   get_playlist_crc)
        ("get_player_status",  get_player_status)
        ("get_song_current",   get_song_current)
        ("get_volume",         get_volume)
        ("set_volume",         set_volume)
        ("get_track_position", get_track_position)
        ("set_track_position", set_track_position)
        ("get_track_length",   get_track_length)
        ("get_custom_status",  get_custom_status)
        ("set_custom_status",  set_custom_status)
        ("set_song_play",      set_song_play)
        ("set_song_position",  set_song_position)
        ("set_player_status",  set_player_status)
        ("player_play",        player_play)
        ("player_pause",       player_pause)
        ("player_stop",        player_stop)
        ("player_prevous",     player_prevous)
        ("player_next",        player_next)
        ("playlist_sort",      playlist_sort)
        ("playlist_add_file",  playlist_add_file)
        ("playlist_del_file",  playlist_del_file)
        ("playlist_queue_add", playlist_queue_add)
        ("playlist_queue_remove", playlist_queue_remove)
        ("download_song",      download_song);
}

const EmulationOfWebCtlPlugin::METHOD_ID* EmulationOfWebCtlPlugin::getMethodID(const std::string& method_name) const
{
    const auto it = method_names_.find(method_name);
    return it != method_names_.end() ? &it->second
                                     : nullptr;
}

namespace WebCtl
{

const char * const kVERSION = "2.6.5.0";
const unsigned int kVERSION_INT = 2650;
unsigned int update_time = 10,
             cache_time  = 60;

std::string urldecode(const std::string& url_src)
{
    std::string url_ret = "";
    const size_t len = url_src.length();
    char tmpstr[5] = { '0', 'x', '_', '_', '\0' };

    for (size_t i = 0; i < len; i++) {
        char ch = url_src.at(i);
        if (ch == '%') {
            if ( i+2 >= len ) {
                return "";
            }
            tmpstr[2] = url_src.at( ++i );
            tmpstr[3] = url_src.at( ++i );
            ch = (char)strtol(tmpstr, nullptr, 16);
        }
        url_ret += ch;
    }
    return url_ret;
}

} // namespace WebCtl

void EmulationOfWebCtlPlugin::getPlaylistList(std::ostringstream& out)
{
    out << "[";

    if ( AIMPPlayer::AIMPManager26* aimp2_manager = dynamic_cast<AIMPPlayer::AIMPManager26*>(&aimp_manager_) ) {
        boost::intrusive_ptr<AIMP2SDK::IAIMP2PlaylistManager2> aimp_playlist_manager(aimp2_manager->aimp2_playlist_manager_);
        const unsigned int playlist_name_length = 256;
        std::wstring playlist_name;
        const short playlists_count = aimp_playlist_manager->AIMP_PLS_Count();
        for (short i = 0; i < playlists_count; ++i) {
            int playlist_id;
            aimp_playlist_manager->AIMP_PLS_ID_By_Index(i, &playlist_id);
            INT64 duration, size;
            aimp_playlist_manager->AIMP_PLS_GetInfo(playlist_id, &duration, &size);
            playlist_name.resize(playlist_name_length, 0);
            aimp_playlist_manager->AIMP_PLS_GetName( playlist_id, &playlist_name[0], playlist_name.length() );
            playlist_name.resize( wcslen( playlist_name.c_str() ) );
            using namespace Utilities;
            replaceAll(L"\"", 1,
                       L"\\\"", 2,
                       &playlist_name);
            if (i != 0) {
                out << ',';
            }
            out << "{\"id\":" << playlist_id << ",\"duration\":" << duration << ",\"size\":" << size << ",\"name\":\"" << StringEncoding::utf16_to_utf8(playlist_name) << "\"}";
        }
    } else if ( AIMPPlayer::AIMPManager30* aimp3_manager = dynamic_cast<AIMPPlayer::AIMPManager30*>(&aimp_manager_) ) {
        using namespace AIMP3SDK;

        boost::intrusive_ptr<AIMP3SDK::IAIMPAddonsPlaylistManager> aimp_playlist_manager(aimp3_manager->aimp3_playlist_manager_);
        const unsigned int playlist_name_length = 256;
        std::wstring playlist_name;
        const int playlists_count = aimp_playlist_manager->StorageGetCount();
        for (short i = 0; i < playlists_count; ++i) {
            const AIMP3SDK::HPLS playlist_handle = aimp_playlist_manager->StorageGet(i);
            INT64 duration, size;
            aimp_playlist_manager->StoragePropertyGetValue( playlist_handle, AIMP_PLAYLIST_STORAGE_PROPERTY_DURATION, &duration, sizeof(duration) );
            aimp_playlist_manager->StoragePropertyGetValue( playlist_handle, AIMP_PLAYLIST_STORAGE_PROPERTY_SIZE,     &size,     sizeof(size) );
            playlist_name.resize(playlist_name_length, 0);
            aimp_playlist_manager->StoragePropertyGetValue( playlist_handle, AIMP_PLAYLIST_STORAGE_PROPERTY_NAME, &playlist_name[0], playlist_name.length() );
            playlist_name.resize( wcslen( playlist_name.c_str() ) );
            using namespace Utilities;
            replaceAll(L"\"", 1,
                       L"\\\"", 2,
                       &playlist_name);
            if (i != 0) {
                out << ',';
            }
            out << "{\"id\":" << cast<PlaylistID>(playlist_handle) << ",\"duration\":" << duration << ",\"size\":" << size << ",\"name\":\"" << StringEncoding::utf16_to_utf8(playlist_name) << "\"}";
        }
    } else if ( AIMPPlayer::AIMPManager36* aimp36_manager = dynamic_cast<AIMPPlayer::AIMPManager36*>(&aimp_manager_) ) {
        using namespace AIMP36SDK;
        using namespace Utilities;

        boost::intrusive_ptr<AIMP36SDK::IAIMPServicePlaylistManager> aimp_service_playlist_manager(aimp36_manager->aimp_service_playlist_manager_);
    
        std::wstring playlist_name;
        for (int i = 0, count = aimp_service_playlist_manager->GetLoadedPlaylistCount(); i != count; ++i) {
            IAIMPPlaylist* playlist_tmp;
            HRESULT r = aimp_service_playlist_manager->GetLoadedPlaylist(i, &playlist_tmp);
            if (S_OK != r) {
                throw std::runtime_error(MakeString() << "IAIMPServicePlaylistManager::GetLoadedPlaylist failed. Result " << r);
            }
            boost::intrusive_ptr<IAIMPPlaylist> playlist(playlist_tmp, false);

            IAIMPPropertyList* playlist_propertylist_tmp;
            r = playlist->QueryInterface(IID_IAIMPPropertyList,
                                         reinterpret_cast<void**>(&playlist_propertylist_tmp));
            if (S_OK != r) {
                throw std::runtime_error(MakeString() << "playlist->QueryInterface(IID_IAIMPPropertyList) failed. Result " << r);
            }
            boost::intrusive_ptr<IAIMPPropertyList> playlist_propertylist(playlist_propertylist_tmp, false);
            playlist_propertylist_tmp = nullptr;

            INT64 size = 0;
            r = playlist_propertylist->GetValueAsInt64(AIMP_PLAYLIST_PROPID_SIZE, &size);
            if (S_OK != r) {
                throw std::runtime_error(MakeString() << "IAIMPPropertyList::GetValueAsInt64(AIMP_PLAYLIST_PROPID_SIZE) failed. Result " << r);
            }

            double duration_sec_tmp_;
            r = playlist_propertylist->GetValueAsFloat(AIMP_PLAYLIST_PROPID_DURATION, &duration_sec_tmp_);
            if (S_OK != r) {
                throw std::runtime_error(MakeString() << "IAIMPPropertyList::GetValueAsFloat(AIMP_PLAYLIST_PROPID_DURATION) failed. Result " << r);
            }
            const INT64 duration_ms = static_cast<INT64>(duration_sec_tmp_ * 1000.);

            IAIMPString* name_tmp;
            r = playlist_propertylist->GetValueAsObject(AIMP_PLAYLIST_PROPID_NAME, IID_IAIMPString, reinterpret_cast<void**>(&name_tmp));
            if (S_OK != r) {
                throw std::runtime_error(MakeString() << "IAIMPAddonsPlaylistManager::StoragePropertyGetValue(AIMP_PLAYLIST_STORAGE_PROPERTY_NAME) failed. Result " << r);
            }
            IAIMPString_ptr nameStr(name_tmp, false);
            playlist_name.assign(nameStr->GetData(), nameStr->GetLength());

            using namespace Utilities;
            replaceAll(L"\"", 1,
                       L"\\\"", 2,
                       &playlist_name);
            if (i != 0) {
                out << ',';
            }
            out << "{\"id\":" << cast<PlaylistID>(playlist.get()) << ",\"duration\":" << duration_ms << ",\"size\":" << size << ",\"name\":\"" << StringEncoding::utf16_to_utf8(playlist_name) << "\"}";
        }
    }

    out << "]";
}

namespace Support {

using namespace AIMP36SDK;

HRESULT getString(IAIMPPropertyList* property_list, const int property_id, IAIMPString_ptr* value)
{
    assert(property_list);
    assert(value);

    IAIMPString* value_tmp;
    HRESULT r = property_list->GetValueAsObject(property_id, IID_IAIMPString, reinterpret_cast<void**>(&value_tmp));
    if (S_OK == r) {
        IAIMPString_ptr(value_tmp, false).swap(*value);

#ifndef NDEBUG
        //BOOST_LOG_SEV(logger(), debug) << "getString(property_id = " << property_id << ") value: " << StringEncoding::utf16_to_utf8(value_tmp->GetData(), value_tmp->GetData() + value_tmp->GetLength());
#endif
    }
    return r;
}

}

void EmulationOfWebCtlPlugin::getPlaylistSongs(int playlist_id, bool ignore_cache, bool return_crc, int offset, int size, std::ostringstream& out)
{
    typedef std::string PlaylistCacheKey;
    typedef unsigned int Time;
    typedef int Crc32;
    typedef std::string CachedPlaylist;

    typedef std::map<PlaylistCacheKey,
                     std::pair< Time, std::pair<Crc32, CachedPlaylist> >
                    > PlaylistCache;
    static PlaylistCache playlist_cache;

    bool cacheFound = false;

    std::ostringstream os;
    os << offset << "_" << size << "_" << playlist_id;
    std::string cacheKey = os.str();

    unsigned int tickCount = GetTickCount();

    PlaylistCache::iterator it;

    if (!ignore_cache) {
        // not used, we have one thread. concurencyInstance.EnterReader();
        // check last time
        it = playlist_cache.find(cacheKey);
        if ( (it != playlist_cache.end()) && (tickCount - it->second.first < WebCtl::cache_time * 1000) ) {
            cacheFound = true;
            if (!return_crc) {
                out << it->second.second.second;
            } else {
                out << it->second.second.first;
            }
        }

        // not used, we have one thread. concurencyInstance.LeaveReader();
    }

    if (cacheFound) {
        return;
    }

    // not used, we have one thread. concurencyInstance.EnterWriter();

    AIMPPlayer::AIMPManager26* aimp2_manager = dynamic_cast<AIMPPlayer::AIMPManager26*>(&aimp_manager_);
    AIMPPlayer::AIMPManager30* aimp3_manager = dynamic_cast<AIMPPlayer::AIMPManager30*>(&aimp_manager_);
    AIMPPlayer::AIMPManager36* aimp36_manager = dynamic_cast<AIMPPlayer::AIMPManager36*>(&aimp_manager_);

    const AIMP3SDK::HPLS playlist_handle = reinterpret_cast<AIMP3SDK::HPLS>(playlist_id);

    int fileCount = 0;
    if (aimp2_manager) {
        fileCount = aimp2_manager->aimp2_playlist_manager_->AIMP_PLS_GetFilesCount(playlist_id);
    } else if (aimp3_manager) {
        fileCount = aimp3_manager->aimp3_playlist_manager_->StorageGetEntryCount(playlist_handle);
    } else if (aimp36_manager) {
        using namespace AIMP36SDK;
        IAIMPPlaylist_ptr playlist(aimp36_manager->getPlaylist(playlist_id));
        fileCount = playlist ? playlist->GetItemCount() : 0;
    }

    if (size == 0) {
        size = fileCount;
    }
    if (offset < 0) {
        offset = 0;
    }

    out << "{\"status\":";

    if (fileCount < 0) {
        out << "\"ERROR\"";
    } else {
        out << "\"OK\",\"songs\":[";

        const unsigned int entry_title_length = 256;
        std::wstring entry_title;
        if (aimp2_manager) {
            boost::intrusive_ptr<AIMP2SDK::IAIMP2PlaylistManager2> aimp_playlist_manager(aimp2_manager->aimp2_playlist_manager_);
            AIMP2SDK::AIMP2FileInfo fileInfo = {0};
            fileInfo.cbSizeOf = sizeof(fileInfo);
            for (int i = offset; (i < fileCount) && (i < offset + size); ++i) {
                aimp_playlist_manager->AIMP_PLS_Entry_InfoGet(playlist_id, i, &fileInfo);
                entry_title.resize(entry_title_length, 0);
                aimp_playlist_manager->AIMP_PLS_Entry_GetTitle( playlist_id, i, &entry_title[0], entry_title.length() );
                entry_title.resize( wcslen( entry_title.c_str() ) );
                using namespace Utilities;
                replaceAll(L"\"", 1,
                           L"\\\"", 2,
                           &entry_title);
                if (i != 0) {
                    out << ',';
                }
                out << "{\"name\":\"" << StringEncoding::utf16_to_utf8(entry_title) << "\",\"length\":" << fileInfo.nDuration << "}";
            }
        } else if (aimp3_manager) {
            using namespace AIMP3SDK;
            for (int i = offset; (i < fileCount) && (i < offset + size); ++i) {
                HPLSENTRY entry_handle = aimp3_manager->aimp3_playlist_manager_->StorageGetEntry(playlist_handle, i);
                entry_title.resize(entry_title_length, 0);
                aimp3_manager->aimp3_playlist_manager_->EntryPropertyGetValue( entry_handle, AIMP_PLAYLIST_ENTRY_PROPERTY_DISPLAYTEXT,
                                                                               &entry_title[0], entry_title.length()
                                                                              );
                TAIMPFileInfo info = {0};
                info.StructSize = sizeof(info);
                //info.Title = &entry_title[0]; // use EntryPropertyGetValue(...DISPLAYTEXT) since it returns string displayed in AIMP playlist.
                //info.TitleLength = entry_title.length();
                aimp3_manager->aimp3_playlist_manager_->EntryPropertyGetValue( entry_handle, AIMP_PLAYLIST_ENTRY_PROPERTY_INFO,
                                                                               &info, sizeof(info)
                                                                              );
                entry_title.resize( wcslen( entry_title.c_str() ) );
                using namespace Utilities;
                replaceAll(L"\"", 1,
                           L"\\\"", 2,
                           &entry_title);
                if (i != 0) {
                    out << ',';
                }
                out << "{\"name\":\"" << StringEncoding::utf16_to_utf8(entry_title) << "\",\"length\":" << info.Duration << "}";
            }
        } else if (aimp36_manager) {
            using namespace AIMP36SDK;
            using namespace Utilities;

            if (IAIMPPlaylist_ptr playlist = aimp36_manager->getPlaylist(playlist_id)) {
                for (int i = offset; (i < fileCount) && (i < offset + size); ++i) {
                    IAIMPPlaylistItem* item_tmp;
                    HRESULT r = playlist->GetItem(i,
                                                  IID_IAIMPPlaylistItem,
                                                  reinterpret_cast<void**>(&item_tmp)
                                                  );
                    if (S_OK != r) {
                        throw std::runtime_error(MakeString() << "playlist->GetItem(IID_IAIMPPlaylistItem) failed. Result " << r);
                    }
                    IAIMPPlaylistItem_ptr item(item_tmp, false); 
                    item_tmp = nullptr;

                    IAIMPString_ptr display_text;
                    r = Support::getString(item.get(), AIMP_PLAYLISTITEM_PROPID_DISPLAYTEXT, &display_text);
                    if (S_OK != r) {
                        throw std::runtime_error(MakeString() << "get prop(AIMP_PLAYLISTITEM_PROPID_DISPLAYTEXT) failed. Result " << r);
                    }

                    IAIMPFileInfo* file_info_tmp;
                    r = item->GetValueAsObject(AIMP_PLAYLISTITEM_PROPID_FILEINFO, IID_IAIMPFileInfo,
                                                       reinterpret_cast<void**>(&file_info_tmp)
                                                       );
                    if (S_OK != r) {
                        throw std::runtime_error( MakeString() << __FUNCTION__": item->GetValueAsObject(AIMP_PLAYLISTITEM_PROPID_FILEINFO) failed for track index " << i << ", playlist " << playlist_id << ". Result: " << r);
                    }
                    boost::intrusive_ptr<IAIMPFileInfo> file_info(file_info_tmp, false);

                    double duration_sec_tmp;
                    r = file_info->GetValueAsFloat(AIMP_FILEINFO_PROPID_DURATION, &duration_sec_tmp);
                    if (S_OK != r) {
                        throw std::runtime_error(MakeString() << "IAIMPFileInfo::GetValueAsFloat(AIMP_FILEINFO_PROPID_DURATION) failed. Result " << r);
                    }
                    const INT64 duration_ms = static_cast<INT64>(duration_sec_tmp * 1000.);

                    entry_title.assign( display_text->GetData(), display_text->GetLength() );
                    using namespace Utilities;
                    replaceAll(L"\"", 1,
                               L"\\\"", 2,
                               &entry_title);
                    if (i != 0) {
                        out << ',';
                    }
                    out << "{\"name\":\"" << StringEncoding::utf16_to_utf8(entry_title) << "\",\"length\":" << duration_ms << "}";
                }
            }
        }
        out << "]";
    }
    out << "}";

    // removing old cache
    for (it = playlist_cache.begin(); it != playlist_cache.end(); ) {
        if (tickCount - it->second.first > 60000 * 60) { // delete playlists which where accessed last time hour or more ago
            it = playlist_cache.erase(it);
        } else {
            ++it;
        }
    }

    const std::string out_str = out.str();
    const unsigned int hash = Utilities::crc32(out_str); // CRC32().GetHash(out.str().c_str())

    playlist_cache[cacheKey] = std::pair< int, std::pair<int, std::string> >(tickCount,
                                                                             std::pair<int, std::string>(hash, out_str)
                                                                             );

    if (return_crc) {
        out.str( std::string() );
        out << hash;
    }

    // not used, we have one thread. concurencyInstance.LeaveWriter();
}

void EmulationOfWebCtlPlugin::getPlayerStatus(std::ostringstream& out)
{
    out << "{\"status\": \"OK\", \"RepeatFile\": "
        << aimp_manager_.getStatus(AIMPManager::STATUS_REPEAT)
        << ", \"RandomFile\": "
        << aimp_manager_.getStatus(AIMPManager::STATUS_SHUFFLE)
        << "}";
}

void EmulationOfWebCtlPlugin::getCurrentSong(std::ostringstream& out)
{
    const TrackDescription track( aimp_manager_.getPlayingTrack() );
    std::wstring entry_title(256, 0);
    if ( AIMPPlayer::AIMPManager26* aimp2_manager = dynamic_cast<AIMPPlayer::AIMPManager26*>(&aimp_manager_) ) {
        aimp2_manager->aimp2_playlist_manager_->AIMP_PLS_Entry_GetTitle( track.playlist_id, track.track_id, 
                                                                         &entry_title[0], entry_title.length()
                                                                        );
        entry_title.resize( wcslen( entry_title.c_str() ) );
    } else if ( AIMPPlayer::AIMPManager30* aimp3_manager = dynamic_cast<AIMPPlayer::AIMPManager30*>(&aimp_manager_) ) {
        using namespace AIMP3SDK;
        HPLSENTRY entry_handle = aimp3_manager->aimp3_playlist_manager_->StorageGetEntry(cast<AIMP3SDK::HPLS>(track.playlist_id), track.track_id);
        //TAIMPFileInfo info = {0};
        //info.StructSize = sizeof(info);
        //info.Title = &entry_title[0];
        //info.TitleLength = entry_title.length();
        //aimp3_manager->aimp3_playlist_manager_->EntryPropertyGetValue( entry_id, AIMP_PLAYLIST_ENTRY_PROPERTY_INFO,
        //                                                               &info, sizeof(info)
        //                                                              );
        // use EntryPropertyGetValue(...DISPLAYTEXT) since it returns string displayed in AIMP playlist.
        aimp3_manager->aimp3_playlist_manager_->EntryPropertyGetValue( entry_handle, AIMP3SDK::AIMP_PLAYLIST_ENTRY_PROPERTY_DISPLAYTEXT,
                                                                       &entry_title[0], entry_title.length()
                                                                      );
        entry_title.resize( wcslen( entry_title.c_str() ) );
    } else if ( AIMPPlayer::AIMPManager36* aimp36_manager = dynamic_cast<AIMPPlayer::AIMPManager36*>(&aimp_manager_) ) {
        using namespace AIMP36SDK;
        using namespace Utilities;
        if (IAIMPPlaylistItem_ptr item = aimp36_manager->getPlaylistItem(track.track_id)) {
            IAIMPString_ptr display_text;
            HRESULT r = Support::getString(item.get(), AIMP_PLAYLISTITEM_PROPID_DISPLAYTEXT, &display_text);
            if (S_OK == r) {
                entry_title.assign(display_text->GetData(), display_text->GetLength()); 
            }
        }
    }

    using namespace Utilities;
    replaceAll(L"\"", 1,
                L"\\\"", 2,
                &entry_title);
    out << "{\"status\": \"OK\", \"PlayingList\": "
        << track.playlist_id
        << ", \"PlayingFile\": "
        << track.track_id
        << ", \"PlayingFileName\": \""
        << StringEncoding::utf16_to_utf8(entry_title)
        << "\", \"length\": "
        << aimp_manager_.getStatus(AIMPManager::STATUS_LENGTH)
        << "}";
}

void EmulationOfWebCtlPlugin::calcPlaylistCRC(int playlist_id)
{
    std::ostringstream out;
    getPlaylistSongs(playlist_id, true, true, 0/*params["offset"]*/, 0/*params["size"]*/, out);
}

void EmulationOfWebCtlPlugin::setPlayerStatus(const std::string& statusType, int value)
{
    if (statusType.compare("repeat") == 0) {
        aimp_manager_.setStatus(AIMPManager::STATUS_REPEAT, value);
    } else if (statusType.compare("shuffle") == 0) {
        aimp_manager_.setStatus(AIMPManager::STATUS_SHUFFLE, value);
    }
}

void EmulationOfWebCtlPlugin::sortPlaylist(int playlist_id, const std::string& sortType)
{
    if ( AIMPPlayer::AIMPManager26* aimp2_manager = dynamic_cast<AIMPPlayer::AIMPManager26*>(&aimp_manager_) ) {
        using namespace AIMP2SDK;
        boost::intrusive_ptr<AIMP2SDK::IAIMP2PlaylistManager2> aimp_playlist_manager(aimp2_manager->aimp2_playlist_manager_);
        if (sortType.compare("title") == 0) {
            aimp_playlist_manager->AIMP_PLS_Sort(playlist_id, AIMP_PLS_SORT_TYPE_TITLE);
        } else if (sortType.compare("filename") == 0) {
            aimp_playlist_manager->AIMP_PLS_Sort(playlist_id, AIMP_PLS_SORT_TYPE_FILENAME);
        } else if (sortType.compare("duration") == 0) {
            aimp_playlist_manager->AIMP_PLS_Sort(playlist_id, AIMP_PLS_SORT_TYPE_DURATION);
        } else if (sortType.compare("artist") == 0) {
            aimp_playlist_manager->AIMP_PLS_Sort(playlist_id, AIMP_PLS_SORT_TYPE_ARTIST);
        } else if (sortType.compare("inverse") == 0) {
            aimp_playlist_manager->AIMP_PLS_Sort(playlist_id, AIMP_PLS_SORT_TYPE_INVERSE);
        } else if (sortType.compare("randomize") == 0) {
            aimp_playlist_manager->AIMP_PLS_Sort(playlist_id, AIMP_PLS_SORT_TYPE_RANDOMIZE);
        }
    } else if ( AIMPPlayer::AIMPManager30* aimp3_manager = dynamic_cast<AIMPPlayer::AIMPManager30*>(&aimp_manager_) ) {
        using namespace AIMP3SDK;
        boost::intrusive_ptr<AIMP3SDK::IAIMPAddonsPlaylistManager> aimp_playlist_manager(aimp3_manager->aimp3_playlist_manager_);
        const AIMP3SDK::HPLS playlist_handle = reinterpret_cast<AIMP3SDK::HPLS>(playlist_id);
        if (sortType.compare("title") == 0) {
            aimp_playlist_manager->StorageSort(playlist_handle, AIMP_PLAYLIST_SORT_TYPE_TITLE);
        } else if (sortType.compare("filename") == 0) {
            aimp_playlist_manager->StorageSort(playlist_handle, AIMP_PLAYLIST_SORT_TYPE_FILENAME);
        } else if (sortType.compare("duration") == 0) {
            aimp_playlist_manager->StorageSort(playlist_handle, AIMP_PLAYLIST_SORT_TYPE_DURATION);
        } else if (sortType.compare("artist") == 0) {
            aimp_playlist_manager->StorageSort(playlist_handle, AIMP_PLAYLIST_SORT_TYPE_ARTIST);
        } else if (sortType.compare("inverse") == 0) {
            aimp_playlist_manager->StorageSort(playlist_handle, AIMP_PLAYLIST_SORT_TYPE_INVERSE);
        } else if (sortType.compare("randomize") == 0) {
            aimp_playlist_manager->StorageSort(playlist_handle, AIMP_PLAYLIST_SORT_TYPE_RANDOMIZE);
        }
    } else if ( AIMPPlayer::AIMPManager36* aimp36_manager = dynamic_cast<AIMPPlayer::AIMPManager36*>(&aimp_manager_) ) {
        using namespace AIMP36SDK;
        if (IAIMPPlaylist_ptr playlist = aimp36_manager->getPlaylist(playlist_id)) {
            if (sortType.compare("title") == 0) {
                playlist->Sort(AIMP_PLAYLIST_SORTMODE_TITLE);
            } else if (sortType.compare("filename") == 0) {
                playlist->Sort(AIMP_PLAYLIST_SORTMODE_FILENAME);
            } else if (sortType.compare("duration") == 0) {
                playlist->Sort(AIMP_PLAYLIST_SORTMODE_DURATION);
            } else if (sortType.compare("artist") == 0) {
                playlist->Sort(AIMP_PLAYLIST_SORTMODE_ARTIST);
            } else if (sortType.compare("inverse") == 0) {
                playlist->Sort(AIMP_PLAYLIST_SORTMODE_INVERSE);
            } else if (sortType.compare("randomize") == 0) {
                playlist->Sort(AIMP_PLAYLIST_SORTMODE_RANDOMIZE);
            }
        }
    }
}

void EmulationOfWebCtlPlugin::addFile(int playlist_id, const std::string& filename_url)
{
    if ( AIMPPlayer::AIMPManager26* aimp2_manager = dynamic_cast<AIMPPlayer::AIMPManager26*>(&aimp_manager_) ) {
        AIMP2SDK::IPLSStrings* strings;
        aimp2_manager->aimp2_controller_->AIMP_NewStrings(&strings);
        const std::wstring filename = StringEncoding::utf8_to_utf16( WebCtl::urldecode(filename_url) );
        strings->AddFile(const_cast<PWCHAR>( filename.c_str() ), nullptr);
        aimp2_manager->aimp2_controller_->AIMP_PLS_AddFiles(playlist_id, strings);
        strings->Release();
    } else if ( AIMPPlayer::AIMPManager30* aimp3_manager = dynamic_cast<AIMPPlayer::AIMPManager30*>(&aimp_manager_) ) {
        using namespace AIMP3SDK;
        AIMP3SDK::HPLS playlist_handle = reinterpret_cast<AIMP3SDK::HPLS>(playlist_id);

        boost::intrusive_ptr<AIMP3SDK::IAIMPAddonsPlaylistStrings> strings( new TAIMPAddonsPlaylistStrings() );
        const std::wstring filename = StringEncoding::utf8_to_utf16( WebCtl::urldecode(filename_url) );
        strings->ItemAdd(const_cast<PWCHAR>( filename.c_str() ), nullptr);
        aimp3_manager->aimp3_playlist_manager_->StorageAddEntries( playlist_handle, strings.get() );
    } else if ( AIMPPlayer::AIMPManager36* aimp36_manager = dynamic_cast<AIMPPlayer::AIMPManager36*>(&aimp_manager_) ) {
        using namespace AIMP36SDK;
        if (IAIMPPlaylist_ptr playlist = aimp36_manager->getPlaylist(playlist_id)) {
            std::wstring filename = StringEncoding::utf8_to_utf16( WebCtl::urldecode(filename_url) );
            AIMPString filename_string(&filename, true);
            filename_string.AddRef(); // prevent destruction by AIMP.
            DWORD flags = 0;
            int insert_in = -1; // at the end of playlist.
            playlist->Add(&filename_string, flags, insert_in);
        }
    }
}

Rpc::ResponseType EmulationOfWebCtlPlugin::execute(const Rpc::Value& root_request, Rpc::Value& root_response)
{
    const Rpc::Value& params = root_request["params"];
    try {
        if (params.type() != Rpc::Value::TYPE_OBJECT || params.size() == 0) {
            throw std::runtime_error("arguments are missing");
        }
        const METHOD_ID* method_id = getMethodID(params["action"]);
        if (method_id == nullptr) {
            std::ostringstream msg;
            msg << "Method " << params["action"] << " not found";
            throw std::runtime_error( msg.str() );
        }

        std::ostringstream out(std::ios::in | std::ios::out | std::ios::binary);

        AIMPPlayer::AIMPManager26* aimp2_manager = dynamic_cast<AIMPPlayer::AIMPManager26*>(&aimp_manager_);
        AIMPPlayer::AIMPManager30* aimp3_manager = dynamic_cast<AIMPPlayer::AIMPManager30*>(&aimp_manager_);
        AIMPPlayer::AIMPManager36* aimp36_manager = dynamic_cast<AIMPPlayer::AIMPManager36*>(&aimp_manager_);

        switch (*method_id) {
        case get_playlist_list:
            getPlaylistList(out);
            break;
        case get_version_string:
            out << WebCtl::kVERSION;
            break;
        case get_version_number:
            out << WebCtl::kVERSION_INT;
            break;
        case get_update_time:
            out << WebCtl::update_time;
            break;
        case get_playlist_songs:
            {
            const int offset = params.isMember("offset") ? params["offset"] : 0;
            const int size = params.isMember("size") ? params["size"] : 0;
            getPlaylistSongs(params["id"], false, false, offset, size, out);
            }
            break;
        case get_playlist_crc:
            getPlaylistSongs(params["id"], false, true, 0/*params["offset"]*/, 0/*params["size"]*/, out);
            break;
        case get_player_status:
            getPlayerStatus(out);
            break;
        case get_song_current:
            getCurrentSong(out);
            break;
        case get_volume:
            out << aimp_manager_.getStatus(AIMPManager::STATUS_VOLUME);
            break;
        case set_volume:
            aimp_manager_.setStatus(AIMPManager::STATUS_VOLUME, params["volume"]);
            break;
        case get_track_position:
            out << "{\"position\":" << aimp_manager_.getStatus(AIMPManager::STATUS_POS) << ",\"length\":" << aimp_manager_.getStatus(AIMPManager::STATUS_LENGTH) << "}";
            break;
        case set_track_position:
            aimp_manager_.setStatus(AIMPManager::STATUS_POS, params["position"]);
            break;
        case get_track_length:
            out << aimp_manager_.getStatus(AIMPManager::STATUS_LENGTH);
            break;
        case get_custom_status:
            // For convenience use AIMPManager::getStatus even on AIMP2.
            //out << aimp2_manager->aimp2_controller_->AIMP_Status_Get( static_cast<int>(params["status"]) ); // use native status getter since web ctl provides access to all statuses.
            out << aimp_manager_.getStatus( cast<AIMPManager::STATUS>( static_cast<int>(params["status"]) ) );
            break;
        case set_custom_status:
            // For convenience use AIMPManager::setStatus even on AIMP2.
            //aimp2_manager->aimp2_controller_->AIMP_Status_Set( static_cast<int>(params["status"]),
            //                                                    static_cast<int>(params["value"])
            //                                                    ); // use native status setter since web ctl provides access to all statuses.
            aimp_manager_.setStatus( cast<AIMPManager::STATUS>( static_cast<int>(params["status"]) ),
                                     static_cast<int>(params["value"])
                                    );
            break;
        case set_song_play:
            {
            const int playlist_id = params["playlist"],
                      track_index = params["song"];
            if (aimp2_manager) {
                const TrackDescription track_desc(playlist_id, track_index);
                aimp_manager_.startPlayback(track_desc);
            } else if (aimp3_manager) {
                using namespace AIMP3SDK;
                HPLSENTRY entry_handle = aimp3_manager->aimp3_playlist_manager_->StorageGetEntry(reinterpret_cast<AIMP3SDK::HPLS>(playlist_id),
                                                                                                 track_index
                                                                                                 );
                aimp3_manager->aimp3_player_manager_->PlayEntry(entry_handle);
            } else if (aimp36_manager) {
                using namespace AIMP36SDK;
                if (IAIMPPlaylist_ptr playlist = aimp36_manager->getPlaylist(playlist_id)) {
                    IAIMPPlaylistItem* item_tmp;
                    HRESULT r = playlist->GetItem(track_index,
                                                  IID_IAIMPPlaylistItem,
                                                  reinterpret_cast<void**>(&item_tmp)
                                                  );
                    if (S_OK == r) {
                        IAIMPPlaylistItem_ptr item(item_tmp, false);
                        aimp36_manager->aimp_service_player_->Play2(item.get());
                    }
                }
            }

            }
            break;
        case set_song_position:
            {
            const int playlist_id = params["playlist"],
                      track_index = params["song"];
            int position = params["position"];
            if (aimp2_manager) {
                aimp2_manager->aimp2_playlist_manager_->AIMP_PLS_Entry_SetPosition(playlist_id, track_index, position);
            } else if (aimp3_manager) {
                using namespace AIMP3SDK;
                HPLSENTRY entry_handle = aimp3_manager->aimp3_playlist_manager_->StorageGetEntry(reinterpret_cast<AIMP3SDK::HPLS>(playlist_id),
                                                                                                 track_index
                                                                                                 );
                aimp3_manager->aimp3_playlist_manager_->EntryPropertySetValue( entry_handle, AIMP_PLAYLIST_ENTRY_PROPERTY_INDEX, &position, sizeof(position) );
            } else if (aimp36_manager) {
                using namespace AIMP36SDK;
                if (IAIMPPlaylist_ptr playlist = aimp36_manager->getPlaylist(playlist_id)) {
                    IAIMPPlaylistItem* item_tmp;
                    HRESULT r = playlist->GetItem(track_index,
                                                  IID_IAIMPPlaylistItem,
                                                  reinterpret_cast<void**>(&item_tmp)
                                                  );
                    if (S_OK == r) {
                        IAIMPPlaylistItem_ptr item(item_tmp, false);

                        item->SetValueAsInt32(AIMP_PLAYLISTITEM_PROPID_INDEX, position);
                    }
                }
            }

            calcPlaylistCRC(playlist_id);
            }
            break;
        case set_player_status:
            setPlayerStatus(params["statusType"], params["value"]);
            break;
        case player_play:
            aimp_manager_.startPlayback();
            break;
        case player_pause:
            aimp_manager_.pausePlayback();
            break;
        case player_stop:
            aimp_manager_.stopPlayback();
            break;
        case player_prevous:
            aimp_manager_.playPreviousTrack();
            break;
        case player_next:
            aimp_manager_.playNextTrack();
            break;
        case playlist_sort:
            {
            const int playlist_id = params["playlist"];
            sortPlaylist(playlist_id, params["sort"]);
            calcPlaylistCRC(playlist_id);
            }
            break;
        case playlist_add_file:
            {
            const int playlist_id = params["playlist"];
            addFile(playlist_id, params["file"]);
            calcPlaylistCRC(playlist_id);
            }
            break;
        case playlist_del_file:
            {
            const int playlist_id = params["playlist"],
                      track_index    = params["file"];
            if (aimp2_manager) {
                aimp2_manager->aimp2_playlist_manager_->AIMP_PLS_Entry_Delete(playlist_id, track_index);
            } else if (aimp3_manager) {
                using namespace AIMP3SDK;
                HPLSENTRY entry_handle = aimp3_manager->aimp3_playlist_manager_->StorageGetEntry(reinterpret_cast<AIMP3SDK::HPLS>(playlist_id),
                                                                                                 track_index
                                                                                                 );
                aimp3_manager->aimp3_playlist_manager_->EntryDelete(entry_handle);
            } else if (aimp36_manager) {
                using namespace AIMP36SDK;
                if (IAIMPPlaylist_ptr playlist = aimp36_manager->getPlaylist(playlist_id)) {
                    playlist->Delete2(track_index);
                }
            }
            calcPlaylistCRC(playlist_id);
            }
            break;
        case playlist_queue_add:
            {
            const bool insert_at_queue_beginning = false;
            const TrackDescription track_desc(params["playlist"], params["song"]);
            aimp_manager_.enqueueEntryForPlay(track_desc, insert_at_queue_beginning);
            }
            break;
        case playlist_queue_remove:
            {
            const TrackDescription track_desc(params["playlist"], params["song"]);
            aimp_manager_.removeEntryFromPlayQueue(track_desc);
            }
            break;
        case download_song:
            {
            const TrackDescription track_desc(params["playlist"], params["song"]);
            // prepare URI for DownloadTrack::RequestHandler.
            out << "/downloadTrack/playlist_id/" << track_desc.playlist_id 
                << "/track_id/" << track_desc.track_id;
            }
            break;
        default:
            break;
        }

        root_response["result"] = out.str(); // result is simple string.
    } catch (std::runtime_error& e) { // catch all exceptions of aimp manager here.
        BOOST_LOG_SEV(logger(), error) << "Error in "__FUNCTION__". Reason: " << e.what();

        root_response["result"] = "";
    }
    return Rpc::RESPONSE_IMMEDIATE;
}

} // namespace AimpRpcMethods
