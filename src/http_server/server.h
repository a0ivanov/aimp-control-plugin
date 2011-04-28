// Copyright (c) 2003-2010 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "connection.h"

namespace Http
{

/*
    HTTP async server.
    Uses Http::RequestHandler object to generate response to request.
*/
class Server : private boost::noncopyable
{
public:
    /*
        Construct the server to listen on the specified TCP address and port.
    */
    explicit Server(boost::asio::io_service& io_service,
                    const std::string& address,
                    const std::string& port,
                    RequestHandler& request_handler
                    ); // throws std::runtime_error.

    ~Server();

private:

    // Handle completion of an asynchronous accept operation.
    void handle_accept(const boost::system::error_code& e);

    // The io_service used to perform asynchronous operations.
    boost::asio::io_service& io_service_;

    // Acceptor used to listen for incoming connections.
    boost::asio::ip::tcp::acceptor acceptor_;

    // The handler for all incoming requests.
    RequestHandler& request_handler_;

    // The next connection to be accepted.
    Connection_ptr new_connection_;
};

} // namespace Http

#endif // #ifndef HTTP_SERVER_H
