// Copyright (c) 2012, Alexey Ivanov

#ifndef XMLRPC_REQUEST_PARSER_H
#define XMLRPC_REQUEST_PARSER_H

#include "rpc/request_parser.h"

namespace Rpc { class Value; }

namespace XmlRpc
{

class RequestParser : public Rpc::RequestParser
{
    virtual bool parse_(const std::string& /*request_uri*/,
                        const std::string& request_content,
                        Rpc::Value* root);
};

} // namespace XmlRpc

#endif // #ifndef XMLRPC_REQUEST_PARSER_H
