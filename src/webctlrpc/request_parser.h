// Copyright (c) 2012, Alexey Ivanov

#ifndef WEBCTLRPC_REQUEST_PARSER_H
#define WEBCTLRPC_REQUEST_PARSER_H

#include "rpc/request_parser.h"

namespace Rpc { class Value; }

namespace WebCtlRpc
{

class RequestParser : public Rpc::RequestParser
{
    virtual bool parse_(const std::string& request_uri,
                        const std::string& /*request_content*/,
                        Rpc::Value* root);
};

} // namespace WebCtlRpc

#endif // #ifndef WEBCTLRPC_REQUEST_PARSER_H
