//#ifdef BLUETOOTH_ENABLED

#ifndef BOOST_ASIO_BLUETOOTH_BLUETOOTH_ENDPOINT_HPP__
#define BOOST_ASIO_BLUETOOTH_BLUETOOTH_ENDPOINT_HPP__

#include <ws2bth.h>
#include <bthsdpdef.h>

#include <boost/asio/basic_stream_socket.hpp>

namespace boost {
namespace asio {
namespace bluetooth {

template<typename BluetoothProtocol>
class bluetooth_endpoint {
private:
	static BTH_ADDR addr_any;

public:
	/// The protocol type associated with the endpoint.
	typedef BluetoothProtocol protocol_type;

	/// The type of the endpoint structure. This type is dependent on the
	/// underlying implementation of the socket layer.
	typedef boost::asio::detail::socket_addr_type address_type;

	/// Default constructor.
	bluetooth_endpoint()
        : address_()
    {
		address_.addressFamily = AF_BTH; //AF_BLUETOOTH;
		address_.btAddr = addr_any;
        address_.serviceClassId = GUID_NULL;
		address_.port = BT_PORT_ANY;
	}

	bluetooth_endpoint(const BluetoothProtocol& protocol,
			           unsigned short port)
        : address_()
    {
		address_.addressFamily = AF_BLUETOOTH;
		address_.btAddr = addr_any;
        address_.serviceClassId = GUID_NULL;
		address_.port = port;
	}

	/// Construct an endpoint using a port number, specified in the host's byte
	/// order. The IP address will be the any address (i.e. INADDR_ANY or
	/// in6addr_any). This constructor would typically be used for accepting new
	/// connections.
	bluetooth_endpoint(unsigned short port)
        : address_()
    {
		address_.addressFamily = AF_BLUETOOTH;
		address_.btAddr = *BDADDR_ANY;
        address_.serviceClassId = GUID_NULL;
		address_.port = port;
	}

	/// Construct an endpoint using a port number and an BT address.
	/// The address is in human readable form as a string.
	bluetooth_endpoint(const char *addr, unsigned short port)
        : address_()
    {
		address_.addressFamily = AF_BLUETOOTH;
		address_.port = port;
		str2ba(addr, &address_.btAddr);
        address_.serviceClassId = GUID_NULL;
	}

	/// Construct an endpoint using a port number and an BT address.
	/// The address is given in the bluetooth-internal format.
	bluetooth_endpoint(BTH_ADDR addr, unsigned short port)
        : address_()
    {
		address_.addressFamily = AF_BLUETOOTH;
		address_.port = port;
		address_.btAddr = addr;
        address_.serviceClassId = GUID_NULL;
	}

	/// Copy constructor.
	bluetooth_endpoint(const bluetooth_endpoint& other) :
		address_(other.address_) {
	}

	/// Assign from another endpoint.
	bluetooth_endpoint& operator=(const bluetooth_endpoint& other) {
		address_ = other.address_;
		return *this;
	}

	/// The protocol associated with the endpoint.
	protocol_type protocol() const {
		return protocol_type::get();
	}

	/// Get the underlying endpoint in the native type.
	/// TODO: make this nice and generic -> union like in tcp
	address_type* data() {
		return (boost::asio::detail::socket_addr_type*) &address_;
	}

	/// Get the underlying endpoint in the native type.
	const address_type* data() const {
		return (boost::asio::detail::socket_addr_type*) &address_;
	}

	/// Get the underlying size of the endpoint in the native type.
	std::size_t size() const {
        return sizeof(address_);
	}

	/// Set the underlying size of the endpoint in the native type.
	void resize(std::size_t size) {
		if ( size > sizeof(SOCKADDR_BTH) ) {
			boost::system::system_error e(boost::asio::error::invalid_argument);
			boost::throw_exception(e);
		}
	}

	/// Get the capacity of the endpoint in the native type.
	std::size_t capacity() const {
		return sizeof(address_);
	}

	/// Get the port associated with the endpoint. The port number is always in
	/// the host's byte order.
	unsigned short port() const {
		return address_.port;
	}

	/// Set the port associated with the endpoint. The port number is always in
	/// the host's byte order.
	void port(unsigned short port_num) {
		address_.port = port_num;
	}

	///// Get the Bluetooth address associated with the endpoint.
	//BTH_ADDR address() const {
	//	return address_.btAddr;
	//}

	///// Set the Bluetooth address associated with the endpoint.
	//void address(const boost::asio::ip::address& addr) {
	//	bluetooth_endpoint<BluetoothProtocol> tmp_endpoint(addr, port());
	//	address_ = tmp_endpoint.address_;
	//}

	///// Get the Bluetooth address in human readable form and write it to buf.
	//void address_hr(char &buf) {
	//	ba2str(&address_.btAddr, buf);
	//}

    std::string to_string() const
    {
      boost::system::error_code ec;
      std::string addr = to_string(ec);
      boost::asio::detail::throw_error(ec);
      return addr;
    }

    std::string to_string(boost::system::error_code& ec) const {
        const int max_addr_bth_str_len = 256;
        char addr_str[max_addr_bth_str_len + 1];
        //WSAPROTOCOL_INFOA protocol_info;
        //size_t protocol_info_size = sizeof(protocol_info);
        //boost::asio::detail::socket_ops(&address_, 0, SOL_SOCKET, SO_PROTOCOL_INFO, &protocol_info, &protocol_info_size, ec);
        DWORD addr_str_length = max_addr_bth_str_len;
        using namespace boost::asio::detail::socket_ops;
        const INT result = error_wrapper(::WSAAddressToStringA(reinterpret_cast<LPSOCKADDR>(const_cast<SOCKADDR_BTH*>(&address_) ),
                                                               size(),
                                                               NULL, // &protocol_info,
                                                               addr_str,
                                                               &addr_str_length),
                                          ec);
        const int socket_error_retval = SOCKET_ERROR;
        // Windows may set error code on success.
        if (result != socket_error_retval) {
            ec = boost::system::error_code();
        // Windows may not set an error code on failure.
        } else if (result == socket_error_retval && !ec) {
            ec = boost::asio::error::invalid_argument;
        }

        if (result == socket_error_retval)  {
            return std::string();
        }        
        
        return addr_str;
    }

	/// Compare two endpoints for equality.
	friend bool operator==(const bluetooth_endpoint& e1,
			const bluetooth_endpoint& e2) {
		return e1.address_ == e2.address_ && e1.port() == e2.port();
	}

	/// Compare two endpoints for inequality.
	friend bool operator!=(const bluetooth_endpoint& e1,
			const bluetooth_endpoint& e2) {
		return e1.address_ != e2.address_ || e1.port() != e2.port();
	}

	/// Compare endpoints for ordering.
	friend bool operator<(const bluetooth_endpoint<BluetoothProtocol>& e1,
			const bluetooth_endpoint<BluetoothProtocol>& e2) {
		if (e1.address_ < e2.address_) return true;
		if (e1.address_ != e2.address_) return false;
		return e1.port() < e2.port();
	}

private:
	// The underlying rfcomm socket address structure thingy.
	SOCKADDR_BTH address_;
};

template<typename X>
BTH_ADDR bluetooth_endpoint<X>::addr_any = 0UL;

template<typename Elem, typename Traits, typename BluetoothProtocol>
std::basic_ostream<Elem, Traits>& operator<<(std::basic_ostream<Elem, Traits>& os,
                                             const bluetooth_endpoint<BluetoothProtocol>& endpoint)
{
    boost::system::error_code ec;
    std::string s = endpoint.to_string(ec);
    if (ec) {
        if (os.exceptions() & std::basic_ostream<Elem, Traits>::failbit) {
            boost::asio::detail::throw_error(ec);
        } else {
            os.setstate(std::basic_ostream<Elem, Traits>::failbit);
        }
    } else {
        for (std::string::iterator i = s.begin(); i != s.end(); ++i) {
            os << os.widen(*i);
        }
    }
    return os;
}

}}} // namespace boost::asio::bluetooth

#endif // #ifndef BOOST_ASIO_BLUETOOTH_BLUETOOTH_ENDPOINT_HPP__
//#endif // #ifdef BLUETOOTH_ENABLED
