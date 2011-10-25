// Copyright (c) 2003-2010 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "connection.h"

#define BLUETOOTH_ENABLED // move it to config
#include "transport/asio/bluetooth_endpoint.hpp"
#include "transport/asio/rfcomm.hpp"

namespace Http
{

typedef Connection<boost::asio::bluetooth::rfcomm::socket> ConnectionBluetoothRfcomm;
typedef boost::shared_ptr<ConnectionBluetoothRfcomm> ConnectionBluetoothRfcomm_ptr;

typedef Connection<boost::asio::ip::tcp::socket> ConnectionIpTcp;
typedef boost::shared_ptr<ConnectionIpTcp> ConnectionIpTcp_ptr;

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
    Server(boost::asio::io_service& io_service,
           const std::string& address,
           const std::string& port,
           RequestHandler& request_handler
           ); // throws std::runtime_error.

    ~Server();

private:

    // Handle completion of an asynchronous accept operation(ip::tcp).
    void handle_accept(boost::asio::ip::tcp::acceptor& acceptor,
                       ConnectionIpTcp_ptr& connection,
                       const boost::system::error_code& e);

    // Handle completion of an asynchronous accept operation(bluetooth::rfcomm).
    void handle_accept_bluetooth(const boost::system::error_code& e);

    void open_specified_socket(const std::string& address, const std::string& port);

    void open_localhost_socket(const std::string& port);

    void open_bluetooth_socket(); // throws std::runtime_error.

    void register_bluetooth_service(); // throws std::runtime_error.

    // The io_service used to perform asynchronous operations.
    boost::asio::io_service& io_service_;

    // Acceptors used to listen for incoming connections over ip::tcp.
    boost::asio::ip::tcp::acceptor acceptor_,
                                   acceptor_localhost_;
    // Acceptors used to listen for incoming connections over bluetooth::rfcomm.
    boost::asio::bluetooth::rfcomm::acceptor acceptor_bluetooth_;

    // The handler for all incoming requests.
    RequestHandler& request_handler_;

    // The next connection to be accepted.
    ConnectionIpTcp_ptr new_connection_,           // connection on interface read from settings file.
                        new_connection_localhost_; // localhost connection, use it unconditionally.
    ConnectionBluetoothRfcomm_ptr new_connection_bluetooth_;
};

} // namespace Http

#endif // #ifndef HTTP_SERVER_H
