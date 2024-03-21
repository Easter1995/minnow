#include "byte_stream.hh"
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

bool Writer::is_closed() const
{
  return is_closed_;
}

void Writer::push( string data )
{
  if ( available_capacity() == 0 || data.empty() ) {
    return;
  }
  uint64_t writeDatalen = min( data.length(), available_capacity() );
  data = data.substr( 0, writeDatalen );
  bytes_pushed_num = bytes_pushed_num + writeDatalen;
  buffer_bytes_num = buffer_bytes_num + writeDatalen;
  // load data into queue
  stream.emplace_back( move( data ) );
  // add a string view which we will use in the reader part
  // view_queue_.emplace_back( stream.back() );
  // 最开始stream_view没有初始化(为空)，这个时候需要将它归位到首位字节
  if ( stream_view.empty() ) {
    stream_view = stream.front();
  }
  return;
}

void Writer::close()
{
  is_closed_ = true;
}

uint64_t Writer::available_capacity() const
{
  // writer处计算available_capacity的方法是：
  // 由一个计数器得到当前收发的数量和（发送+，接收-），通过capacity- 目前存在buffer里面的 即可得到available
  return capacity_ - buffer_bytes_num;
}

uint64_t Writer::bytes_pushed() const
{
  // Total number of bytes cumulatively pushed to the stream
  return bytes_pushed_num;
}

bool Reader::is_finished() const
{
  // Is the stream finished (closed and fully popped)? 当write==read量的时候，就说明完全的pop出去了
  return is_closed_ && buffer_bytes_num == 0;
}

uint64_t Reader::bytes_popped() const
{
  // Total number of bytes cumulatively popped from stream
  return bytes_popped_num;
}

string_view Reader::peek() const
{
  // Peek at the next bytes in the buffer
  if ( stream_view.empty() ) {
    return {};
  }
  return stream_view;
}

void Reader::pop( uint64_t len )
{
  // Remove `len` bytes from the buffer
  auto n = min( len, buffer_bytes_num );
  while ( n > 0 ) {
    auto sz = stream_view.length();
    if ( n <= sz ) {
      stream_view.remove_prefix( n );
      buffer_bytes_num -= n;
      bytes_popped_num += n;
      // 检测是否stream_view空了
      if ( stream_view.empty() ) {
        stream.pop_front();
        if ( stream.empty() ) {
          stream_view = ""; // 将stream_view置空
        } else {
          stream_view = stream.front();
        }
        // stream_view = stream.empty() ? "" : stream.front();
      }
      return;
    }
    // view_queue_.pop_front();
    stream.pop_front();
    // stream_view = stream.empty() ? "" : stream.front();
    if ( stream.empty() ) {
      stream_view = ""; // 将stream_view置空
    } else {
      stream_view = stream.front();
    }
    n -= sz;
    buffer_bytes_num -= sz;
    bytes_popped_num += sz;
  }
}

uint64_t Reader::bytes_buffered() const
{
  // Number of bytes currently buffered (pushed and not popped)
  return buffer_bytes_num;
}