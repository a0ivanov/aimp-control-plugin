// Copyright (c) 2012, Alexey Ivanov

#include "stdafx.h"
#include "rpc/request_handler.h"
#include "http_server/request_handler.h"
#include "rpc/value.h"
#include "rpc/method.h"
#include "rpc/exception.h"
#include "rpc/frontend.h"
#include "rpc/request_parser.h"
#include "rpc/response_serializer.h"
#include "utils/util.h"
#include <boost/foreach.hpp>

namespace Rpc
{

const int kGENERAL_ERROR_CODE = -1;

void RequestHandler::addFrontend(std::auto_ptr<Frontend> frontend)
{
    frontends_.push_back( frontend.release() );
}

Frontend* RequestHandler::getFrontEnd(const std::string& uri)
{
    BOOST_FOREACH(Frontend& frontend, frontends_) {
        if ( frontend.canHandleRequest(uri) ) {
            return &frontend;
        }
    }
    return nullptr;
}

Method* RequestHandler::getMethodByName(const std::string& name)
{
    auto method_iterator = rpc_methods_.find(name);

    if ( method_iterator != rpc_methods_.end() ) {
        return method_iterator->second;
    }

    return nullptr;
}

void RequestHandler::addMethod(std::auto_ptr<Method> method)
{
    std::string key( method->name() ); // boost::ptr_map::insert() needs reference to non-constant.
    rpc_methods_.insert( key, method.release() );
}

boost::tribool RequestHandler::handleRequest(const std::string& request_uri,
                                             const std::string& request_content,
                                             Http::DelayedResponseSender_ptr delayed_response_sender,
                                             Frontend& frontend,
                                             std::string* response,
                                             std::string* response_content_type
                                             )
{
    assert(response);
    assert(response_content_type);

    *response_content_type = frontend.responseSerializer().mimeType();

    Value root_request;
    if ( frontend.requestParser().parse(request_uri, request_content, &root_request) ) {

        if ( !root_request.isMember("id") ) { // for example xmlrpc does not use request id, so add null value since Rpc methods rely on it.
            root_request["id"] = Value::Null();
        }

        return callMethod(root_request,
                          delayed_response_sender,
                          frontend.responseSerializer(),
                          response
                          );
    } else {
        frontend.responseSerializer().serializeFault(root_request, "Request parsing error", Rpc::REQUEST_PARSING_ERROR, response);
        return false;
    }
}

boost::tribool RequestHandler::callMethod(const Value& root_request,
                                          Http::DelayedResponseSender_ptr delayed_response_sender,
                                          ResponseSerializer& response_serializer,
                                          std::string* response
                                          )
{
    assert(response);

    try {
        const std::string& method_name = root_request["method"];
        Method* method = getMethodByName(method_name);
        if (method == nullptr) {
            response_serializer.serializeFault(root_request, method_name + ": method not found", METHOD_NOT_FOUND_ERROR, response);
            return false;
        }

        
        { // execute method
        //PROFILE_EXECUTION_TIME( method_name.c_str() );

        active_delayed_response_sender_ = delayed_response_sender; // save current http request handler ref in weak ptr to use in delayed response.
        active_response_serializer_ = &response_serializer;

        Value root_response;
        root_response["id"] = root_request["id"]; // currently all methods set id of response, so set it here. Method can set it to null if needed.
        ResponseType response_type = method->execute(root_request, root_response);
        if (RESPONSE_DELAYED == response_type) {
            return boost::indeterminate; // method execution is delayed, say to http response handler not to send answer immediately.
        } else {
            assert( root_response.valid() ); ///??? Return empty string if execute() method did not set result.
            if ( !root_response.valid() ) {
                root_response = "";
            }

            response_serializer.serializeSuccess(root_response, response);
            return true;
        }
        }
    } catch (const Exception& e) {
        response_serializer.serializeFault(root_request, e.message(), e.code(), response);
        return false;
    }
}

boost::shared_ptr<DelayedResponseSender> RequestHandler::getDelayedResponseSender() const
{
    boost::shared_ptr<Http::DelayedResponseSender> ptr = active_delayed_response_sender_.lock();
    if (ptr) {
        return boost::make_shared<DelayedResponseSender>(ptr, *active_response_serializer_);
    } else {
        return boost::shared_ptr<DelayedResponseSender>();
    }
}


void DelayedResponseSender::sendResponseSuccess(const Value& root_response)
{
    std::string response;
    response_serializer_.serializeSuccess(root_response, &response);
    comet_http_response_sender_->send(response,
                                      response_serializer_.mimeType()
                                      );
}

void DelayedResponseSender::sendResponseFault(const Value& root_request, const std::string& error_msg, int error_code)
{
    std::string response_string;
    response_serializer_.serializeFault(root_request, error_msg, error_code, &response_string);
    comet_http_response_sender_->send(response_string,
                                      response_serializer_.mimeType()
                                      );
}

} // namespace XmlRpc
