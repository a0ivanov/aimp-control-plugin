// Copyright (c) 2013, Alexey Ivanov

#pragma once

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
