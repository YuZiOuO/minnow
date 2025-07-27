#include "reassembler.hh"
#include "debug.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  auto& writer = output_.writer();
  uint64_t available = writer.available_capacity();
  uint64_t pushed = writer.bytes_pushed();
  uint64_t unacceptable_from = pushed + available;

  if(all_cached_ || first_index >= unacceptable_from || first_index + data.size() < pushed){
    return;
  }else{
    auto len = data.size();
    cache_.emplace( data.substr( pushed > first_index ? pushed - first_index : 0, min( len, unacceptable_from-first_index ) ),
                    max(pushed,first_index) );
    cached_index_.insert( first_index );
    if ( is_last_substring && len < available ) {
      all_cached_ = true;
    }
  }
  flush_();
}

void Reassembler::flush_(){
  auto& writer = output_.writer();

  for(
    auto pushed = writer.bytes_pushed(),
    available = writer.available_capacity();

    !cache_.empty() && available > 0 && pushed + 1 != cache_.top().index;

    pushed = writer.bytes_pushed(),
    available = writer.available_capacity()
  ){
    auto seg = cache_.top();
    cache_.pop();
    cached_index_.erase(seg.index);

    if(seg.index < pushed){
      continue;
    }

    auto len = seg.data.size();
    writer.push(seg.data.substr(0UL,max(len,available)));

    if(len > available){
      cache_.emplace(seg.data.substr(available,len),seg.index + available);
      cached_index_.insert(seg.index + available);
    }
  }

  if(all_cached_ && cache_.empty()){
    writer.close();
  }
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  std::priority_queue<CachedSegment> copy = cache_;
  uint64_t pending = 0;
  while(!copy.empty()){
    pending += copy.top().data.size();
    copy.pop();
  }
  return pending;
}
