#include "reassembler.hh"
#include "debug.hh"
#include <algorithm>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  CachedSegment newSeg{data,first_index,first_index + data.size()};
  auto pushed_i = output_.writer().bytes_pushed();                        // pushed_index
  auto unacceptable_i = pushed_i + output_.writer().available_capacity(); // unacceptable_index

  // Record if it's last string
  if ( is_last_substring ) {
    last_index_set = true;
    last_index_ = newSeg.end;
  }

  // Assert: no data after last_index_
  if ( last_index_set && newSeg.end > last_index_ ) {
    throw exception();
  }

  // ignore already processed and unacceptable data
  if ( newSeg.end < pushed_i || newSeg.begin >= unacceptable_i ) {
    return;
  }

  // truncate given data to [pushed,unacceptable)
  if ( newSeg.begin < pushed_i ) {
    data = data.substr( pushed_i - newSeg.begin );
    newSeg.begin = pushed_i;
  }
  data = data.substr(0UL,unacceptable_i - newSeg.begin);
  newSeg.end = unacceptable_i < newSeg.end ? unacceptable_i : newSeg.end;

  // Merge any overlapped segment into newSeg
  auto search_start = cache_.lower_bound(newSeg);
  if(search_start != cache_.begin()) search_start--;
  CachedSegment end{"",newSeg.end,std::string::npos};
  auto search_end = cache_.upper_bound(end);

  auto it = search_start;
  while(it != search_end){
    bool merged = merge(newSeg,*it);
    ++it;
    if(merged){ 
      cache_.erase(std::prev(it));
    }
  }

  // Insert merged segment
  cache_.emplace(newSeg);
  flush_();
}

void Reassembler::flush_()
{
  auto& writer = output_.writer();

  while(!cache_.empty()){
    auto available = writer.available_capacity();
    auto pushed = writer.bytes_pushed();

    auto seg = cache_.extract(cache_.begin());
    auto len = seg.value().data.size();

    // do not push those already pushed bytes
    if(seg.value().end <= pushed){
      continue;
    }
    auto push_start = pushed > seg.value().begin ? pushed - seg.value().begin : 0UL;

    if ( len > available ) {
      writer.push(seg.value().data.substr(push_start,available));
      seg.value().data = seg.value().data.substr( available, std::string::npos );
      seg.value().begin += available;
      break;
    } else {
      writer.push(seg.value().data);
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

/**
 * Merge the Segment 'from' to 'to'. After the merge, Segment 'from' totaly includes 'from'.
 * Return true if merge success, else makes no changes in 'to'.
 */
bool merge(CachedSegment& to,const CachedSegment& from){
  if((from.end < to.begin) or (from.begin > to.end)){
    return false;
  }

  if((from.begin >= to.end) and (from.end <= to.begin)){
    return true; // totally overlap
  }

  if(from.begin < to.begin){
    // left partial overlap
    to.data = from.data.substr(0UL,to.begin - from.begin) + to.data;
    to.begin = from.begin;
  }else{
    // right partial overlap
    to.data.append(from.data.substr(to.end - from.begin,std::string::npos));
    to.end = from.end;
  }
  return true;
}