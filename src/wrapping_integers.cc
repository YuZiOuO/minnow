#include "wrapping_integers.hh"
#include "debug.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32 { static_cast<uint32_t>(n) + zero_point.raw_value_ };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  constexpr uint64_t UINT32_RANGE = (1UL << 32);
  
  // seqno_major and minor represents the direct neighbor of the checkpoint.
  uint64_t prefix = checkpoint - static_cast<uint32_t>(checkpoint);
  uint32_t zero_offset = this->raw_value_ - zero_point.raw_value_;
  uint64_t seqno_major = prefix + zero_offset;
  uint64_t seqno_minor = seqno_major > checkpoint ? seqno_major - UINT32_RANGE : seqno_major + UINT32_RANGE;

  // compare which one is nearer
  int64_t major_s = static_cast<int64_t>(seqno_major);
  int64_t minor_s = static_cast<int64_t>(seqno_minor);
  int64_t checkpoint_s = static_cast<int64_t>(checkpoint);
  //special case: different sign, marking the minor has overflowed.
  if((major_s ^ minor_s) < 0){ 
    return seqno_major; 
  }
  return abs(major_s - checkpoint_s) < abs(minor_s - checkpoint_s) ? seqno_major:seqno_minor;
}
