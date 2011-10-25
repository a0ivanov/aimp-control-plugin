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
        return sizeof(SOCKADDR_BTH);
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
		return sizeof(SOCKADDR_BTH);
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

}}} // namespace boost::asio::bluetooth

#endif // #ifndef BOOST_ASIO_BLUETOOTH_BLUETOOTH_ENDPOINT_HPP__
//#endif // #ifdef BLUETOOTH_ENABLED
