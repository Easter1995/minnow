#include "reassembler.hh"
#include <cstdint>
#include <set>
#include <string>
#include <utility>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  firstUnacceptableIndex = firstUnassembledIndex + output_.writer().available_capacity();
  if (is_last_substring) {
    has_last_index = true;
  }
  // 如果data不为空、未重复、可接受
  if (!data.empty()) {
    if (first_index < firstUnacceptableIndex && first_index + data.size() > firstUnassembledIndex) {
      // 先对data进行预处理
      if (first_index < firstUnassembledIndex && first_index + data.size() > firstUnassembledIndex) {
        data = data.substr(firstUnassembledIndex - first_index);
        first_index = firstUnassembledIndex;
      }
      if (first_index + data.size() > firstUnacceptableIndex) {
        data = data.substr(0 , firstUnacceptableIndex - first_index);
        has_last_index = false;
      }
      // 先把data插入set
      // 首先set里面如果没有元素 则直接放入set
      if (unassembledData.empty()) {
        if (!data.empty()) {
          unassembledData.insert(make_pair(first_index, data));
          pending_num += data.size();
        }
      }
      else {
      // 解决data与元素有重叠的情况
        // data的前一个
        auto pre_obj = unassembledData.upper_bound({first_index , ""});
        // 不与data重叠的后一个
        auto next_obj = unassembledData.upper_bound({first_index + data.size() , ""});
        if (pre_obj != unassembledData.begin()) {
          pre_obj--;
        }
        else if (pre_obj == next_obj) {
          // 如果等式成立，表示set里面所有的元素都比data大
          next_obj++;
        }
        uint64_t start_index = first_index;
        string opeStr = data; // 进行操作的字符串
        // 截取操作进行规范，我们需要定义关键位置：理解这几个公式需要画图
        uint64_t leftDownIndex = 0, leftUpIndex = 0, rightDownIndex = 0, rightUpindex = 0;
        // 开始遍历分割字符串
        // pre_obj和next_obj之间是所有可能与data有重合的字符串
        for ( ; pre_obj != next_obj ; pre_obj++ ) {
          // 更新关键位置变量：
          leftDownIndex = min( pre_obj->first, start_index );
          leftUpIndex = min( pre_obj->first, start_index + opeStr.size() );
          rightDownIndex = max( pre_obj->first + pre_obj->second.size(), start_index );
          rightUpindex = max( pre_obj->first + pre_obj->second.size(), start_index + opeStr.size() );
          // 截取
          if ( leftDownIndex != leftUpIndex ) {
            // 表示opeStr的开头在pre_obj开头的前面，而结尾在pre_obj开头的后面，因此要截取前面的一段
            auto insertStr = opeStr.substr( 0, leftUpIndex - leftDownIndex );
            unassembledData.insert( { leftDownIndex, move( insertStr ) } );
          }
          // 更新start、opeStr
          // 删掉opeStr与pre_obj重合的一段且判断opeStr是否与pre_obj完全重合了
          opeStr = rightDownIndex == rightUpindex
                    ? ""
                    : opeStr.substr( rightDownIndex - start_index, rightUpindex - rightDownIndex );
          start_index = rightDownIndex;
          pending_num += leftUpIndex - leftDownIndex;
        }
        if ( !opeStr.empty() ) {
          pending_num += opeStr.size();
          unassembledData.insert( { rightDownIndex, move( opeStr ) } );
        }
      }
      // 看看set里面是否有目前的next byte
      while ( !unassembledData.empty() && first_index == firstUnassembledIndex ) {
        pending_num -= unassembledData.begin()->second.size();
        firstUnassembledIndex += unassembledData.begin()->second.size();
        // push
        output_.writer().push( move( unassembledData.begin()->second ) );
        unassembledData.erase( unassembledData.begin() );
        first_index = unassembledData.begin()->first; // 走到下一个元素
      }
    }
    
  }
  if (first_index >= firstUnacceptableIndex) {
    has_last_index = false;
  }
  if (has_last_index && unassembledData.empty()) {
    output_.writer().close();
    return;
  }
}

uint64_t Reassembler::bytes_pending() const
{
  return pending_num;
}