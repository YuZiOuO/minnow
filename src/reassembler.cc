#include "reassembler.hh"
#include "debug.hh"
#include <algorithm>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring)
{
  // TODO: organize code structure
  auto& writer = output_.writer();
  uint64_t pushed = writer.bytes_pushed();
  uint64_t unacceptable_index = pushed + writer.available_capacity();
  if(is_last_substring){
    last_index_set = true;
    last_index_ = first_index + data.size();
  }

  if(last_index_set && first_index + data.size() > last_index_){
    throw exception();
  }

  if( (first_index + data.size() < pushed) || (first_index >= unacceptable_index)){
    return;
  }else{
    if(first_index < pushed){
      data = data.substr(pushed - first_index);
      first_index = pushed;
    }
    data = data.substr(0UL,unacceptable_index > first_index ? unacceptable_index - first_index : 0UL );

    auto data_end_index = first_index + data.size();
    for(
      //TODO: Refactor with std::move
      auto pos = std::lower_bound(cache_.begin(),cache_.end(),first_index),next = std::next(pos);
      pos != cache_.end() && pos->index <= data_end_index;
      pos = next
    ){
      auto current_start = pos->index;
      auto current_end = pos->index + pos->data.size();
      
      // if(current_start <= first_index && current_end >= data_end_index){
      //   return;
      // }

      // if(current_start < first_index && current_end <= data_end_index){
      //   data = pos->data.substr(0UL + first_index - current_start);
      //   first_index = current_start;
      //   cache_.erase(pos);
      //   break;
      // }

      if(current_start >= first_index && current_end > data_end_index){
        data = data + pos->data.substr(data_end_index - current_start);
        cache_.erase(pos);
        break;
      }

      if(current_end <= data_end_index){
        cache_.erase(pos);
        break;
      }
    }

    auto i_pos = std::lower_bound(cache_.begin(),cache_.end(),first_index);
    cache_.emplace(i_pos,data,first_index);
  }

  flush_();
}

void Reassembler::flush_(){
  auto& writer = output_.writer();

  for(
    auto pushed = writer.bytes_pushed(),
    available = writer.available_capacity();

    !cache_.empty() && available && pushed == cache_.cbegin()->index;

    pushed = writer.bytes_pushed(),
    available = writer.available_capacity()
  ){
    auto& seg = *cache_.begin();
    auto len = seg.data.size();
    writer.push(seg.data.substr(0UL,max(len,available)));

    if(len > available){
      seg.data = seg.data.substr(available,len);
      seg.index += available;
    }else{
      cache_.pop_front();
    }
  }

  // TODO:不能保证cache传来的数据size刚好是last_index_
  if(last_index_set && writer.bytes_pushed() == last_index_ && cache_.empty()){
    writer.close();
  }
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  uint64_t count = 0;
  for(auto it:cache_){
    count += it.data.size();
  }
  return count;
}
