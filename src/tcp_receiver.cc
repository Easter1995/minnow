#include "tcp_receiver.hh"
#include "tcp_receiver_message.hh"
#include "wrapping_integers.hh"
#include <cstdint>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  // 有错
  if (message.RST) {
    reader().set_error();
    return;
  }
  // 如果已经有初始值
  if (ISN.has_value()) {
    uint64_t unwrapped_seqno = message.seqno.unwrap(ISN.value(), unwrapped_ackno);
    // 注意ISN的index在message里面是0，但是在resembler里面第一个byte的index是0
    reassembler_.insert(unwrapped_seqno - 1, message.payload, message.FIN);
  }
  else {
    // 设置初始值
    if (message.SYN) {
      ISN = message.seqno;
      uint64_t unwrapped_seqno = message.seqno.unwrap(ISN.value(), unwrapped_ackno);
      reassembler_.insert(unwrapped_seqno, message.payload, message.FIN);
    }  
  }
  // write没结束：ackno就是push的字节数+ISN(index = 0)
  // write结束：ackno就算push的字节数+ISN(index = 0)+FIN(index = 最后一字节+1)
  unwrapped_ackno = writer().is_closed() ? writer().bytes_pushed() + 2 : writer().bytes_pushed() + 1; 
  return;
}

TCPReceiverMessage TCPReceiver::send() const
{
  TCPReceiverMessage message;
  message.ackno = ISN.has_value() ? Wrap32::wrap(unwrapped_ackno, ISN.value()) : ISN;
  message.RST = reader().has_error() || writer().has_error();
  // 注意window的大小不能超过uint16_MAX = 65535
  message.window_size = writer().available_capacity() > 65535 ? 65535 : writer().available_capacity();
  return message;
}