// Copyright (c) 2012, Alexey Ivanov

#ifndef RPCFRONTEND_H
#define RPCFRONTEND_H

#include <string>

namespace Rpc
{

class RequestParser;
class ResponseSerializer;

class Frontend
{
public:

    virtual bool canHandleRequest(const std::string& uri) const = 0;

    virtual RequestParser& requestParser() = 0;

    virtual ResponseSerializer& responseSerializer() = 0;
};

} // namespace Rpc

#endif // #ifndef RPCFRONTEND_H