#include "wrapping_integers.hh"
#include "debug.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32 { static_cast<uint32_t>(n) + zero_point.raw_value_ };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint64_t prefix = checkpoint - static_cast<uint32_t>(checkpoint);
  uint32_t zero_offset = this->raw_value_ - zero_point.raw_value_;
  constexpr uint64_t power32 = (1UL << 32);
  uint64_t seqno_major = prefix + zero_offset;
  uint64_t seqno_minor = seqno_major > checkpoint ? seqno_major - power32:seqno_major + power32;
  if(seqno_major == checkpoint || (seqno_major < power32 && seqno_major > checkpoint)){
    return seqno_major;
  }

  // FIXME: it happends that Wrap(1<<63-1).unwrap(0,1<<63-2) was correctly handled,
  // but not considered.
  if(seqno_major > checkpoint){
    return seqno_major - checkpoint > checkpoint - seqno_minor ? seqno_minor : seqno_major;
  }else{
    return checkpoint - seqno_major > seqno_minor - checkpoint ? seqno_minor : seqno_major;
  }
}
