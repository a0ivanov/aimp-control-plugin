// Copyright (c) 2012, Alexey Ivanov

#include "stdafx.h"
#include "aimp/playlist.h"
#include "aimp/common_types.h"
#include "plugin/logger.h"
#include "utils/string_encoding.h"
#include <boost/foreach.hpp>

namespace {
using namespace ControlPlugin::PluginLogger;
ModuleLoggerType& logger()
    { return getLogManager().getModuleLogger<AIMPPlayer::AIMPManager>(); }
}

namespace AIMPPlayer
{

Playlist::Playlist()
    : 
    entries_( boost::make_shared<EntriesListType>() )
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
    entries_( boost::make_shared<EntriesListType>() )
{
}

Playlist::Playlist(Playlist&& rhs)
    :
    title_( std::move(rhs.title_) ),
    file_count_(rhs.file_count_),
    duration_(rhs.duration_),
    size_of_all_entries_in_bytes_(rhs.size_of_all_entries_in_bytes_),
    id_(rhs.id_),
    entries_( std::move(rhs.entries_) )
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
    swap(entries_, rhs.entries_);
}

const EntriesListType& Playlist::entries() const
{
    assert(entries_);
    return *entries_;
}

EntriesListType& Playlist::entries()
{
    // every time when entry list is changed we need to perform reset to be sure it does not use old cache.
    assert(entries_);
    return *entries_;
}

} // namespace AIMPPlayer
