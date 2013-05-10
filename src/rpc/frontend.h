// Copyright (c) 2013, Alexey Ivanov

#pragma once

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
