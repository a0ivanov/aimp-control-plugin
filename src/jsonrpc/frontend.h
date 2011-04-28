// Copyright (c) 2011, Alexey Ivanov

#ifndef JSONRPCFRONTEND_H
#define JSONRPCFRONTEND_H

#include "rpc/frontend.h"
#include "jsonrpc/request_parser.h"
#include "jsonrpc/response_serializer.h"

namespace JsonRpc
{

class Frontend : public Rpc::Frontend
{
public:

    virtual bool canHandleRequest(const std::string& uri) const
        { return uri == "/RPC_JSON"; }

    virtual Rpc::RequestParser& requestParser()
        { return request_parser_; }

    virtual Rpc::ResponseSerializer& responseSerializer()
        { return response_serializer_; }

private:

    RequestParser request_parser_;
    ResponseSerializer response_serializer_;
};

} // namespace JsonRpc

#endif // #ifndef JSONRPCFRONTEND_H