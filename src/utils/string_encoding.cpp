// Copyright (c) 2011, Alexey Ivanov

#include "stdafx.h"
#include "utils/string_encoding.h"
#include "plugin/logger.h"
#include <vector>

namespace StringEncoding {

/*!
    \brief Converts buffer defined by begin-end pointers in UTF-16 encoding to specified encoding.
    \param ID of destination encoding. (used in WideCharToMultiByte() function)
    \param pointer to buffer begin.
    \param pointer to buffer end.
    \throw InvalidArgument if begin follows end.
    \throw EncodingError if convertion fails.
    \return string in specified encoding.
*/
static std::string from_utf16_to(int code_page, const WCHAR* begin, const WCHAR* end) // throws InvalidArgument, EncodingError
{
    if (begin > end) {
        assert(!"Invalid argument in "__FUNCTION__);
        throw InvalidArgument("Error in "__FUNCTION__", bad arguments: begin > end.");
    } else if (begin == end) {
        /*
            Return empty string here for following reasons:
                1) Zero indicates failure of WideCharToMultiByte.
                2) Prevent exception in debug mode in &converted_buffer[0] statement.
        */
        return std::string();
    }

    const int utf16_string_size = std::distance(begin, end);

    const int converted_buffer_length_calculated = WideCharToMultiByte( code_page, // __in   UINT CodePage,
                                                                        0, // __in   DWORD dwFlags,
                                                                        begin, // __in   LPCWSTR lpWideCharStr,
                                                                        utf16_string_size, // __in   int cchWideChar,
                                                                        NULL, // __out  LPSTR lpMultiByteStr,
                                                                        0, // __in   int cbMultiByte, // value 0 means request for converted converted buffer length
                                                                        NULL, // __in   LPCSTR lpDefaultChar,
                                                                        NULL // __out  LPBOOL lpUsedDefaultChar
                                                                      );
    if (converted_buffer_length_calculated <= 0) {
        std::ostringstream s;
        s << "Error of WideCharToMultiByte(0): error code" << GetLastError() << "in "__FUNCTION__". Code page:" << code_page;
        throw EncodingError( s.str() );
    }

    std::vector<char> converted_buffer(converted_buffer_length_calculated);

    const int converted_buffer_length = WideCharToMultiByte(code_page,
                                                            0,
                                                            begin,
                                                            utf16_string_size,
                                                            &converted_buffer[0],
                                                            converted_buffer_length_calculated,
                                                            NULL,
                                                            NULL
                                                           );
    if (converted_buffer_length != converted_buffer_length_calculated) {
        std::ostringstream s;
        s << "Error of WideCharToMultiByte(): error code" << GetLastError() << "in "__FUNCTION__". Code page:" << code_page;
        throw EncodingError( s.str() );
    }

    return std::string( converted_buffer.begin(), converted_buffer.end() );
}

/*!
    \brief Converts buffer defined by begin-end pointers in specified encoding to UTF-16 encoding.
    \param ID of source encoding. (used in MultiByteToWideChar() function)
    \param pointer to buffer begin.
    \param pointer to buffer end.
    \throw InvalidArgument if begin follows end.
    \throw EncodingError if convertion fails.
    \return string in UTF-16 encoding.
*/
static std::wstring to_utf16_from(int code_page, const CHAR* begin, const CHAR* end) // throws InvalidArgument, EncodingError
{
    if (begin > end) {
        assert(!"Invalid argument in "__FUNCTION__);
        throw InvalidArgument("Error in "__FUNCTION__", bad arguments: begin > end.");
    } else if (begin == end) {
        /*
            Return empty string here for following reasons:
                1) The function MultiByteToWideChar returns 0 if it does not succeed.
                2) Prevent exception in debug mode in &converted_buffer[0] statement.
        */
        return std::wstring();
    }

    const int string_size = std::distance(begin, end);

    const int converted_string_length_calculated = MultiByteToWideChar( code_page,// __in   UINT CodePage,
                                                                        0,// __in   DWORD dwFlags,
                                                                        begin, // __in   LPCSTR lpMultiByteStr,
                                                                        string_size, //__in   int cbMultiByte,
                                                                        NULL,// __out  LPWSTR lpWideCharStr,
                                                                        0 //__in   int cchWideChar // value 0 means request for converted converted string length
                                                                      );

    if (converted_string_length_calculated <= 0) {
        std::ostringstream s;
        s << "Error of MultiByteToWideChar(0): error code" << GetLastError() << "in "__FUNCTION__". Code page:" << code_page;
        throw EncodingError( s.str() );
    }

    std::vector<WCHAR> converted_buffer(converted_string_length_calculated);

    const int converted_string_length = MultiByteToWideChar(code_page,// __in   UINT CodePage,
                                                            0,// __in   DWORD dwFlags,
                                                            begin, // __in   LPCSTR lpMultiByteStr,
                                                            string_size, //__in   int cbMultiByte,
                                                            &converted_buffer[0],// __out  LPWSTR lpWideCharStr,
                                                            converted_string_length_calculated //__in   int cchWideChar
                                                           );
    if (converted_string_length != converted_string_length_calculated) {
        std::ostringstream s;
        s << "Error of MultiByteToWideChar(): error code" << GetLastError() << "in "__FUNCTION__". Code page:" << code_page;
        throw EncodingError( s.str() );
    }
    return std::wstring( converted_buffer.begin(), converted_buffer.end() );
}

std::string utf16_to_system_ansi_encoding(const std::wstring& utf16_string) // throws EncodingError
{
    const int kCodePage = CP_ACP; // The system default Windows ANSI code page.
    const WCHAR* string_data = utf16_string.c_str();
    return from_utf16_to( kCodePage, string_data, string_data + utf16_string.length() );
}

std::string utf16_to_system_ansi_encoding(const WCHAR* begin, const WCHAR* end) // throws InvalidArgument, EncodingError
{
    const int kCodePage = CP_ACP; // The system default Windows ANSI code page.
    return from_utf16_to(kCodePage, begin, end);
}

std::wstring system_ansi_encoding_to_utf16(const std::string& ansi_string) // throws EncodingError
{
    const int kCodePage = CP_ACP; // The system default Windows ANSI code page.
    const CHAR* string_data = ansi_string.c_str();
    return to_utf16_from( kCodePage, string_data, string_data + ansi_string.length() );
}

std::wstring system_ansi_encoding_to_utf16_safe(const std::string& ansi_string) // throws ()
{
    try {
        return system_ansi_encoding_to_utf16(ansi_string);
    } catch (EncodingError&) {
        return std::wstring();
    }
}

std::string utf16_to_system_ansi_encoding_safe(const std::wstring& utf16_string) // throws ()
{
    try {
        return utf16_to_system_ansi_encoding(utf16_string);
    } catch (EncodingError&) {
        return std::string();
    }
}

std::string utf16_to_utf8(const std::wstring& utf16_string) // throws EncodingError
{
    const int kCodePage = CP_UTF8;
    const WCHAR* string_data = utf16_string.c_str();
    return from_utf16_to( kCodePage, string_data, string_data + utf16_string.length() );
}

std::wstring utf8_to_utf16(const std::string& utf8_string) // throws EncodingError
{
    const int kCodePage = CP_UTF8;
    const CHAR* string_data = utf8_string.c_str();
    return to_utf16_from( kCodePage, string_data, string_data + utf8_string.length() );
}

} // namespace StringEncoding
