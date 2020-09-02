#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    _time_since_last_segment_received = 0;
    if (seg.header().rst) {
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        return;
    }

    // REPLAY_NON_OVERLAP
    // if the TCPReceiver complains that the segment didn't overlap the window and was unacceptable
    // the TCPConnection needs to make sure that a segment is sent back to the peer
    bool invalid_ack = false;
    // deliver ack to sender
    if (seg.header().ack) {
        invalid_ack = !_sender.ack_received(seg.header().ackno, seg.header().win);
    }

    if (invalid_ack && _sender.next_seqno_absolute() == 0) {  // ignore invalid ACK before connection established
        return;
    }

    size_t accepted_data_size_before = _receiver.unassembled_bytes() + _receiver.stream_out().bytes_written();
    bool segment_received = _receiver.segment_received(seg);
    size_t accepted_data_size =  // The size of accepted data that unseen before.
        _receiver.unassembled_bytes() + _receiver.stream_out().bytes_written() - accepted_data_size_before;

    // reset linger_after_streams_finish if remote EOF before inbound EOF
    if (_receiver.stream_out().eof() && !_sender.stream_in().eof()) {
        _linger_after_streams_finish = false;
    }

    bool shot = shot_segments();

    if (!shot &&
        (seg.header().fin                   // Deal with 2nd+ FIN. If peer resend FIN, connection should reply.
         || invalid_ack                     // should reply to invalid ack (described in REPLAY_NON_OVERLAP)
         || !segment_received               // should reply to non-overlab segment (described in REPLAY_NON_OVERLAP)
         || accepted_data_size > 0          // should reply when get new byte in window
         || seg.length_in_sequence_space()  // should reply when got not just ACK
         ) &&
        // When SYN is sent, connection ONLY expect SYN_ACK. ANY other segment will not get reply.
        _sender.bytes_in_flight() != _sender.next_seqno_absolute()) {
        _sender.send_empty_segment();
        shot_segments();
    }
}

bool TCPConnection::active() const {
    if (_receiver.stream_out().error() || _sender.stream_in().error()) {
        return false;
    }
    if (_receiver.stream_out().eof() && _sender.stream_in().eof() && _sender.bytes_in_flight() == 0 &&
        !has_new_ackno_to_be_sent()) {
        if (_linger_after_streams_finish && _time_since_last_segment_received < 10 * _cfg.rt_timeout) {
            return true;
        }
        return false;
    }
    return true;
}

size_t TCPConnection::write(const string &data) {
    size_t writen = _sender.stream_in().write(data);
    shot_segments();
    return writen;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _sender.tick(ms_since_last_tick);
    _time_since_last_segment_received += ms_since_last_tick;
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        reset();
    }

    if (_time_since_last_segment_received >= 10 * _cfg.rt_timeout && !active()) {
        _linger_after_streams_finish = false;
    }

    if (_sender.next_seqno_absolute() == 0) {  // should not send segment(SYN) when stream is not started.
        return;
    }
    shot_segments();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    shot_segments();
}

void TCPConnection::connect() { shot_segments(); }

// shot_segments take the segments from sender.segments_out and
// mark them with correct ack_no, window_size and ACK flag.
// If connection have new ack_no to be send but not sent yet,
// this function will ensure new ack_no is sent, along with payload or just a empty segment.
bool TCPConnection::shot_segments(bool fill_window) {
    bool shoot = false;
    if (fill_window) {
        _sender.fill_window();
    }
    while (active() && !_sender.segments_out().empty()) {
        auto seg = _sender.segments_out().front();
        // Add ackno to sending segments
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            _last_ackno_sent = _receiver.ackno();
        }
        seg.header().win = _receiver.window_size();
        _segments_out.push(seg);
        _sender.segments_out().pop();
        shoot = true;
        if (fill_window) {
            _sender.fill_window();
        }
    }
    if (!shoot && has_new_ackno_to_be_sent()) {  // Ensure the ack_no update will be sent.
        _sender.send_empty_segment();
        shot_segments();
        shoot = true;
    }
    return shoot;
}

bool TCPConnection::has_new_ackno_to_be_sent() const {  // receiver's ack_no is changed and not sent yet.
    return _receiver.ackno().has_value() &&
           (!_last_ackno_sent.has_value() || (_last_ackno_sent.value() != _receiver.ackno().value()));
}

void TCPConnection::reset() {
    TCPSegment segment;
    segment.header().seqno = _sender.next_seqno();
    segment.header().rst = true;
    _segments_out.push(segment);
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            reset();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
