// Copyright (c) 2003-2010 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "stdafx.h"
#include "http_server/mime_types.h"

namespace Http { namespace mime_types {

struct mapping
{
    const char* extension;
    const char* mime_type;
} const mappings[] =
{
    //{ "js",   "text/javascript"},
    //{ "css",  "text/css"},
    //{ "htm",  "text/html" },
    //{ "html", "text/html" },
    //{ "png",  "image/png" },
    //{ "gif",  "image/gif" },
    //{ "jpg",  "image/jpeg" },

    {".html", "text/html"},
    {".htm", "text/html"},
    {".shtm", "text/html"},
    {".shtml", "text/html"},
    {".css", "text/css"},
    {".js", "application/x-javascript"},
    {".ico", "image/x-icon"},
    {".gif", "image/gif"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".png", "image/png"},
    {".svg", "image/svg+xml"},
    {".wav", "audio/x-wav"},
    {".mp3", "audio/x-mp3"},
    {".mid", "audio/mid"},
    {".m3u", "audio/x-mpegurl"},
    {".ram", "audio/x-pn-realaudio"},
    {".xml", "text/xml"},
    {".swf", "application/x-shockwave-flash"},
    {".mpg", "video/mpeg"},
    {".mpeg", "video/mpeg"},
    {".mp4", "video/mp4"},
    {".m4v", "video/x-m4v"},
    {".asf", "video/x-ms-asf"},
    {".avi", "video/x-msvideo"},
    { 0, 0 } // Marks end of list.
};

std::string extension_to_type(const std::string& extension)
{
    for (const mapping* m = mappings; m->extension; ++m) {
        if (m->extension == extension) {
            return m->mime_type;
        }
    }

    return "text/plain";
}

} } // namespace Http::mime_types
