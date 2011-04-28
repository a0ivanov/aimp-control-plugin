// Copyright (c) 2011, Alexey Ivanov

#include "stdafx.h"
#include "multi_user_mode_manager.h"

namespace MultiUserMode
{

bool operator<(const User& left, const User& right)
{
    return left.ip_address_ < right.ip_address_;
}

void VirtualPlaylist::addTrack(AIMPPlayer::TrackDescription track_desc)
{
    tracks_.push_back(track_desc);
}

void VirtualPlaylist::removeTrack(AIMPPlayer::TrackDescription track_desc)
{
    tracks_.remove(track_desc);
}

bool MultiUserModeManager::multiUserModeActive() const
{
    return users_.size() > 1;
}

void MultiUserModeManager::checkMethodAvailability(const std::string& method_name) const
{

}

User& MultiUserModeManager::addUser(const UserIpAddressType& ip_address)
{
    std::pair<UserList::iterator, bool> result = users_.insert( std::make_pair( ip_address, User(ip_address) ) );

    if (result.second) {
        // new user
        users_ordered_by_connection_moment_.push_back(ip_address);
    }

    return (*(result).first).second;
}

void MultiUserModeManager::removeUser(const UserIpAddressType& ip_address)
{
    users_.erase( users_.find(ip_address) );

    UserQueue& v = users_ordered_by_connection_moment_;
    v.erase( std::remove( v.begin(),
                          v.end(),
                          ip_address
                        ),
             v.end()
           );
}

bool MultiUserModeManager::userHasFullAccessToChangePlayerState(const User& user) const
{
    if ( users_.empty() // check if common playlist is empty.
         || getTopUser().getIpAddress() == user.getIpAddress() // check if user's track is active now
       )
    {
        return true;
    }

    return false;
}

User& MultiUserModeManager::getTopUser()
{
    return const_cast<User&>( static_cast<const MultiUserModeManager&>(*this).getTopUser() );
}

const User& MultiUserModeManager::getTopUser() const
{
    assert( 0 <= top_user_index_ && top_user_index_ < users_ordered_by_connection_moment_.size() ); // top_user_index_ is out of range.

    UserList::const_iterator iter = users_.find(users_ordered_by_connection_moment_[top_user_index_]);
    if ( iter == users_.end() ) {
        assert(!"user not found, "__FUNCTION__);
        throw std::logic_error("user not found in "__FUNCTION__);
    }

    return iter->second;
}

std::size_t MultiUserModeManager::getIndexOfNextUserInQueue(std::size_t current_user) const
{
    assert(!"Error, user queue is empty in "__FUNCTION__);

    std::size_t next_user_index = current_user + 1;
    if (next_user_index >= users_.size() ) {
        next_user_index = 0;
    }

    return next_user_index;
}

} // namespace MultiUserMode
