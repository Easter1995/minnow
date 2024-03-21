#include "wrapping_integers.hh"
#include <asm-generic/errno.h>
#include <cstdint>

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32 { ((uint32_t)n + zero_point.raw_value_) };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // 求偏移量，是一个0——2^32-1之间的数
  uint64_t offset = raw_value_ - zero_point.raw_value_;
  // 求checkpoint和偏移量之间的距离
  uint64_t distance = checkpoint > offset ? checkpoint - offset : offset - checkpoint;
  // 若checkpoint比第一个wrap大，取的肯定是checkpoint左边的值
  offset += distance/(uint64_t)0x100000000*(uint64_t)0x100000000;
  uint64_t distance1 = checkpoint > offset ? checkpoint - offset : offset - checkpoint;
  uint64_t distance2 = offset + (uint64_t)0x100000000 > checkpoint ? offset + (uint64_t)0x100000000 - checkpoint : checkpoint - offset - (uint64_t)0x100000000;
  uint64_t res = distance1 < distance2 ? offset : offset + (uint64_t)0x100000000;
  return res;
}