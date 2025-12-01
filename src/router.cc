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

  auto [it, _] = routes_.try_emplace( route_prefix );

  auto routes_in_this_length = ( *it ).second;

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
  debug( "unimplemented route() called" );
}

std::optional<size_t> Router::find_interface( const Address& destination )
{
  // traverse all routes, starting from routes of highest prefix length.
  for ( const auto& routes_in_some_prefix : routes_ ) {
    const auto& [prefix_length, routes] = routes_in_some_prefix;
    uint32_t mask = ( (int32_t)1 << 31 ) >> prefix_length; // Undefined behavior 
    uint32_t masked_destination = destination.ipv4_numeric() & mask;

    for ( const auto& route : routes ) {
      if ( masked_destination == route.route_prefix & mask ) {
        return route.next_hop.has_value() ? find_interface( route.next_hop.value() ) : route.interface_num;
      }
    }
  }

  return std::make_optional<size_t>();
}
