#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

bool TCPReceiver::segment_received(const TCPSegment &seg) {
  uint64_t old_abs_ackno = 0;
  if (isn.has_value()) {
    old_abs_ackno = abs_ackno();
  }
  // Deal with the first SYN
  if (seg.header().syn && !isn.has_value()) {
    isn = make_optional<WrappingInt32>(seg.header().seqno);
  }

  if (!isn.has_value()) {
    return false;
  }

  // absolute sequence number
  uint64_t checkpoint = _reassembler.stream_out()
                            .bytes_written(); // bytes_written is a little bit small, but good enough for checkpoint
  uint64_t abs_seq = unwrap(seg.header().seqno, isn.value(), checkpoint);

  uint64_t old_window_size = window_size();

  if (!(abs_seq < old_abs_ackno + old_window_size && abs_seq + seg.length_in_sequence_space() > old_abs_ackno)) {
    return false; // Not overlap with the window
  }
  bool all_fill = abs_seq + seg.length_in_sequence_space() < old_abs_ackno + old_window_size;

  if (all_fill && seg.header().fin) { // only when fin also fall in the window
    fin_received = true;
  }

  uint64_t stream_indices = abs_seq > 0 ? abs_seq - 1 : 0;
  std::string payload(seg.payload().copy());
  _reassembler.push_substring(payload, stream_indices, fin_received);

  return true;
}

uint64_t TCPReceiver::abs_ackno() const {
  // I write the StreamReassembler,
  // I know that the reassembler will write to outoput stream once there are something can be sumbmit.
  return _reassembler.stream_out().bytes_written() + 1 + (fin_received ? 1 : 0);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
  if (!isn.has_value()) {
    return nullopt;
  }
  return make_optional<WrappingInt32>(wrap(abs_ackno(), isn.value()));
}

size_t TCPReceiver::window_size() const { return _reassembler.stream_out().remaining_capacity(); }
