#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : buffer_ {}, capacity_ { capacity } {}

void Writer::push( string data )
{
  auto available_capacity = this->available_capacity();
  if ( !closed_ && available_capacity ) {
    auto it = data.begin();
    while ( buffer_.size() < capacity_ && it != data.end() ) {
      buffer_.push_back( *it );
      it++;
      pushed_count_ ++;
    }
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
  size_t used = buffer_.size();
  if ( used >= capacity_ ) {
    return 0UL;
  } else {
    return capacity_ - used;
  }
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
    return string_view(&buffer_.at( 0 ), 1UL);
  }
}

void Reader::pop( uint64_t len )
{
  uint64_t index = 0;
  while ( index != len && !buffer_.empty() ) {
    buffer_.pop_front();
    index++;
    poped_count_++;
  }
}

bool Reader::is_finished() const
{
  return closed_ && buffer_.empty();
}

uint64_t Reader::bytes_buffered() const
{
  return buffer_.size();
}

uint64_t Reader::bytes_popped() const
{
  return poped_count_;
}
