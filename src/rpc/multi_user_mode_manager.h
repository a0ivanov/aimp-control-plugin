// Copyright (c) 2011, Alexey Ivanov

#ifndef MULTI_USER_MODE_MANAGER_H
#define MULTI_USER_MODE_MANAGER_H

#include "aimp/manager.h"
#include "utils/string_encoding.h"
#include <vector>
#include <list>
#include <set>

namespace MultiUserMode
{

//! Represent user's virtual playlist.
class VirtualPlaylist
{
public:
    VirtualPlaylist(AIMPPlayer::PlaylistID id, const std::wstring& title)
        :
        id_(id),
        title_(title)
    {}

    //! Add track to playlist.
    void addTrack(AIMPPlayer::TrackDescription track_desc);

    ///! Remove track from playlist.
    void removeTrack(AIMPPlayer::TrackDescription track_desc);

    //! Get title of playlist.
    const std::wstring& getTitle() const { return title_; }

    //! Set title of playlist.
    void setTitle(const std::wstring& title) { title_ = title; }

    //! Get ID of playlist.
    AIMPPlayer::PlaylistID getID() const { return id_; }

    //! Get count of tracks in playlist.
    DWORD getEntriesCount() const { return tracks_.size(); }

    //! Return true if playlist does not contain tracks.
    bool empty() const { return tracks_.empty(); }

private:

    std::list<AIMPPlayer::TrackDescription> tracks_; //!< tracks descriptors list.
    AIMPPlayer::PlaylistID id_; //!< playlist ID.
    std::wstring title_; //!< playlist title.
};

typedef std::string UserIpAddressType;

class User
{
public:
    User(const UserIpAddressType& ip_address, const std::wstring& nickname = L"")
        :
        ip_address_(ip_address),
        nickname_(nickname),
        playlist_(0, // id of user virtual playlist always zero.
                    nickname_.empty() ? StringEncoding::system_ansi_encoding_to_utf16_safe("Playlist of " + ip_address_)
                                      : nickname_ )
    {}

    const UserIpAddressType& getIpAddress() const { return ip_address_; }

    const VirtualPlaylist& getPlaylist() const { return playlist_; }

private:

    friend bool operator<(const User&, const User&);

    UserIpAddressType ip_address_;
    std::wstring nickname_;
    VirtualPlaylist playlist_;
};

bool operator<(const User& left, const User& right);


struct CommonPlaylistElement
{

};

class MultiUserModeManager
{
public:
    MultiUserModeManager(AIMPPlayer::AIMPManager& aimp_manager)
        :
        aimp_manager_(aimp_manager)
    {}

    //! Determine if multi user mode active.
    /*!
        \return true if multi user mode is active, otherwise return false.
    */
    bool multiUserModeActive() const;

    void checkMethodAvailability(const std::string& method_name) const; // throws XmlRpc::XmlRpcException

    //! Create new user from IP, add it in list and return reference to User object.
    /*!
        If user with specified IP already exists, reference to it is returned.
    */
    User& addUser(const UserIpAddressType& ip_address);

    //! Remove user from user list by IP address.
    void removeUser(const UserIpAddressType& ip_address);

    //! Determines if user has full access to controls which change player state.
    /*!
        Following conditions are true:
            - user's track is active.
            - there is no tracks in common virtual playlist. All users have full access in this case.
    */
    bool userHasFullAccessToChangePlayerState(const User& user) const;


private:

    //! \return user who's track is active.
    User& getTopUser(); // throws std::logic_error
    const User& getTopUser() const; // throws std::logic_error

    //! Return index of next user in queue.
    std::size_t getIndexOfNextUserInQueue(std::size_t current_user) const;

    AIMPPlayer::AIMPManager& aimp_manager_;
    std::set<std::string> forbidden_methods_;

    typedef std::map<UserIpAddressType, User> UserList;
    UserList users_; //!< users list with fast access by IP address.

    typedef std::vector<UserIpAddressType> UserQueue;
    UserQueue users_ordered_by_connection_moment_; //!< user's IP addresses ordered by IP address.
    std::size_t top_user_index_; //!< index of user whose track is active.

};

} // namespace MultiUserMode

#endif // #ifndef MULTI_USER_MODE_MANAGER_H
