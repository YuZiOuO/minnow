#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : buffer_ {}, capacity_ { capacity } {}

void Writer::push( string data )
{
  auto available_capacity = this->available_capacity();
  auto len = data.size();
  if ( !closed_ && len && available_capacity ) {
    auto bytes_to_be_pushed = min(len,available_capacity);
    buffer_.insert(buffer_.end(),data.begin(),std::next(data.begin(),bytes_to_be_pushed));
    pushed_count_ += bytes_to_be_pushed;
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
    return string_view( &buffer_.at( 0 ), 1UL );
  }
}

void Reader::pop( uint64_t len )
{
  auto bytes_to_be_poped = min(buffer_.size(),len);
  buffer_.erase(buffer_.begin(),std::next(buffer_.begin(),bytes_to_be_poped));
  poped_count_ += bytes_to_be_poped;
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
