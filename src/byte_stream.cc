#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : buffer_ {}, capacity_ { capacity } {}

// Assert data pushed has size less than available capacity
void Writer::push( string data )
{
  if(closed_){
    return;
  }

  auto available_capacity = this->available_capacity();
  auto len = data.size();
  if(available_capacity && len){
    if(available_capacity >= len){
      buffer_.push_back(std::move(data));
    }else{
      buffer_.push_back(data.substr(0UL,available_capacity));
    }
    auto pushed = min(len,available_capacity);
    pushed_count_ += pushed;
    buffer_count_ += pushed;
  }
}

void Writer::close()
{
  closed_ = true;
}

bool Writer::is_closed() const
{
  return closed_;
}

uint64_t Writer::available_capacity() const
{
  auto used = buffer_count_;
  if(capacity_ < buffer_count_){
    throw exception();
  }
  return capacity_ - used;
}

uint64_t Writer::bytes_pushed() const
{
  return pushed_count_;
}

string_view Reader::peek() const
{
  if ( buffer_.empty() ) {
    return string_view();
  } else {
    return string_view( &buffer_.begin()->at(0) , 1UL );
  }
}

void Reader::pop( uint64_t len )
{
  uint64_t poped = 0;

  for(auto it = buffer_.begin(),next = std::next(it);
  it != buffer_.end() && poped < len;
  it = next){
    auto bytes_to_be_poped = len - poped;
    auto current_len = it->size();

    if(current_len <= bytes_to_be_poped){
      poped += current_len;
      buffer_.erase(it);
    }else{
      poped += bytes_to_be_poped;
      *it = it->substr(bytes_to_be_poped);
    }
  }

  poped_count_ += poped;
  buffer_count_ -= poped;
}

bool Reader::is_finished() const
{
  return closed_ && buffer_.empty();
}

uint64_t Reader::bytes_buffered() const
{
  return buffer_count_;
}

uint64_t Reader::bytes_popped() const
{
  return poped_count_;
}
