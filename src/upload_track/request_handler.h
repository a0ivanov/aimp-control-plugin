// Copyright (c) 2013, Alexey Ivanov

#pragma once

#include <boost/filesystem.hpp>

namespace AIMPPlayer { class AIMPManager; }
namespace Http {
    struct Request; 
    struct Reply;
}

namespace UploadTrack
{

class RequestHandler : boost::noncopyable
{
public:
    RequestHandler(AIMPPlayer::AIMPManager& aimp_manager, boost::filesystem::wpath temp_dir)
        :
        aimp_manager_(aimp_manager),
        temp_dir_(temp_dir)
    {}

    bool handle_request(const Http::Request& req, Http::Reply& rep); // throws std::exception.

private:

    AIMPPlayer::AIMPManager& aimp_manager_;
    boost::filesystem::wpath temp_dir_;
};

} // namespace UploadTrack
