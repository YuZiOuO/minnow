#include "tcp_receiver.hh"
#include "wrapping_integers.hh"
#include <limits>
#include <cstdint>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( message.RST ) {
    reader().set_error();
  }
  if ( message.SYN && !ISN.has_value() ) {
    ISN.emplace( message.seqno );
  }
  if ( ISN.has_value() ) {

    auto abs_seqno = message.seqno.unwrap( ISN.value(), writer().bytes_pushed() );
    if ( !( abs_seqno == 0 && !message.SYN && !message.payload.empty() ) ) {
      // special case: trying to override seqno occupied by SYN.
      uint64_t stream_index = abs_seqno ? abs_seqno - 1 : 0;
      reassembler_.insert( stream_index, message.payload, message.FIN );
    }
  }
}

TCPReceiverMessage TCPReceiver::send() const
{
  auto ack_seqno = ISN.has_value()
                     ? Wrap32::wrap( ISN.has_value() + writer().bytes_pushed() + writer().is_closed(), ISN.value() )
                     : std::optional<Wrap32>();
  uint64_t available_capacity
    = min( writer().available_capacity(), static_cast<uint64_t>( std::numeric_limits<uint16_t>::max() ) );
  return { ack_seqno, static_cast<uint16_t>( available_capacity ), reader().has_error() };
}
