#ifndef BOOST_ASIO_BLUETOOTH_RFCOMM_HPP__
#define BOOST_ASIO_BLUETOOTH_RFCOMM_HPP__

#include "bluetooth_endpoint.hpp"

namespace boost {
namespace asio {
namespace bluetooth {

class rfcomm {
public:
	/// The type of endpoint.
	typedef bluetooth_endpoint<rfcomm> endpoint;

	/// Get an Instance.
	/// We need this to fulfill the asio Endpoint requirements, I think.
	static rfcomm get() {
		return rfcomm();
	}

	/// Obtain an identifier for the type of the protocol.
	int type() const {
		return SOCK_STREAM;
	}

	/// Obtain an identifier for the protocol.
	int protocol() const {
		return BTHPROTO_RFCOMM;
	}

	/// Obtain an identifier for the protocol family.
	int family() const {
		return AF_BTH; // AF_BLUETOOTH;
	}

	/// The RFCOMM socket type.
	typedef basic_stream_socket<rfcomm> socket;

	/// The RFCOMM acceptor type.
	typedef basic_socket_acceptor<rfcomm> acceptor;

};

inline std::ostream& operator<<(std::ostream& os, const rfcomm::endpoint& endpoint)
{
    os << "TODO: implement";
    return os;
}

}}} // namespace boost::asio::bluetooth

#endif // #ifndef BOOST_ASIO_BLUETOOTH_RFCOMM_HPP__
