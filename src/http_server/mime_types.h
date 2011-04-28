// Copyright (c) 2003-2010 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef HTTP_MIME_TYPES_H
#define HTTP_MIME_TYPES_H

#include <string>

namespace Http { namespace mime_types {

// Convert a file extension into a MIME type.
std::string extension_to_type(const std::string& extension);

} } // namespace Http::mime_types

#endif // HTTP_MIME_TYPES_H
