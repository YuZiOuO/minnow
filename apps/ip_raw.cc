#include "socket.hh"
#include <arpa/inet.h>
#include <bitset>
#include <cstring>

using namespace std;

class RawSocket : public DatagramSocket
{
public:
  RawSocket() : DatagramSocket( AF_INET, SOCK_RAW, IPPROTO_UDP ) {}
};

int main()
{
  // construct an Internet or user datagram here, and send using the RawSocket as in the Jan. 10 lecture
  auto sock = RawSocket();
  auto addr = Address { "127.0.0.1", 8081 };
  sock.bind( addr );

  auto recv_addr = Address { "127.0.0.1", 8080 };

  string payload = "Hello,CS144!";

  constexpr uint16_t header_length = 8;
  uint16_t length = header_length + payload.size();

  uint16_t n_addr_port = htons( addr.port() );
  uint16_t n_recv_addr_port = htons( recv_addr.port() );
  uint16_t n_length = htons( length );
  constexpr uint16_t n_checksum = 0;

  uint64_t header = 0;
  header |= static_cast<uint64_t>( n_addr_port );
  header |= static_cast<uint64_t>( n_recv_addr_port ) << 16;
  header |= static_cast<uint64_t>( n_length ) << 32;
  header |= static_cast<uint64_t>( n_checksum ) << 48;

  std::string header_str;
  header_str.resize( sizeof( uint64_t ) );
  std::memcpy( header_str.data(), &header, sizeof( uint64_t ) );

  sock.sendto( recv_addr, header_str + payload );
  return 0;
}
