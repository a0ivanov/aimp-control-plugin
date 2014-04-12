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

typedef boost::asio::bluetooth::rfcomm::acceptor BluetoothConnectionAcceptor;
typedef boost::shared_ptr<BluetoothConnectionAcceptor> BluetoothConnectionAcceptor_ptr;

typedef boost::asio::ip::tcp::acceptor IpTcpConnectionAcceptor;
typedef boost::shared_ptr<IpTcpConnectionAcceptor> IpTcpConnectionAcceptor_ptr;

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
    Server(boost::asio::io_service& io_service, RequestHandler& request_handler); // throws std::runtime_error.

    ~Server();

private:

    // Handle completion of an asynchronous accept operation(ip::tcp).
    void handle_accept(IpTcpConnectionAcceptor_ptr acceptor,
                       ConnectionIpTcp_ptr accepted_connection,
                       const boost::system::error_code& e);

    // Handle completion of an asynchronous accept operation(bluetooth::rfcomm).
    void handle_accept_bluetooth(BluetoothConnectionAcceptor_ptr acceptor,
                                 ConnectionBluetoothRfcomm_ptr accepted_connection,
                                 const boost::system::error_code& e);

    void open_specified_socket(const std::string& address, const std::string& port);

    void start_accept_connections_on(boost::asio::ip::tcp::endpoint endpoint);

    void open_localhost_socket(const std::string& port);

    void open_bluetooth_socket(); // throws std::runtime_error.

    void register_bluetooth_service(BluetoothConnectionAcceptor_ptr acceptor); // throws std::runtime_error.

    // The io_service used to perform asynchronous operations.
    boost::asio::io_service& io_service_;

    // The handler for all incoming requests.
    RequestHandler& request_handler_;
};

} // namespace Http

#endif // #ifndef HTTP_SERVER_H
