// Copyright (c) 2003-2010 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <string>
#include <vector>
#include "http_server/header.h"
#include "http_server/mpfd_parser/Parser.h"

namespace Http {

/// A request received from a client.
struct Request
{
    /// The request method, e.g. "GET", "POST".
    std::string method;

    /// The requested URI, such as a path to a file.
    std::string uri;

    /// Major version number, usually 1.
    int http_version_major;

    /// Minor version number, usually 0 or 1.
    int http_version_minor;

    /// The headers included with the request.
    std::vector<header> headers;

    /// The optional content sent with the request.
    std::string content;

    /// The optional multipart form data content sent with the request.
    /// Will contain all data.
    typedef MPFD::Parser mpfd_parser_t;
    mpfd_parser_t mpfd_parser;
};

} // namespace Http

#endif // HTTP_REQUEST_H
