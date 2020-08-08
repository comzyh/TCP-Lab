#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _total_flying_size(0)
    , _receive_window_size(0)  // initial remote receive window size is 0.
    , _timer_countdown(0)
    , _timer_started(false)
    , _RTO(_initial_retransmission_timeout)
    , _consecutive_retransmissions(0)
    , _latest_abs_ackno(0)  // the ackno of SYN should be 1, so 0 for initialization is fine
    , _fin_abs_seq(0)       //
{}

uint64_t TCPSender::bytes_in_flight() const { return _total_flying_size; }

void TCPSender::fill_window() {
    // The size of data stream chunk. should counsider the following parameters:
    // - TCP payload size should not exceed the MAX_PAYLOAD_SIZE = 1452
    // - The receiver's receive window
    // - If SYN is set, send_size should be 0.
    // - The remaining size
    size_t send_size = TCPConfig::MAX_PAYLOAD_SIZE;
    TCPSegment segment;

    // set segment seqno before modify internal data structure
    segment.header().seqno = next_seqno();

    if (_next_seqno == 0) {  // SYN not sent yet
        send_size = 0;
        segment.header().syn = true;
        _next_seqno++;
    }

    send_size = min(send_size, _receive_window_size);  // adjust remote receive window

    segment.payload() = Buffer(_stream.read(send_size));
    send_size = min(send_size, segment.payload().size());
    _next_seqno += send_size;

    // No data needs to be send and receiver have window to receive FIN
    if (_stream.eof() && _fin_abs_seq == 0 && segment.length_in_sequence_space() < _receive_window_size) {
        segment.header().fin = true;
        _fin_abs_seq = _next_seqno;
        _next_seqno++;
    }
    _total_flying_size += segment.length_in_sequence_space();

    if (segment.length_in_sequence_space() == 0) {
        return;
    }
    _flying_segments.push(segment);
    _segments_out.push(segment);  // send the segment
    _receive_window_size -= segment.length_in_sequence_space();

    // start the timer if not started
    if (!_timer_started) {
        _timer_started = true;
        _timer_countdown = _RTO;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
//! \returns `false` if the ackno appears invalid (acknowledges something the TCPSender hasn't sent yet)
bool TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t abs_ackno = unwrap(ackno, _isn, _next_seqno);
    // received ackno that not sent yet.
    // note that abs_ackno is the seq_no that the receiver want to receive, so we use ">".
    if (abs_ackno > _next_seqno) {
        return false;
    }

    // update the recv window size
    _receive_window_size = window_size;

    if (abs_ackno <= _latest_abs_ackno) {  // ack already acked segment
        return true;
    }
    // If ack some data that never benn acked:
    // (a) Set the RTO back to its \initial value."
    _RTO = _initial_retransmission_timeout;
    // (b) Restart the retransmission timer so that it will expire after RTO milliseconds
    _timer_countdown = _RTO;
    // (c) Reset the count of "consecutive retransmissions" back to zero.
    _consecutive_retransmissions = 0;

    // Deal with bytes_in_flight
    auto new_acked_data_size = abs_ackno - _latest_abs_ackno;
    // count is in "sequence space,"  SYN and FIN each count for one byte
    // if (_latest_abs_ackno == 0) {  // ack of SYN
    //     new_acked_data_size--;
    // }
    // if (abs_ackno > _fin_abs_seq) {  // ack of FIN
    //     new_acked_data_size--;
    // }
    _total_flying_size -= new_acked_data_size;

    // Stop the retransmission timer if all outstanding data has been acknowledged,
    if (_total_flying_size == 0) {
        _timer_started = false;
    }

    // Clean the retransmission queue
    while (!_flying_segments.empty()) {
        auto &seg = _flying_segments.front();
        uint64_t abs_seq = unwrap(seg.header().seqno, _isn, _next_seqno);
        if (abs_seq + seg.length_in_sequence_space() <= abs_ackno) {  // this segment is fully acked
            _flying_segments.pop();
        } else {
            break;
        }
    }
    // Update _latest_abs_ackno
    _latest_abs_ackno = abs_ackno;

    return true;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    // if timer is not started, do nothing.
    if (!_timer_started) {
        return;
    }
    _timer_countdown -= ms_since_last_tick;
    if (_timer_countdown > 0) {  // timer not expired, do nothing.
        return;
    }

    // (a) retransmit the earliest segment that hasn't been fully acknowledged
    _segments_out.push(_flying_segments.front());

    // (b) if windows size is nonzero:
    if (_receive_window_size > 0) {
        _consecutive_retransmissions++;  // track of the number of consecutive retransmissions,
        _RTO *= 2;                       // exponential backoff
    }
    // (c) start the retransmission timer
    _timer_countdown = _RTO;
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment segment;
    segment.header().seqno = next_seqno();
    _segments_out.push(segment);
    // doesn't need to be kept track track of the number of consecutive retransmissions
}
