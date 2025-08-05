#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"

#include <cstdint>
#include <deque>
#include <functional>
#include <optional>

class TCPSenderTimer
{
public:
  TCPSenderTimer( uint64_t expire_duration ) : expire_duration_( expire_duration ) {};
  bool started() { return enabled_; }
  bool goes_off() { return time_elapsed_ >= expire_duration_; }

  void enable() { enabled_ = true; }
  void tick( uint64_t time_elapsed )
  {
    if ( enabled_ ) {
      time_elapsed_ += time_elapsed;
    }
  }

  // Disable the Timer while setting a new expire duration.
  void reset( uint64_t expire_duration )
  {
    enabled_ = false;
    expire_duration_ = expire_duration;
    time_elapsed_ = 0;
  }

private:
  bool enabled_ = false;
  uint64_t time_elapsed_ = 0;
  uint64_t expire_duration_;
};

enum TCPSenderState
{
  CLOSED,      // SYN not sent
  HANDSHAKE,   // SYN sent and not ACKed
  STREAMING,   // SYN sent and ACKed
  ZERO_WINDOW, // ZERO WINDOW probe sent
  FINISHED,    // FIN sent
};

class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) ), isn_( isn ), initial_RTO_ms_( initial_RTO_ms ), timer_( initial_RTO_ms )
  {}

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // For testing: how many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // For testing: how many consecutive retransmissions have happened?
  const Writer& writer() const { return input_.writer(); }
  const Reader& reader() const { return input_.reader(); }
  Writer& writer() { return input_.writer(); }

private:
  Reader& reader() { return input_.reader(); }

  ByteStream input_;
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;

  /* Below are non-constant variables. */
  std::deque<TCPSenderMessage> outstandings_ = {};

  uint64_t current_RTO_ms_ = initial_RTO_ms_;
  TCPSenderTimer timer_;

  TCPSenderState state_ = CLOSED;

  // Seqno for the zero_window probe, this should be empty if not in ZERO_WINDOW state.
  std::optional<uint64_t> zw_probe_seqno = std::nullopt;

  uint64_t acked_seqno_ = 0;
  uint64_t sent_seqno_ = 0;
  uint16_t windows_size_ = 1;
};
