// Copyright (c) 2003-2010 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef HTTP_REQUEST_PARSER_H
#define HTTP_REQUEST_PARSER_H

#include <string>
#include <boost/logic/tribool.hpp>
#include <boost/tuple/tuple.hpp>
#include "mpfd_parser/Parser.h"

namespace Http {

struct Request;

/// Parser for incoming requests.
class request_parser
{
public:
    /// Construct ready to parse the request method.
    request_parser();

    /// Reset to initial parser state.
    void reset();

    /// Parse some data. The tribool return value is true when a complete request
    /// has been parsed, false if the data is invalid, indeterminate when more
    /// data is required. The InputIterator return value indicates how much of the
    /// input has been consumed.
    template <typename InputIterator>
    boost::tuple<boost::tribool, InputIterator> parse(Request& req,
                                                      InputIterator begin,
                                                      InputIterator end)
    {
        switch (state_) {
        case content_multipart_formdata:
            return parse_mpfd(req, begin, end);
        default:
            while (begin != end) {
                boost::tribool result = consume(req, *begin++);
                if (result || !result) {
                    return boost::make_tuple(result, begin);
                }

                if (state_ == content_multipart_formdata) {
                    return parse_mpfd(req, --begin, end);
                }
            }
            break;
        }

        boost::tribool result = boost::indeterminate;
        return boost::make_tuple(result, begin);
    }

private:

    template <typename InputIterator>
    boost::tuple<boost::tribool, InputIterator> parse_mpfd(Request& req,
                                                           InputIterator begin,
                                                           InputIterator end)
    {
        assert (state_ == content_multipart_formdata);

        if (content_consumed_ < content_length_) {   
            assert(begin <= end);
            const std::size_t length = std::distance(begin, end);
            assert(mpfd_parser_);
            mpfd_parser_->AcceptSomeData(begin, length);
            content_consumed_ += length;
            boost::tribool result = boost::indeterminate;
            if (content_consumed_ == content_length_) {
                result = true; // all content has been consumed, stop parsing.
                req;
            }
            return boost::make_tuple(result, end);
        }
        return boost::make_tuple(false, begin);
    }

    /// The name of the content length header.
    static std::string content_length_name_;

    /// Content length as decoded from headers. Defaults to 0.
    std::size_t content_length_;

    /// Handle the next character of input.
    boost::tribool consume(Request& req, char input);

    /// Check if a byte is an HTTP character.
    static bool is_char(int c);

    /// Check if a byte is an HTTP control character.
    static bool is_ctl(int c);

    /// Check if a byte is defined as an HTTP tspecial character.
    static bool is_tspecial(int c);

    /// Check if a byte is a digit.
    static bool is_digit(int c);

    /// Check if two characters are equal, without regard to case.
    static bool tolower_compare(char a, char b);

    /// Check whether the two request header names match.
    static bool headers_equal(const std::string& a, const std::string& b);

    /// The current state of the parser.
    enum state
    {
        method_start,
        method,
        uri_start,
        uri,
        http_version_h,
        http_version_t_1,
        http_version_t_2,
        http_version_p,
        http_version_slash,
        http_version_major_start,
        http_version_major,
        http_version_minor_start,
        http_version_minor,
        expecting_newline_1,
        header_line_start,
        header_lws,
        header_name,
        space_before_header_value,
        header_value,
        expecting_newline_2,
        expecting_newline_3,
        select_content_parser,
        content,
        content_multipart_formdata
    } state_;

    typedef MPFD::Parser mpfd_parser;
    std::unique_ptr<mpfd_parser> mpfd_parser_;
    std::size_t content_consumed_;
};

} // namespace Http

#endif // HTTP_REQUEST_PARSER_H
