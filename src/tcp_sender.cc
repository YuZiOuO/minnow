#include "tcp_sender.hh"
#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
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
  return static_cast<uint64_t>( log2( (double)current_RTO_ms_ / (double)initial_RTO_ms_ ) );
}

void TCPSender::push( const TransmitFunction& transmit )
{

  auto seqno_in_flight = sent_abs_seqno_ - acked_abs_seqno_;
  auto window_size = static_cast<uint64_t>( windows_size_ );
  auto seqno_available = window_size > seqno_in_flight ? window_size - seqno_in_flight : 0;
  if ( !window_size ) {
    seqno_available = 1;
  }

  if ( SYN_sent && !SYN_ACKED ) {
    // Intention: Get the correct windows size, which the first SYN/ACK will return.
    return;
  }
  if ( zero_window_probe_sent ) {
    return;
  }
  
  while ( seqno_available ) {
    TCPSenderMessage msg = make_empty_message();

    msg.SYN = !SYN_sent;
    while ( msg.sequence_length() < seqno_available && reader().bytes_buffered()
            && msg.payload.size() < TCPConfig::MAX_PAYLOAD_SIZE ) {
      msg.payload.append( reader().peek() );
      reader().pop( 1UL );
    }
    msg.FIN = reader().is_finished() && msg.sequence_length() < seqno_available && !FIN_sent;

    SYN_sent = SYN_sent || msg.SYN;
    FIN_sent = FIN_sent || msg.FIN;

    if ( !msg.payload.empty() || msg.SYN || msg.FIN || msg.RST ) {
      transmit( msg );

      if ( !window_size ) {
        zero_window_probe_sent = true;
      }

      outstandings_.push_back( msg );
      sent_abs_seqno_ += msg.sequence_length();
      if ( !timer_.started() ) {
        timer_.reset( current_RTO_ms_ );
        timer_.enable();
      }
    }

    // Update loop helper var
    seqno_available -= msg.sequence_length();
    seqno_available = reader().bytes_buffered() ? seqno_available : 0;
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  return { .seqno = Wrap32::wrap( sent_abs_seqno_, isn_ ),
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
  auto msg_acked_abs_seqno = msg.ackno.value().unwrap( isn_, acked_abs_seqno_ );
  if ( msg_acked_abs_seqno > sent_abs_seqno_ ) {
    return;
  }

  if ( SYN_sent && msg_acked_abs_seqno > acked_abs_seqno_ ) {
    SYN_ACKED = true;
  }

  if ( msg_acked_abs_seqno > acked_abs_seqno_ ) {
    // Check Outsandings
    while ( !outstandings_.empty() ) {
      const auto& top = *outstandings_.begin();
      const auto current_abs_seqno_should_be_acked
        = top.seqno.unwrap( isn_, acked_abs_seqno_ ) + top.sequence_length(); // -1:abs_seqno.begin()
      if ( msg_acked_abs_seqno >= current_abs_seqno_should_be_acked ) {
        if ( top.payload.size() == 1 ) {
          zero_window_probe_sent = false;
        }
        outstandings_.pop_front();
      } else {
        break;
      }
    }

    current_RTO_ms_ = initial_RTO_ms_;
    acked_abs_seqno_ = msg_acked_abs_seqno;

    timer_.reset( current_RTO_ms_ );
    if ( !outstandings_.empty() ) {
      timer_.enable();
    }
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  time_alive_ += ms_since_last_tick;

  if ( !outstandings_.empty() && timer_.started() ) {
    timer_.tick( ms_since_last_tick );
    if ( timer_.goes_off() ) {
      if ( !zero_window_probe_sent ) {
        current_RTO_ms_ *= 2;
      }
      timer_.reset( current_RTO_ms_ );
      timer_.enable();
      transmit( *outstandings_.begin() );
    }
  }
}
