// Copyright (c) 2003-2010 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "stdafx.h"
#include <ctype.h>
#include "request_parser.h"
#include "request.h"
#include <boost/lexical_cast.hpp>
#include "mpfd_parser_factory.h"
#include "http_server/header.h"

namespace Http {

request_parser::request_parser()
    :
    content_length_(0),
    state_(method_start),
    content_consumed_(0)
{
}

void request_parser::reset()
{
    state_ = method_start;
    content_consumed_ = 0;
}

std::string request_parser::content_length_name_ = "Content-Length";
const std::string content_type_name = "Content-Type";

boost::tribool request_parser::consume(Request& req, char input)
{
    switch (state_)
    {
    case method_start:
        req.method.clear();
        req.uri.clear();
        req.http_version_major = 0;
        req.http_version_minor = 0;
        req.headers.clear();
        req.content.clear();
        content_length_ = 0;

        if ( !is_char(input) || is_ctl(input) || is_tspecial(input) ) {
            return false;
        } else {
            state_ = method;
            req.method.push_back(input);
            return boost::indeterminate;
        }
    case method:
        if (input == ' ') {
            state_ = uri;
            return boost::indeterminate;
        } else if ( !is_char(input) || is_ctl(input) || is_tspecial(input) ) {
            return false;
        } else {
            req.method.push_back(input);
            return boost::indeterminate;
        }
    case uri_start:
        if ( is_ctl(input) ) {
            return false;
        } else {
            state_ = uri;
            req.uri.push_back(input);
            return boost::indeterminate;
        }
    case uri:
        if (input == ' ') {
            state_ = http_version_h;
            return boost::indeterminate;
        } else if ( is_ctl(input) ) {
            return false;
        } else {
            req.uri.push_back(input);
            return boost::indeterminate;
        }
    case http_version_h:
        if (input == 'H') {
            state_ = http_version_t_1;
            return boost::indeterminate;
        } else {
            return false;
        }
    case http_version_t_1:
        if (input == 'T') {
            state_ = http_version_t_2;
            return boost::indeterminate;
        } else {
            return false;
        }
    case http_version_t_2:
        if (input == 'T') {
            state_ = http_version_p;
            return boost::indeterminate;
        } else {
            return false;
        }
    case http_version_p:
        if (input == 'P') {
            state_ = http_version_slash;
            return boost::indeterminate;
        } else {
            return false;
        }
    case http_version_slash:
        if (input == '/') {
            req.http_version_major = 0;
            req.http_version_minor = 0;
            state_ = http_version_major_start;
            return boost::indeterminate;
        } else {
            return false;
        }
    case http_version_major_start:
        if ( is_digit(input) ) {
            req.http_version_major = req.http_version_major * 10 + input - '0';
            state_ = http_version_major;
            return boost::indeterminate;
        } else {
            return false;
        }
    case http_version_major:
        if (input == '.') {
            state_ = http_version_minor_start;
            return boost::indeterminate;
        } else if ( is_digit(input) ) {
            req.http_version_major = req.http_version_major * 10 + input - '0';
            return boost::indeterminate;
        } else {
            return false;
        }
    case http_version_minor_start:
        if ( is_digit(input) ) {
            req.http_version_minor = req.http_version_minor * 10 + input - '0';
            state_ = http_version_minor;
            return boost::indeterminate;
        } else {
            return false;
        }
    case http_version_minor:
        if (input == '\r') {
            state_ = expecting_newline_1;
            return boost::indeterminate;
        } else if ( is_digit(input) ) {
            req.http_version_minor = req.http_version_minor * 10 + input - '0';
            return boost::indeterminate;
        } else {
            return false;
        }
    case expecting_newline_1:
        if (input == '\n') {
            state_ = header_line_start;
            return boost::indeterminate;
        } else {
            return false;
        }
    case header_line_start:
        if (input == '\r') {
            state_ = expecting_newline_3;
            return boost::indeterminate;
        } else if ( !req.headers.empty() && (input == ' ' || input == '\t') ) {
            state_ = header_lws;
            return boost::indeterminate;
        } else if ( !is_char(input) || is_ctl(input) || is_tspecial(input) ) {
            return false;
        } else {
            req.headers.push_back(header());
            req.headers.back().name.push_back(input);
            state_ = header_name;
            return boost::indeterminate;
        }
    case header_lws:
        if (input == '\r') {
            state_ = expecting_newline_2;
            return boost::indeterminate;
        } else if (input == ' ' || input == '\t') {
            return boost::indeterminate;
        } else if ( is_ctl(input) ) {
            return false;
        } else {
            state_ = header_value;
            req.headers.back().value.push_back(input);
            return boost::indeterminate;
        }
    case header_name:
        if (input == ':') {
            state_ = space_before_header_value;
            return boost::indeterminate;
        } else if ( !is_char(input) || is_ctl(input) || is_tspecial(input) ) {
            return false;
        } else {
            req.headers.back().name.push_back(input);
            return boost::indeterminate;
        }
    case space_before_header_value:
        if (input == ' ') {
            state_ = header_value;
            return boost::indeterminate;
        } else {
            return false;
        }
    case header_value:
        if (input == '\r') {
            state_ = expecting_newline_2;
            return boost::indeterminate;
        } else if ( is_ctl(input) ) {
            return false;
        } else {
            req.headers.back().value.push_back(input);
            return boost::indeterminate;
        }
    case expecting_newline_2:
        if (input == '\n') {
            state_ = header_line_start;
            return boost::indeterminate;
        } else {
            return false;
        }
    case expecting_newline_3:
        if (input == '\n') {
            // Check for optional Content-Length header.
            const std::string* content_length_value;
            if (get_header_value(req.headers, content_length_name_, content_length_value)) {
                try {
                    content_length_ = boost::lexical_cast<std::size_t>(*content_length_value);
                    state_ = select_content_parser;
                    return boost::indeterminate;
                } catch (boost::bad_lexical_cast&) {
                    return false;
                }
            }
            
            return true; // no Content-Length header, stop parsing.           
        } else {
            return false;
        }
    case select_content_parser: {
        state_ = content;

        const std::string* content_type_value;
        if (get_header_value(req.headers, content_type_name, content_type_value)) {
            if (boost::starts_with(*content_type_value, "multipart/form-data;")) {
                using namespace MPFD;
                if (ParserFactory::instance()) {
                    req.mpfd_parser = ParserFactory::instance()->createParser(*content_type_value);
                    state_ = content_multipart_formdata;
                    return boost::indeterminate;
                } else {
                    return false;
                }
            }
        }

        return consume(req, input);
                                }
    case content:
        // Content.
        if (req.content.size() < content_length_) {
            req.content.push_back(input);
            if (req.content.size() == content_length_) {
                return true;  // all content has been consumed, stop parsing.
            }
            return boost::indeterminate;
        }
        return false;
    case content_multipart_formdata:
        assert(!"mpfd must not be processed by one char");
        return false;
    default:
        return false;
    }
}

bool request_parser::is_char(int c)
{
    return c >= 0 && c <= 127;
}

bool request_parser::is_ctl(int c)
{
    return (c >= 0 && c <= 31) || (c == 127);
}

bool request_parser::is_tspecial(int c)
{
    switch (c)
    {
    case '(': case ')': case '<': case '>': case '@':
    case ',': case ';': case ':': case '\\': case '"':
    case '/': case '[': case ']': case '?': case '=':
    case '{': case '}': case ' ': case '\t':
        return true;
    default:
        return false;
    }
}

bool request_parser::is_digit(int c)
{
    return c >= '0' && c <= '9';
}

bool tolower_compare(char a, char b)
{
  return ::tolower(a) == ::tolower(b);
}

bool headers_equal(const std::string& a, const std::string& b)
{
  if ( a.length() != b.length() ) {
    return false;
  }

  return std::equal(a.begin(), a.end(), b.begin(),
                    &tolower_compare);
}

bool get_header_value(const std::vector<header>& headers, const std::string& header_name, const std::string*& header_value)
{
    const auto header_it = std::find_if(headers.begin(), headers.end(),
                                        [header_name](const header& h) { return headers_equal(h.name, header_name); }
                                        );
    if (header_it != headers.end()) {
        header_value = &(header_it->value);
        return true;
    }
    return false;
}


} // namespace Http
