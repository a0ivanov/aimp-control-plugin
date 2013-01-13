// Copyright (c) 2013, Alexey Ivanov

#include "stdafx.h"
#include "webctlrpc/response_serializer.h"
#include "rpc/value.h"
#include "rpc/exception.h"
#include <cassert>

namespace WebCtlRpc
{

const std::string kMIME_TYPE = "application/json";

void ResponseSerializer::serializeSuccess(const Rpc::Value& root_response, std::string* response) const
{
    *response = root_response["result"];
}

void ResponseSerializer::serializeFault(const Rpc::Value& /*root_request*/, const std::string& /*error_msg*/, int /*error_code*/, std::string* response) const
{
    *response = "";
}

const std::string& ResponseSerializer::mimeType() const
{
    return kMIME_TYPE;
}

} // namespace WebCtlRpc
