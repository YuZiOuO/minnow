#include "reassembler.hh"
#include "debug.hh"
#include <algorithm>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  auto& writer = output_.writer();
  auto d_begin_i = first_index;                                 // data_begin_index
  auto d_end_i = first_index + data.size();                     // data_end_index
  auto pushed_i = writer.bytes_pushed();                        // pushed_index
  auto unacceptable_i = pushed_i + writer.available_capacity(); // unacceptable_index

  if ( is_last_substring ) {
    last_index_set = true;
    last_index_ = d_end_i;
  }
  if ( last_index_set && d_end_i > last_index_ ) {
    throw exception();
  }

  if ( d_end_i < pushed_i || d_begin_i >= unacceptable_i ) {
    // ignore already processed and unacceptable data
    return;
  }

  // truncate given data to [pushed,unacceptable)
  if ( d_begin_i < pushed_i ) {
    data = data.substr( pushed_i - d_begin_i );
    d_begin_i = pushed_i;
  }
  data = data.substr( 0UL, unacceptable_i > d_begin_i ? unacceptable_i - d_begin_i : 0UL );

  // iterater over cached data
  for (
    // TODO: Refactor with std::move
    auto it = cache_.begin(), next = std::next( it ); it != cache_.end() && it->index <= d_end_i;
    it = next, next = std::next( it ) ) {
    auto c_begin_i = it->index;                 // current_begin_index
    auto c_end_i = it->index + it->data.size(); // current_end_index

    if ( c_end_i < d_begin_i ) {
      continue;
    }

    if ( c_begin_i <= d_begin_i && c_end_i >= d_end_i ) {
      return; // ignore already cached data
    }

    // handle partly overlapping data
    if ( c_begin_i < d_begin_i && c_end_i <= d_end_i ) {
      data = it->data.substr( 0UL, d_begin_i - c_begin_i ) + data;
      d_begin_i = c_begin_i;
    } else if ( c_begin_i >= d_begin_i && c_end_i > d_end_i ) {
      data = data + it->data.substr( d_end_i - c_begin_i );
    }

    // overwrite(delete) overlapping data
    cache_.erase( it );
  }

  // cache new data
  auto i_pos = std::lower_bound( cache_.begin(), cache_.end(), d_begin_i );
  cache_.emplace( i_pos, data, d_begin_i );

  flush_();
}

void Reassembler::flush_()
{
  auto& writer = output_.writer();

  for ( auto pushed = writer.bytes_pushed(), available = writer.available_capacity();

        !cache_.empty() && available && pushed == cache_.cbegin()->index;

        pushed = writer.bytes_pushed(), available = writer.available_capacity() ) {
    auto& seg = *cache_.begin();
    auto len = seg.data.size();
    writer.push( seg.data.substr( 0UL, max( len, available ) ) );

    if ( len > available ) {
      seg.data = seg.data.substr( available, len );
      seg.index += available;
    } else {
      cache_.pop_front();
    }
  }

  if ( last_index_set && writer.bytes_pushed() == last_index_ ) {
    if ( !cache_.empty() ) {
      throw exception();
    }
    writer.close();
  }
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  uint64_t count = 0;
  for ( auto it : cache_ ) {
    count += it.data.size();
  }
  return count;
}
