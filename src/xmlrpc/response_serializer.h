// Copyright (c) 2014, Alexey Ivanov

#pragma once

#include "rpc/response_serializer.h"

namespace XmlRpc
{

class ResponseSerializer : public Rpc::ResponseSerializer
{
public:

    virtual void serializeSuccess(const Rpc::Value& root_response, std::string* response) const;

    virtual void serializeFault(const Rpc::Value& root_request, const std::string& error_msg, int error_code, std::string* response) const;

    virtual const std::string& mimeType() const;

private:

};

} // namespace XmlRpc
