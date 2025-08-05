#include "tcp_sender.hh"
#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <sys/types.h>

using namespace std;

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::sequence_numbers_in_flight() const
{

  return sent_seqno_ - acked_seqno_;
}

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::consecutive_retransmissions() const
{
  return static_cast<uint64_t>( log2( (double)current_RTO_ms_ / (double)initial_RTO_ms_ ) );
}

void TCPSender::push( const TransmitFunction& transmit )
{
  if ( state_ == HANDSHAKE || state_ == ZERO_WINDOW || state_ == FINISHED ) {
    return;
  }

  auto seqno_in_flight = sent_seqno_ - acked_seqno_;
  auto window_size = static_cast<uint64_t>( windows_size_ );
  auto seqno_available = window_size > seqno_in_flight ? window_size - seqno_in_flight : 0;

  // Special case: if windows size == 0,construct ZERO_WINDOW probe.
  if ( !window_size ) {
    seqno_available = 1;
  }

  while ( seqno_available ) {
    // Construct sender message
    TCPSenderMessage msg = make_empty_message();
    msg.SYN = ( state_ == CLOSED );
    while ( msg.sequence_length() < seqno_available && reader().bytes_buffered()
            && msg.payload.size() < TCPConfig::MAX_PAYLOAD_SIZE ) {
      msg.payload.append( reader().peek() );
      reader().pop( 1UL );
    }
    msg.FIN = reader().is_finished() && msg.sequence_length() < seqno_available;

    // Abort empty packet
    if ( msg.payload.empty() && !( msg.SYN || msg.FIN || msg.RST ) ) {
      break;
    }

    // Transmit and push to outstandings
    transmit( msg );
    outstandings_.push_back( msg );

    // Update state machine
    if ( msg.SYN ) {
      state_ = HANDSHAKE;
    }
    if ( msg.FIN ) {
      state_ = FINISHED;
    }
    if ( !window_size ) {
      state_ = ZERO_WINDOW;
    }
    sent_seqno_ += msg.sequence_length();
    if ( !timer_.started() ) {
      timer_.reset( current_RTO_ms_ );
      timer_.enable();
    }

    // Update seqno available to support multiple packet in a push
    seqno_available -= msg.sequence_length();
    seqno_available = reader().bytes_buffered() ? seqno_available : 0;
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  return { .seqno = Wrap32::wrap( sent_seqno_, isn_ ),
           .SYN = false,
           .payload = string(),
           .FIN = false,
           .RST = reader().has_error() };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if ( msg.RST ) {
    writer().set_error();
  }
  windows_size_ = msg.window_size;

  // Check if the ACK packet have set ackno.If so,decode the ackno and check validity.
  bool ACK = msg.ackno.has_value();
  if ( !ACK ) {
    return;
  }
  auto msg_acked_seqno = msg.ackno.value().unwrap( isn_, acked_seqno_ );
  if ( msg_acked_seqno > sent_seqno_ ) {
    return;
  }

  // Handle ACK
  if ( msg_acked_seqno > acked_seqno_ ) {
    if ( state_ == HANDSHAKE ) {
      state_ = STREAMING;
    }

    // Traverse in-flight packets
    while ( !outstandings_.empty() ) {
      // Compute seqno that should be acked for the current in-flight packet
      const auto& pkt = *outstandings_.begin();
      const auto pkt_seqno_to_be_acked = pkt.seqno.unwrap( isn_, acked_seqno_ ) + pkt.sequence_length();
      if ( msg_acked_seqno >= pkt_seqno_to_be_acked ) {
        if ( state_ == ZERO_WINDOW && pkt.payload.size() == 1 ) {
          state_ = STREAMING; // TODO: consider this condition
        }
        outstandings_.pop_front();
      } else {
        break;
      }
    }

    // Update State Machine
    current_RTO_ms_ = initial_RTO_ms_;
    acked_seqno_ = msg_acked_seqno;
    timer_.reset( current_RTO_ms_ );
    if ( !outstandings_.empty() ) {
      timer_.enable();
    }
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  time_alive_ += ms_since_last_tick;

  if ( timer_.started() ) {
    timer_.tick( ms_since_last_tick );
    if ( timer_.goes_off() ) {
      current_RTO_ms_ = state_ == ZERO_WINDOW ? current_RTO_ms_ : 2 * current_RTO_ms_;
      timer_.reset( current_RTO_ms_ );
      timer_.enable();
      // assert(!outstandings_.empty());
      transmit( *outstandings_.begin() );
    }
  }
}
