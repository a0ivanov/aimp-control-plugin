// Copyright (c) 2013, Alexey Ivanov

#include "stdafx.h"
#include "aimp/playlist.h"
#include "aimp/common_types.h"
#include "plugin/logger.h"
#include "utils/string_encoding.h"
#include <boost/crc.hpp>
#include "utils/util.h"

namespace {
using namespace ControlPlugin::PluginLogger;
ModuleLoggerType& logger()
    { return getLogManager().getModuleLogger<AIMPPlayer::AIMPManager>(); }
}

namespace AIMPPlayer
{

Playlist::Playlist()
    :
    crc32_total_(0),
    crc32_properties_(0),
    crc32_entries_(0)
{}

Playlist::Playlist( const WCHAR* title,
                    DWORD file_count,
                    INT64 duration,
                    INT64 size_of_all_entries_in_bytes,
                    PlaylistID id
                   )
    :
    title_(title),
    file_count_(file_count),
    duration_(duration),
    size_of_all_entries_in_bytes_(size_of_all_entries_in_bytes),
    id_(id),
    crc32_total_(0),
    crc32_properties_(0),
    crc32_entries_(0)
{
}

Playlist::Playlist(Playlist&& rhs)
    :
    title_( std::move(rhs.title_) ),
    file_count_(rhs.file_count_),
    duration_(rhs.duration_),
    size_of_all_entries_in_bytes_(rhs.size_of_all_entries_in_bytes_),
    id_(rhs.id_),
    crc32_total_(rhs.crc32_total_),
    crc32_properties_(rhs.crc32_properties_),
    crc32_entries_(rhs.crc32_entries_)
{}

Playlist& Playlist::operator=(Playlist&& rhs)
{
    Playlist tmp( std::move(rhs) );
    swap(tmp);
    return *this;
}

void Playlist::swap(Playlist& rhs)
{
    using std::swap;
    swap(title_, rhs.title_);
    swap(file_count_, rhs.file_count_);
    swap(duration_, rhs.duration_);
    swap(size_of_all_entries_in_bytes_, rhs.size_of_all_entries_in_bytes_);
    swap(id_, rhs.id_);
    swap(crc32_total_, rhs.crc32_total_);
    swap(crc32_properties_, rhs.crc32_properties_);
    swap(crc32_entries_, rhs.crc32_entries_);
}

crc32_t Playlist::calc_crc32_properties() const
{
    const crc32_t members_crc32_list [] = {
        Utilities::crc32( title() ),
        Utilities::crc32( entriesCount() ),
        Utilities::crc32( duration() ),
        Utilities::crc32( sizeOfAllEntriesInBytes() )
    };

    return Utilities::crc32( &members_crc32_list[0], sizeof(members_crc32_list) );
}

crc32_t Playlist::calc_crc32_entries() const
{
    boost::crc_32_type crc32_calculator;

    ///!!! TODO: implement DB crc32 calc
    //for(const AIMPPlayer::PlaylistEntry& entry : entries()) {
    //    const crc32_t crc = entry.crc32();
    //    crc32_calculator.process_bytes( &crc, sizeof(crc) );
    //}
    return crc32_calculator.checksum();
}

crc32_t Playlist::crc32() const
{
    if (crc32_properties_ == 0) {
        crc32_properties_ = calc_crc32_properties();
        crc32_total_ = 0;
    }

    if (crc32_entries_ == 0) {
        crc32_entries_ = calc_crc32_entries();
        crc32_total_ = 0;
    }

    if (crc32_total_ == 0) {
        // if crc32 was not passed as parameter, calc it here
        const crc32_t crc32_list[] = {
            crc32_properties_,
            crc32_entries_
        };

        crc32_total_ = Utilities::crc32( &crc32_list[0], sizeof(crc32_list) );
    }
    return crc32_total_;
}

} // namespace AIMPPlayer
