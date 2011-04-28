#include "stdafx.h"
#include "xmlrpc/parse_util.h"

#ifndef MAKEDEPEND
# include <ctype.h>
# include <iostream>
# include <stdarg.h>
# include <stdio.h>
# include <string>
#endif

namespace XmlRpc
{

// Version id
const char XMLRPC_VERSION[] = "XMLRPC++ 0.7";

namespace Util
{

// \return contents between <tag> and </tag>, updates offset to char after </tag>
std::string parseTag(const char* tag, std::string const& xml, std::size_t* offset)
{
    if ( *offset >= xml.length() ) {
        return std::string();
    }
    size_t istart = xml.find(tag, *offset);
    if (istart == std::string::npos) {
        return std::string();
    }
    istart += strlen(tag);
    std::string etag = "</";
    etag += tag + 1;
    size_t iend = xml.find(etag, istart);
    if (iend == std::string::npos) {
        return std::string();
    }
    *offset = iend + etag.length();

    return xml.substr(istart, iend - istart);
}


// \return true if the tag is found and updates offset to the char after the tag
bool findTag(const char* tag, std::string const& xml, std::size_t* offset)
{
    if ( *offset >= xml.length() ) {
        return false;
    }
    size_t istart = xml.find(tag, *offset);
    if (istart == std::string::npos) {
        return false;
    }
    *offset = istart + strlen(tag);

    return true;
}


// \return true if the tag is found at the specified offset (modulo any whitespace)
// and updates offset to the char after the tag
bool nextTagIs(const char* tag, std::string const& xml, std::size_t* offset)
{
    if (*offset >= xml.length() ) {
        return false;
    }

    const char* cp = xml.c_str() + *offset;
    std::size_t nc = 0;
    while ( *cp && isspace(*cp) ) {
        ++cp;
        ++nc;
    }

    std::size_t len = strlen(tag);
    if ( *cp && (strncmp(cp, tag, len) == 0) ) {
        *offset += nc + len;
        return true;
    }

    return false;
}

// \return the next tag and updates offset to the char after the tag, or empty string
// if the next non-whitespace character is not '<'
std::string getNextTag(std::string const& xml, std::size_t* offset)
{
    if (*offset >= xml.length() ) {
        return std::string();
    }

    size_t pos = *offset;
    const char* cp = xml.c_str() + pos;
    while ( *cp && isspace(*cp) ) {
        ++cp;
        ++pos;
    }

    if (*cp != '<') {
        return std::string();
    }

    std::string s;
    do {
        s += *cp;
        ++pos;
    } while (*cp++ != '>' && *cp != 0);

    *offset = pos;

    return s;
}


// xml encodings (xml-encoded entities are preceded with '&')
static const char  AMP = '&';
static const char  rawEntity[] = { '<',   '>',   '&',    '\'',    '\"',    0 };
static const char* xmlEntity[] = { "lt;", "gt;", "amp;", "apos;", "quot;", 0 };
static const int   xmlEntLen[] = { 3,     3,     4,      5,       5 };

// Replace xml-encoded entities with the raw text equivalents.
std::string xmlDecode(const std::string& encoded)
{
    std::string::size_type iAmp = encoded.find(AMP);
    if (iAmp == std::string::npos) {
        return encoded;
    }

    std::string decoded(encoded, 0, iAmp);
    std::string::size_type iSize = encoded.size();
    decoded.reserve(iSize);

    const char* ens = encoded.c_str();
    while (iAmp != iSize) {
        if (encoded[iAmp] == AMP && (iAmp + 1) < iSize) {
            int iEntity;
            for (iEntity = 0; xmlEntity[iEntity] != 0; ++iEntity) {
                //if (encoded.compare(iAmp+1, xmlEntLen[iEntity], xmlEntity[iEntity]) == 0)
                if (strncmp(ens + iAmp + 1, xmlEntity[iEntity], xmlEntLen[iEntity]) == 0) {
                    decoded += rawEntity[iEntity];
                    iAmp += xmlEntLen[iEntity] + 1;
                    break;
                }
            }
            if (xmlEntity[iEntity] == 0) {   // unrecognized sequence
                decoded += encoded[iAmp++];
            }
        } else {
            decoded += encoded[iAmp++];
        }
    }

    return decoded;
}

// Replace raw text with xml-encoded entities.
std::string xmlEncode(const std::string& raw)
{
    std::string::size_type iRep = raw.find_first_of(rawEntity);
    if (iRep == std::string::npos) {
        return raw;
    }

    std::string encoded(raw, 0, iRep);
    std::string::size_type iSize = raw.size();

    while (iRep != iSize) {
        int iEntity;
        for (iEntity=0; rawEntity[iEntity] != 0; ++iEntity) {
            if (raw[iRep] == rawEntity[iEntity]) {
                encoded += AMP;
                encoded += xmlEntity[iEntity];
                break;
            }
        }
        if (rawEntity[iEntity] == 0) {
            encoded += raw[iRep];
        }
        ++iRep;
    }

    return encoded;
}

} // namespace Util
} // namespace XmlRpc
