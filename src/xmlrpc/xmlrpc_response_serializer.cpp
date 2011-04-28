// Copyright (c) 2011, Alexey Ivanov

#include "stdafx.h"
#include <cassert>
#include "xmlrpc/response_serializer.h"
#include "xmlrpc/parse_util.h"
#include "xmlrpc/value.h"
#include "rpc/value.h"
#include "rpc/exception.h"

namespace XmlRpc
{

const std::string kMIME_TYPE = "text/xml";

void convertRpcValueToXmlRpcValue(const Rpc::Value& rpc_value, Value* xml_rpc_value) // throws Rpc::Exception
{
    assert(xml_rpc_value);

    switch ( rpc_value.type() ) {
    case Rpc::Value::TYPE_NONE:
        Value().swap(*xml_rpc_value);
        break;
    case Rpc::Value::TYPE_BOOL:
        *xml_rpc_value = bool(rpc_value);
        break;
    case Rpc::Value::TYPE_INT:
        *xml_rpc_value = int(rpc_value);
        break;
    case Rpc::Value::TYPE_UINT:
        {
        const unsigned int uint_value = unsigned int(rpc_value);
        if (uint_value > INT_MAX) {
            assert(!"Rpc::Value(uint): cast error uint -> int");
            throw Rpc::Exception("cast error: uint -> int", Rpc::VALUE_RANGE_ERROR);
        }
        *xml_rpc_value = static_cast<int>(uint_value);
        }
        break;
    case Rpc::Value::TYPE_DOUBLE:
        *xml_rpc_value = double(rpc_value);
        break;
    case Rpc::Value::TYPE_STRING:
        *xml_rpc_value = static_cast<const std::string&>(rpc_value);
        break;
    case Rpc::Value::TYPE_ARRAY:
        xml_rpc_value->setSize( rpc_value.size() );
        for (size_t i = 0, size = xml_rpc_value->size(); i != size; ++i) {
            convertRpcValueToXmlRpcValue(rpc_value[i], &((*xml_rpc_value)[i]));
        }
        break;
    case Rpc::Value::TYPE_OBJECT:
        {
        Value::ValueStruct structValue;
        Rpc::Value::Object::const_iterator member_it = rpc_value.getObjectMembersBegin(),
                                           end       = rpc_value.getObjectMembersEnd();
        for (; member_it != end; ++member_it) {
            convertRpcValueToXmlRpcValue(member_it->second, &structValue[member_it->first]);
        }

        *xml_rpc_value = structValue;
        }
        break;
    case Rpc::Value::TYPE_NULL:
        Value( Value::Nil() ).swap(*xml_rpc_value);
        break;
    default:
        throw Rpc::Exception("unknown type", Rpc::TYPE_ERROR);
    }
}

void generateResponse(const std::string& result_xml, std::string* response)
{
    static const char RESPONSE_START[] =
        "<?xml version=\"1.0\"?>\r\n"
        "<methodResponse><params><param>\r\n\t";
    static const char RESPONSE_END[] =
        "\r\n</param></params></methodResponse>\r\n";

    *response = RESPONSE_START;
    *response += result_xml;
    *response += RESPONSE_END;
}

void generateFaultResponse(const std::string& error_msg, int error_code, std::string* response)
{
    static const char RESPONSE_START[] =
        "<?xml version=\"1.0\"?>\r\n"
        "<methodResponse><fault>\r\n\t";
    static const char RESPONSE_END[] =
        "\r\n</fault></methodResponse>\r\n";

    static const std::string FAULTCODE   = "faultCode";
    static const std::string FAULTSTRING = "faultString";

    Value fault_struct;
    fault_struct[FAULTCODE] = error_code;
    fault_struct[FAULTSTRING] = error_msg;
    *response = RESPONSE_START;
    *response += fault_struct.toXml();
    *response += RESPONSE_END;
}


void ResponseSerializer::serializeSuccess(const Rpc::Value& root_response, std::string* response) const
{
    Value xmlrpc_response;
    convertRpcValueToXmlRpcValue(root_response["result"], &xmlrpc_response);
    generateResponse(xmlrpc_response.toXml(), response);
}

void ResponseSerializer::serializeFault(const Rpc::Value& /*root_request*/, const std::string& error_msg, int error_code, std::string* response) const
{
    generateFaultResponse(error_msg, error_code, response);
}

const std::string& ResponseSerializer::mimeType() const
{
    return kMIME_TYPE;
}

} // namespace XmlRpc
