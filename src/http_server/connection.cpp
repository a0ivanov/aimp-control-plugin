// Copyright (c) 2003-2010 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "stdafx.h"
#include <vector>
#include <boost/bind.hpp>
#include "connection.h"
#include "http_server/request_handler.h"
#include "plugin/logger.h"

//#include <ctime>
//#include <iostream>
#include <string>

namespace Http {

namespace TransmitFile {

#if defined(BOOST_ASIO_HAS_WINDOWS_OVERLAPPED_PTR)

using boost::asio::ip::tcp;
using boost::asio::windows::overlapped_ptr;
using boost::asio::windows::random_access_handle;

// A wrapper for the TransmitFile overlapped I/O operation.
template <typename SocketT, typename Handler>
void transmit_file(SocketT& socket,
    random_access_handle& file, Handler handler)
{
  // Construct an OVERLAPPED-derived object to contain the handler.
  overlapped_ptr overlapped(socket.get_io_service(), handler);

  // Initiate the TransmitFile operation.
  BOOL ok = ::TransmitFile(socket.native_handle(),
      file.native_handle(), 0, 0, overlapped.get(), 0, 0);
  DWORD last_error = ::GetLastError();

  // Check if the operation completed immediately.
  if (!ok && last_error != ERROR_IO_PENDING)
  {
    // The operation completed immediately, so a completion notification needs
    // to be posted. When complete() is called, ownership of the OVERLAPPED-
    // derived object passes to the io_service.
    boost::system::error_code ec(last_error,
        boost::asio::error::get_system_category());
    overlapped.complete(ec, 0);
  }
  else
  {
    // The operation was successfully initiated, so ownership of the
    // OVERLAPPED-derived object has passed to the io_service.
    overlapped.release();
  }
}

template <typename SocketT>
class connection
  : public boost::enable_shared_from_this< connection<SocketT> >,
	private boost::noncopyable
{
public:
  typedef boost::shared_ptr< connection<SocketT> > pointer;

  static pointer create(boost::asio::io_service& io_service,
						boost::shared_ptr<SocketT> socket,
                        const std::wstring& filename)
  {
    return pointer(new connection(io_service, socket, filename));
  }

  SocketT& socket()
  {
    assert(socket_);
    return *socket_;
  }

  void start()
  {
    boost::system::error_code ec;
    file_.assign(::CreateFile(filename_.c_str(), GENERIC_READ, 0, 0,
          OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, 0), ec);
    if (file_.is_open())
    {
      transmit_file(socket(), file_,
          boost::bind(&connection::handle_write, shared_from_this(),
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));
    }
  }

private:
  connection(boost::asio::io_service& io_service, boost::shared_ptr<SocketT> socket, const std::wstring& filename)
    : socket_(socket),
      filename_(filename),
      file_(io_service)
  {
	assert(socket_);
  }

  void handle_write(const boost::system::error_code& /*error*/,
      size_t /*bytes_transferred*/)
  {
    boost::system::error_code ignored_ec;
    socket_->shutdown(SocketT::shutdown_both, ignored_ec);
  }

  boost::shared_ptr<SocketT> socket_;
  std::wstring filename_;
  random_access_handle file_;
};

#else // defined(BOOST_ASIO_HAS_WINDOWS_OVERLAPPED_PTR)
# error Overlapped I/O not available on this platform
#endif // defined(BOOST_ASIO_HAS_WINDOWS_OVERLAPPED_PTR)

} // namespace TransmitFile

template <typename SocketT>
Connection<SocketT>::Connection(boost::asio::io_service& io_service, RequestHandler& handler)
    :
    strand_(io_service),
    socket_(boost::make_shared<SocketT>(io_service)),
    request_handler_(handler)
{
    try {
        BOOST_LOG_SEV(logger(), info) << "Creating connection to host " << socket().remote_endpoint();
    } catch (boost::system::system_error&) {
        BOOST_LOG_SEV(logger(), info) << "Creating connection.";
    }
}

template <typename SocketT>
Connection<SocketT>::~Connection()
{
    try {
        BOOST_LOG_SEV(logger(), info) << "Destroying connection to host " << socket().remote_endpoint();
    } catch (boost::system::system_error&) {
        BOOST_LOG_SEV(logger(), info) << "Destroying connection.";
    }
}

template <typename SocketT>
SocketT& Connection<SocketT>::socket()
{
    assert(socket_);
    return *socket_;
}

template <typename SocketT>
void Connection<SocketT>::start()
{
    read_some_to_buffer();
}

template <typename SocketT>
void Connection<SocketT>::read_some_to_buffer()
{
    socket().async_read_some(boost::asio::buffer(buffer_),
                            strand_.wrap(boost::bind(&Connection<SocketT>::handle_read,
                                                     shared_from_this(),
                                                     boost::asio::placeholders::error,
                                                     boost::asio::placeholders::bytes_transferred
                                                     )
                                         )
                            );
}

template <typename SocketT>
void Connection<SocketT>::write_reply_content()
{
    if ( !reply_.filename.empty() ) {
        // send large file.
        boost::asio::async_write(socket(),
                                 reply_.to_buffers_headers_only(),
                                 strand_.wrap(boost::bind(&Connection<SocketT>::handle_write_headers_on_file_sending,
                                                          shared_from_this(),
                                                          boost::asio::placeholders::error
                                                          )
                                              )
                                 );
    } else {
        // send small string data.
        boost::asio::async_write(socket(),
                                 reply_.to_buffers(),
                                 strand_.wrap(boost::bind(&Connection<SocketT>::handle_write,
                                                          shared_from_this(),
                                                          boost::asio::placeholders::error
                                                          )
                                              )
                                 );
    }
}

template <typename SocketT>
void Connection<SocketT>::handle_read(const boost::system::error_code& e,
                                      std::size_t bytes_transferred)
{
    if (!e) {
        boost::tribool result;
        boost::tie(result, boost::tuples::ignore) = request_parser_.parse(request_,
                                                                          buffer_.data(),
                                                                          buffer_.data() + bytes_transferred
                                                                          );
        if (result) {
            ICometDelayedConnection_ptr comet_connection( new CometDelayedConnection<SocketT>( shared_from_this() ) );
            bool reply_immediately = request_handler_.handle_request(request_, reply_, comet_connection);
            if (reply_immediately) {
                write_reply_content();
            }
        } else if (!result) {
            reply_ = Reply::stock_reply(Reply::bad_request);
            write_reply_content();
        } else {
            read_some_to_buffer();
        }
    } else {
        // BOOST_LOG_SEV(logger(), debug) << "Connection<SocketT>::handle_read(): failed to read data. Reason: " << e.message();
    }

    // If an error occurs then no new asynchronous operations are started. This
    // means that all shared_ptr references to the connection object will
    // disappear and the object will be destroyed automatically after this
    // handler returns. The Connection class's destructor closes the socket.
}

template <typename SocketT>
void Connection<SocketT>::handle_write(const boost::system::error_code& e)
{
    if (!e) {
        // Initiate graceful connection closure.
        boost::system::error_code ignored_ec;
        socket().shutdown(SocketT::shutdown_both, ignored_ec);
    }

    // No new asynchronous operations are started. This means that all shared_ptr
    // references to the connection object will disappear and the object will be
    // destroyed automatically after this handler returns. The Connection class's
    // destructor closes the socket.
}

template <typename SocketT>
void Connection<SocketT>::handle_write_headers_on_file_sending(const boost::system::error_code& e)
{
    if (!e) {
        // http headers were sent successfully, now send file content.
        typedef TransmitFile::connection<SocketT> TransmitFileConnection;
        TransmitFileConnection::pointer tfc = TransmitFileConnection::create(strand_.get_io_service(),
                                                                             socket_,
																		     reply_.filename);
        
        socket_.reset(); // this object is not socket owner anymore.

        tfc->start();
    }

    // No new asynchronous operations are started. This means that all shared_ptr
    // references to the connection object will disappear and the object will be
    // destroyed automatically after this handler returns.
    // All file transferring work will do TransmitFileConnection.
}

template <typename SocketT>
void CometDelayedConnection<SocketT>::sendResponse(DelayedResponseSender_ptr comet_http_response_sender)
{
    BOOST_LOG_SEV(logger(), debug) << "CometDelayedConnection::sendResponse to " << connection_->socket().remote_endpoint();
    boost::asio::async_write( connection_->socket(),
                              comet_http_response_sender->get_reply().to_buffers(),
                              connection_->strand_.wrap(boost::bind(&CometDelayedConnection<SocketT>::handle_write,
                                                                    shared_from_this(),
                                                                    comet_http_response_sender,
                                                                    boost::asio::placeholders::error
                                                                    )
                                                         )
                             );
}

template <typename SocketT>
void CometDelayedConnection<SocketT>::handle_write(DelayedResponseSender_ptr comet_http_response_sender, const boost::system::error_code& e)
{
    if (!e) {
        BOOST_LOG_SEV(logger(), debug) << "CometDelayedConnection::success sending response to " << connection_->socket().remote_endpoint();
        // Initiate graceful connection closure.
        boost::system::error_code ignored_ec;
        connection_->socket().shutdown(SocketT::shutdown_both, ignored_ec);
    } else {
        BOOST_LOG_SEV(logger(), debug) << "CometDelayedConnection::fail to send response to "
                                       << connection_->socket().remote_endpoint()
                                       << ". Reason: " << e.message();
    }

    // No new asynchronous operations are started. This means that all shared_ptr
    // references to the CometDelayedConnection object will disappear and the object will be
    // destroyed automatically after this handler returns. The Connection class's
    // destructor closes the socket.
}

} // namespace Http
