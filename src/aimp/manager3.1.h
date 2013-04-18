// Copyright (c) 2013, Alexey Ivanov

#pragma once

#include "manager3.0.h"

namespace AIMPPlayer
{

/*!
    \brief Extends AIMP3Manager30 with new functionality introduced in AIMP 3.1.
*/
class AIMPManager31 : public AIMPManager30
{
public:

    /*!
        \param aimp3_core_unit - pointer to IAIMPCoreUnit object.
    */
    AIMPManager31(boost::intrusive_ptr<AIMP3SDK::IAIMPCoreUnit> aimp3_core_unit); // throws std::runtime_error

    virtual ~AIMPManager31();

    virtual void enqueueEntryForPlay(TrackDescription track_desc, bool insert_at_queue_beginning); // throws std::runtime_error

    virtual void removeEntryFromPlayQueue(TrackDescription track_desc); // throws std::runtime_error

    /*!
        Check availability with isPlaylistQueueSupported() method. Supported since AIMP 3.1.
    */
    void reloadQueuedEntries(); // throws std::runtime_error

    /*!
        Check availability with isPlaylistQueueSupported() method. Supported since AIMP 3.1.
        Track should be already queued.
    */
    void moveQueueEntry(TrackDescription track_desc, int new_queue_index); // throws std::runtime_error

    /*!
        Check availability with isPlaylistQueueSupported() method. Supported since AIMP 3.1.
    */
    void moveQueueEntry(int old_queue_index, int new_queue_index); // throws std::runtime_error

private:

    void initializeAIMPObjects(); // throws std::runtime_error
    void initPlaylistDB(); // throws std::runtime_error

    TrackDescription getTrackDescOfQueuedEntry(AIMP3SDK::HPLSENTRY entry_handle) const; // throws std::runtime_error;
    void deleteQueuedEntriesFromPlaylistDB();

    boost::intrusive_ptr<AIMP3SDK::IAIMPAddonsPlaylistQueue> aimp3_playlist_queue_;
};

} // namespace AIMPPlayer
