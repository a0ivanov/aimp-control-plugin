// Copyright (c) 2014, Alexey Ivanov

#pragma once

#include "rpc/frontend.h"
#include "xmlrpc/request_parser.h"
#include "xmlrpc/response_serializer.h"

namespace XmlRpc
{

class Frontend : public Rpc::Frontend
{
public:

    virtual bool canHandleRequest(const std::string& uri) const
        { return uri == "/RPC_XML"; }

    virtual Rpc::RequestParser& requestParser()
        { return request_parser_; }

    virtual Rpc::ResponseSerializer& responseSerializer()
        { return response_serializer_; }

private:

    RequestParser request_parser_;
    ResponseSerializer response_serializer_;
};

} // namespace XmlRpc
