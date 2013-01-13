// Copyright (c) 2013, Alexey Ivanov

#ifndef JSONRPC_REQUEST_PARSER_H
#define JSONRPC_REQUEST_PARSER_H

#include "rpc/request_parser.h"

namespace Rpc { class Value; }

namespace JsonRpc
{

struct RequestParserImpl;

class RequestParser : public Rpc::RequestParser
{
public:

    RequestParser();
    ~RequestParser();

private:

    virtual bool parse_(const std::string& /*request_uri*/,
                        const std::string& request_content,
                        Rpc::Value* root);

    RequestParserImpl* impl_;

    RequestParser(const RequestParser&);
    RequestParser& operator=(const RequestParser&);
};

} // namespace JsonRpc

#endif // #ifndef JSONRPC_REQUEST_PARSER_H
