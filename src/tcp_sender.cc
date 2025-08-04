#include "tcp_sender.hh"
#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_receiver_message.hh"
#include "wrapping_integers.hh"
#include <cmath>
#include <cstdint>
#include <sys/types.h>

using namespace std;

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::sequence_numbers_in_flight() const
{

  return sent_abs_seqno_ - acked_abs_seqno_;
}

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::consecutive_retransmissions() const
{
  return static_cast<uint64_t>( log2( (double)RTO / (double)initial_RTO_ms_ ) );
}

void TCPSender::push( const TransmitFunction& transmit )
{

  // TODO: use seq_len API
  auto bytes_to_be_sent = std::min(
    { static_cast<uint64_t>( windows_size_ ), reader().bytes_buffered(), TCPConfig::MAX_PAYLOAD_SIZE } );
  if ( !windows_size_ ) {
    bytes_to_be_sent = 1;
  }
  std::string payload;
  payload.reserve( bytes_to_be_sent );

  while ( payload.size() != bytes_to_be_sent ) {
    payload.append( reader().peek() );
    reader().pop( 1UL );
  }

  TCPSenderMessage msg = { .seqno = Wrap32::wrap( acked_abs_seqno_, isn_ ),
                           .SYN = !SYN_sent,
                           .payload = payload,
                           .RST = reader().has_error() };
  if ( !SYN_sent ) {
    SYN_sent = true;
  }

  if ( !msg.payload.empty() || msg.SYN || msg.FIN || msg.RST ) {
    transmit( msg );
    outstandings_.push( msg );
    sent_abs_seqno_ += msg.sequence_length();
    if ( !timer_ ) {
      timer_ = time_alive_;
    }
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  return { .seqno = Wrap32::wrap( sent_abs_seqno_, isn_ ) };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  auto msg_acked_abs_seqno = msg.ackno->unwrap( isn_, acked_abs_seqno_ );

  windows_size_ = msg.window_size;
  if ( msg.RST ) {
    reader().set_error();
  }

  if ( msg_acked_abs_seqno > acked_abs_seqno_ ) {
    // Check Outsandings
    while ( !outstandings_.empty() ) {
      const auto& top = outstandings_.top();
      const auto current_abs_seqno
        = top.seqno.unwrap( isn_, acked_abs_seqno_ ) - 1 + top.sequence_length(); // -1:abs_seqno.begin()
      if ( msg_acked_abs_seqno > current_abs_seqno ) {
        outstandings_.pop();
      } else {
        break;
      }
    }

    RTO = initial_RTO_ms_;
    acked_abs_seqno_ = msg_acked_abs_seqno;
  }

  if ( outstandings_.empty() ) {
    timer_ = 0;
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  time_alive_ += ms_since_last_tick;

  if ( !outstandings_.empty() && timer_ ) {
    timer_ += ms_since_last_tick;
    if ( time_alive_ - timer_ > RTO ) {
      timer_ = time_alive_;
      RTO *= 2;
      transmit( outstandings_.top() );
    }
  }
}
