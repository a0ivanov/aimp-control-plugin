// Copyright (c) 2011, Alexey Ivanov

#ifndef RPCMETHOD_H
#define RPCMETHOD_H

#include <string>
#include <boost/noncopyable.hpp>

namespace Rpc {

class Value;

//! flag to RequestHandler response, if response should be delayed or sent immediately.
enum ResponseType { RESPONSE_DELAYED, RESPONSE_IMMEDIATE };

//! Abstract single RPC method
class Method : boost::noncopyable
{
public:

    Method(const std::string& method_name)
        :
        name_(method_name)
    {}

    virtual ~Method() {}

    /*!
        \brief Execute the method. Subclasses must provide a definition for this method.
        \return true if method response should be delayed (Comet technique, "subscribe" method implementation), false if response must be sent immediately.
    */
    virtual ResponseType execute(const Value& root_request, Value& root_response) = 0;

    /*!
        \brief Returns a help string for the method.
               Subclasses should override this method if introspection is being used.
    */
    virtual std::string help() const
        { return std::string(); }

    const std::string& name() const
        { return name_; }

protected:

    std::string name_;
};

} // namespace Rpc

#endif // #ifndef RPCMETHOD_H
