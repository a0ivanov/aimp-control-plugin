// Copyright (c) 2012, Alexey Ivanov

#ifndef RPC_REQUEST_HANDLER_H
#define RPC_REQUEST_HANDLER_H

#include <boost/logic/tribool.hpp>
#include <boost/ptr_container/ptr_map.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

// headers of DelayedResponseSender class
#include <boost/enable_shared_from_this.hpp>

namespace Http { class DelayedResponseSender; }

namespace Rpc
{

class Method;
class Value;
class RpcCallerDescription;
class Frontend;
class ResponseSerializer;

class RequestHandler : boost::noncopyable
{
    friend class DelayedResponseSender;

public:

    void addFrontend(std::auto_ptr<Frontend> frontend);

    /*!
        \brief Appends method object in methods list.
               Object will be owned by RequestHandler.
    */
    void addMethod(std::auto_ptr<Method> method);

    boost::shared_ptr<DelayedResponseSender> getDelayedResponseSender() const;

    Frontend* getFrontEnd(const std::string& uri);

    boost::tribool handleRequest(const std::string& request_uri,
                                 const std::string& request_content,
                                 boost::shared_ptr<Http::DelayedResponseSender> delayed_response_sender,
                                 Frontend& frontend,
                                 std::string* response,
                                 std::string* response_content_type
                                 );

private:

    boost::tribool callMethod(const Value& root,
                              boost::shared_ptr<Http::DelayedResponseSender> delayed_response_sender,
                              ResponseSerializer& response_serializer,
                              std::string* response
                              );

    // Get method object by name from registered methods.
    Rpc::Method* getMethodByName(const std::string& name);

    typedef boost::ptr_vector<Frontend> Frontends;
    Frontends frontends_;

    typedef boost::ptr_map<std::string, Method> RPCMethodsMap; // maps method name to pointer to method object.
    RPCMethodsMap rpc_methods_; // List of RPC methods.

    boost::weak_ptr<Http::DelayedResponseSender> active_delayed_response_sender_; // stores response sender while Rpc method is executed. Allows not to pass this handler in Method::execute() as argument since only comet method needs it.
    ResponseSerializer* active_response_serializer_; // work in pair with active_delayed_response_sender_ member.
};


//! Adaptor for Connection class, provide only one method: sendResponse().
class DelayedResponseSender : public boost::enable_shared_from_this<DelayedResponseSender>, boost::noncopyable
{
public:

    DelayedResponseSender(boost::shared_ptr<Http::DelayedResponseSender> comet_http_response_sender,
                          const ResponseSerializer& response_serializer
                          )
        :
        comet_http_response_sender_(comet_http_response_sender),
        response_serializer_(response_serializer)
    {}

    void sendResponseSuccess(const Value& root_response);

    void sendResponseFault(const Value& root_request, const std::string& error_msg, int error_code);

private:

    boost::shared_ptr<Http::DelayedResponseSender> comet_http_response_sender_;
    const ResponseSerializer& response_serializer_;
};

typedef boost::shared_ptr<DelayedResponseSender> DelayedResponseSender_ptr;


//! Client descriptor for use in RPC methods.
class RpcCallerDescription
{
public:
    RpcCallerDescription(const std::string& ip_addr)
        :
        ip_addr_(ip_addr)
    {}

private:

    std::string ip_addr_;
};

typedef boost::shared_ptr<RpcCallerDescription> RpcCallerDescription_ptr;

} // namespace Rpc

#endif // #ifndef RPC_REQUEST_HANDLER_H
