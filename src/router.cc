#include "router.hh"
#include "debug.hh"

#include <algorithm>
#include <iostream>
#include <utility>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  auto [it, _] = routes_.try_emplace( prefix_length );

  auto& routes_in_this_length = ( *it ).second;

  auto existing_route = std::find_if( routes_in_this_length.begin(),
                                      routes_in_this_length.end(),
                                      [route_prefix]( struct Route r ) { return route_prefix == r.route_prefix; } );

  if ( existing_route != routes_in_this_length.end() ) {
    return;
  } else {
    struct Route new_route = {
      .route_prefix = route_prefix,
      .prefix_length = prefix_length,
      .next_hop = next_hop,
      .interface_num = interface_num,
    };
    routes_in_this_length.push_back( new_route );
  }
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  for ( const auto& src_interface_ptr : interfaces_ ) {
    auto& src_interface = *src_interface_ptr;
    auto& queue = src_interface.datagrams_received();
    while ( !queue.empty() ) {
      auto& pkt_incoming = queue.front();

      auto dst_addr = Address::from_ipv4_numeric(pkt_incoming.header.dst);
      auto route = find_route(dst_addr);

      if(route.has_value()){
        auto dst_interface = interface(route.value().interface_num);

        InternetDatagram pkt_outgoing(pkt_incoming);
        pkt_outgoing.header.ttl --;
        if(pkt_incoming.header.ttl && pkt_outgoing.header.ttl){ // Edge case: receive a pkt with ttl = 0
          pkt_outgoing.header.compute_checksum();
          dst_interface->send_datagram(pkt_outgoing,route.value().next_hop.value_or(dst_addr));
        }
      }
      // else: no route is matched, drop this packet.
      
      queue.pop();
    }
  }
}

std::optional<Router::Route> Router::find_route( const Address& destination )
{
  // traverse all routes, starting from routes of highest prefix length.
  for ( const auto& routes_in_some_prefix : routes_ ) {
    const auto& [prefix_length, routes] = routes_in_some_prefix;
    uint32_t mask = prefix_length ? (~(uint32_t)0) << (32 - prefix_length) : 0;
    uint32_t masked_destination = destination.ipv4_numeric() & mask;

    for ( const auto& route : routes ) {
      if ( masked_destination == (route.route_prefix & mask )) {
        return route;
      }
    }
  }

  return std::nullopt;
}
