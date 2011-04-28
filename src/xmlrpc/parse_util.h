//
// XmlRpc++ Copyright (c) 2002-2003 by Chris Morley
//

#ifndef _XMLRPCUTIL_H_
#define _XMLRPCUTIL_H_

#include <string>

#if defined(_MSC_VER)
# define snprintf    _snprintf_s
# define vsnprintf   _vsnprintf_s
# define strcasecmp  _stricmp_s
# define strncasecmp _strnicmp_s
#elif defined(__BORLANDC__)
# define strcasecmp  stricmp
# define strncasecmp strnicmp
#endif

namespace XmlRpc
{

//! Version identifier
extern const char XMLRPC_VERSION[];

//! Utilities for XML parsing, encoding, and decoding and message handlers.
namespace Util
{

// hokey xml parsing
//! \return contents between <tag> and </tag>, updates offset to char after </tag>
std::string parseTag(const char* tag, std::string const& xml, std::size_t* offset);

//! \return true if the tag is found and updates offset to the char after the tag
bool findTag(const char* tag, std::string const& xml, std::size_t* offset);

//! \return the next tag and updates offset to the char after the tag, or empty string
//! if the next non-whitespace character is not '<'
std::string getNextTag(std::string const& xml, std::size_t* offset);

//! \return true if the tag is found at the specified offset (modulo any whitespace)
//! and updates offset to the char after the tag
bool nextTagIs(const char* tag, std::string const& xml, std::size_t* offset);

//! Convert raw text to encoded xml.
std::string xmlEncode(const std::string& raw);

//! Convert encoded xml to raw text
std::string xmlDecode(const std::string& encoded);

} // namespace Util
} // namespace XmlRpc

#endif // _XMLRPCUTIL_H_
