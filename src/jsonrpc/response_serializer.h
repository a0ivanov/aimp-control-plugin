// Copyright (c) 2014, Alexey Ivanov

#ifndef JSONRPC_RESPONSE_SERIALIZER_H
#define JSONRPC_RESPONSE_SERIALIZER_H

#include "rpc/response_serializer.h"

namespace JsonRpc
{

struct ResponseSerializerImpl;

class ResponseSerializer : public Rpc::ResponseSerializer
{
public:

    ResponseSerializer();
    ~ResponseSerializer();

    virtual void serializeSuccess(const Rpc::Value& root_response, std::string* response) const;

    virtual void serializeFault(const Rpc::Value& root_request, const std::string& error_msg, int error_code, std::string* response) const;

    virtual const std::string& mimeType() const;

private:

    ResponseSerializerImpl* impl_;

    ResponseSerializer(const ResponseSerializer&);
    ResponseSerializer& operator=(const ResponseSerializer&);
};

} // namespace JsonRpc

#endif // #ifndef JSONRPC_RESPONSE_SERIALIZER_H
