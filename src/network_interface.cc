#include <iostream>

#include "arp_message.hh"
#include "debug.hh"
#include "ethernet_frame.hh"
#include "exception.hh"
#include "helpers.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  // Address was requested
  auto record = arp_.find( next_hop.ipv4_numeric() );
  if ( record != arp_.end() ) {
    const auto& [ip, arp] = *record;

    // Check if arp was not replied
    constexpr size_t ARP_REQUEST_EXPIRE_TIME = 5 * 1000; // 5s
    if ( not arp.ethernet_address.has_value() ) {
      if ( time_ms - arp.timestamp <= ARP_REQUEST_EXPIRE_TIME ) {
        return;
      } else {
        // Not replied and last request expired
        goto send_arp_boardcast;
      }
    }

    // ARP was replied,
    // Check if expired
    constexpr size_t ARP_REPLY_EXPIRE_TIME = 30 * 1000; // 30s
    if ( time_ms - arp.timestamp > ARP_REPLY_EXPIRE_TIME ) {
      goto send_arp_boardcast;
    }

    // Directly send datagram if arp is valid
    transmit( {
      .header
      = { .dst = arp.ethernet_address.value(), .src = this->ethernet_address_, .type = EthernetHeader::TYPE_IPv4 },
      .payload = serialize( dgram ),
    } );
    return;
  }

send_arp_boardcast:
  // Cacheing this datagram and send arp message
  datagrams_sending_.emplace( next_hop.ipv4_numeric(), dgram );
  struct ARPMessage arp_msg = {
    .opcode = ARPMessage::OPCODE_REQUEST,
    .sender_ethernet_address = this->ethernet_address_,
    .sender_ip_address = this->ip_address_.ipv4_numeric(),
    .target_ip_address = next_hop.ipv4_numeric(),
  };
  transmit( {
    .header = { .dst = ETHERNET_BROADCAST, .src = this->ethernet_address_, .type = EthernetHeader::TYPE_ARP },
    .payload = serialize( arp_msg ),
  } );

  if ( record != arp_.end() ) {
    auto& [ip, arp] = *record;
    arp.ethernet_address.reset();
    arp.timestamp = time_ms;
  } else {
    struct ARPRecord r = { .ethernet_address = {}, .timestamp = time_ms };
    arp_.emplace( next_hop.ipv4_numeric(), r );
  }
  return;
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( EthernetFrame frame )
{
  if ( frame.header.dst != this->ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST ) {
    return; // Ignoring packet not destined for this interface
  }

  // Try parsing as an IP datagram
  InternetDatagram dgram;
  if ( parse( dgram, clone( frame ).payload ) ) {
    datagrams_received_.push( dgram );
    return;
  }

  // Try parsing as a ARP message
  struct ARPMessage arp_msg;
  if ( parse( arp_msg, clone( frame ).payload ) ) {
    // Override or construct record
    auto record = arp_.find( arp_msg.sender_ip_address );
    if ( record != arp_.end() ) {
      auto& [ip, arp] = *record;
      arp.ethernet_address = arp_msg.sender_ethernet_address, arp.timestamp = time_ms;
    } else {
      struct ARPRecord r = { .ethernet_address = arp_msg.sender_ethernet_address, .timestamp = time_ms };
      arp_.emplace( arp_msg.sender_ip_address, r );
    }

    // If it's a request, send reply
    if ( arp_msg.opcode == ARPMessage::OPCODE_REQUEST
         && arp_msg.target_ip_address == this->ip_address_.ipv4_numeric() ) {
      struct ARPMessage reply_arp_msg = {
        .opcode = ARPMessage::OPCODE_REPLY,
        .sender_ethernet_address = this->ethernet_address_,
        .sender_ip_address = this->ip_address_.ipv4_numeric(),
        .target_ethernet_address = arp_msg.sender_ethernet_address,
        .target_ip_address = arp_msg.sender_ip_address,
      };
      transmit( { .header = { .dst = arp_msg.sender_ethernet_address,
                              .src = this->ethernet_address_,
                              .type = EthernetHeader::TYPE_ARP },
                  .payload = serialize( reply_arp_msg ) } );
    }

    // If it's a reply, check if a datagram to this address is queueing
    if ( arp_msg.opcode == ARPMessage::OPCODE_REPLY ) {
      auto it = datagrams_sending_.find( arp_msg.sender_ip_address );
      if ( it != datagrams_sending_.end() ) {
        transmit({
          .header = {
            .dst = arp_msg.sender_ethernet_address,
            .src = arp_msg.target_ethernet_address,
            .type = EthernetHeader::TYPE_IPv4,
          },
          .payload = serialize(it->second),
        });
        datagrams_sending_.erase( it );
      }
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  time_ms += ms_since_last_tick;
}