// Copyright (c) 2013, Alexey Ivanov

#ifndef WEBCTLRPCFRONTEND_H
#define WEBCTLRPCFRONTEND_H

#include "rpc/frontend.h"
#include "webctlrpc/request_parser.h"
#include "webctlrpc/response_serializer.h"

namespace WebCtlRpc
{

class Frontend : public Rpc::Frontend
{
public:

    virtual bool canHandleRequest(const std::string& uri) const
        { return uri.find('?') != std::string::npos; }

    virtual Rpc::RequestParser& requestParser()
        { return request_parser_; }

    virtual Rpc::ResponseSerializer& responseSerializer()
        { return response_serializer_; }

private:

    RequestParser request_parser_;
    ResponseSerializer response_serializer_;
};

} // namespace WebCtlRpc

#endif // #ifndef WEBCTLRPCFRONTEND_H
