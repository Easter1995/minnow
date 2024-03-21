#include "tcp_sender.hh"
#include "tcp_config.hh"
#include "tcp_sender_message.hh"

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // How many sequence numbers are outstanding?
  return NextByte2Sent - LastByteAcked;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return con_trans_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  /* Push bytes from the outbound stream */
  TCPSenderMessage msg_to_send;
  msg_to_send.payload = "";

  if ((SYN || reader().bytes_buffered() || (writer().is_closed() && FIN)) && NextByte2Sent - LastByteAcked < rwnd)
  {
    do {      
      //
      std::string_view data_view = reader().peek();
      msg_to_send.FIN = writer().is_closed();
      uint64_t spare_room = rwnd - (NextByte2Sent - LastByteAcked);
      // 不需要裁剪的数据部分
      while (!data_view.empty() && msg_to_send.sequence_length() + data_view.size() <= spare_room && !data_view.empty() && msg_to_send.payload.size() + data_view.size() <= TCPConfig::MAX_PAYLOAD_SIZE)
      {
        msg_to_send.payload += data_view;
        input_.reader().pop(data_view.size());
        data_view = reader().peek();
      }
      // 需要裁剪的部分
      if (!data_view.empty() && msg_to_send.payload.size() <= spare_room && msg_to_send.payload.size() < TCPConfig::MAX_PAYLOAD_SIZE)
      {
        uint64_t payload_size = min(spare_room , TCPConfig::MAX_PAYLOAD_SIZE);
        data_view = data_view.substr(0, payload_size - msg_to_send.payload.size());
        msg_to_send.payload += data_view;
        msg_to_send.FIN = false; // 被裁剪了，FIN肯定是false
      }
      msg_to_send.seqno = Wrap32::wrap(NextByte2Sent, isn_);  
      msg_to_send.SYN = SYN;
      msg_to_send.RST = reader().has_error() || writer().has_error();
      SYN = false;
      // 如果已经发送过FIN，那么FIN永远保持在false，避免进入循环重复发送FIN
      FIN = FIN ? !msg_to_send.FIN : false;
      transmit(msg_to_send);
      // 暂存刚发出还没有ack的segment
      NextByte2Sent += msg_to_send.sequence_length();
      outstanding_seq.push(msg_to_send);
      input_.reader().pop(outstanding_seq.front().payload.size());
      msg_to_send.payload.clear();
    }while (reader().bytes_buffered() && (NextByte2Sent - LastByteAcked < rwnd));
  } 
  else if ( rwnd == 0 && !has_trans_win0 ) {
    // 特殊情况rwnd为0，那么直接transmit一个byte
    msg_to_send.seqno = Wrap32::wrap( NextByte2Sent, isn_ );
    if ( !reader().is_finished() ) {
      msg_to_send.payload = reader().peek().substr( 0, 1 );
      input_.reader().pop( 1 );
    } else {
      msg_to_send.FIN = true;
    }
    outstanding_seq.push( msg_to_send );
    NextByte2Sent++;
    transmit( msg_to_send );
    has_trans_win0 = true;
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage empty_msg;
  empty_msg.payload = "";
  empty_msg.seqno = Wrap32::wrap(NextByte2Sent, isn_);
  empty_msg.SYN = false;
  empty_msg.FIN = false;
  empty_msg.RST = reader().has_error() || writer().has_error();
  return empty_msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if ( msg.RST ) { // 遇到异常情况
      writer().set_error();
      writer().close();
      return;
  }

  rwnd = msg.window_size;
  if (msg.ackno.has_value())
  {
    uint64_t ackno = msg.ackno->unwrap(isn_, LastByteAcked);
    if (ackno <= LastByteAcked || ackno > NextByte2Sent) // 冗余确认
      return;
    LastByteAcked = ackno; 
    // 去掉已经被确认的segment
    while (outstanding_seq.front().seqno.unwrap(isn_, LastByteAcked) < ackno) {
      outstanding_seq.pop();
    }
    con_trans_ = 0;
  }// 只要没有冗余确认，就重新开始计时
  total_time_ms_ = 0;
  RTO_ms_ = initial_RTO_ms_;
  if ( has_trans_win0 )
    has_trans_win0 = false;
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  if (!outstanding_seq.empty()) // 如果还有正在传的数据
  {
    total_time_ms_ += ms_since_last_tick;
    if (total_time_ms_ < RTO_ms_)
      return;
    else
    {
      total_time_ms_ = 0;
      con_trans_++;
      RTO_ms_ = rwnd == 0 ? RTO_ms_ : RTO_ms_*2;
      // 重传最老的一个没有ack的数据
      transmit(outstanding_seq.front());
    }
  }
}