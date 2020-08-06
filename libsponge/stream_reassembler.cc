#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity)
    , _capacity(capacity)
    , buffer(capacity, 0)
    , bit_map(capacity, 0)
    , rpos(0)
    , _unassembled_bytes(0)
    , eof_received(false) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t start_index = max(rpos, index);                                             // starting index to be copied
    size_t end_index = min(rpos + _output.remaining_capacity(), index + data.size());  // ending inedx to be copied, I think

    for (size_t i = start_index; i < end_index; i++) {
        const size_t bi = i % _capacity;
        buffer[bi] = data[i - index];
        if (bit_map[bi] == 0) {
            bit_map[bi] = 1;
            _unassembled_bytes++;
        }
    }

    // Submit segment if possible
    size_t avail_end = rpos;  // ending index of available data to submit
    while (bit_map[avail_end % _capacity] && avail_end < rpos + _capacity)
        avail_end++;

    if (avail_end > rpos) {
        string data_submit(avail_end - rpos, 0);
        for (size_t i = rpos; i < avail_end; i++) {
            data_submit[i - rpos] = buffer[i % _capacity];
        }
        size_t written_bytes = _output.write(data_submit);
        for (size_t i = rpos; i < rpos + written_bytes; i++) {
            bit_map[i % _capacity] = 0;
        }
        rpos += written_bytes;
        _unassembled_bytes -= written_bytes;
    }

    // Handle EOF
    if (eof) {
        eof_received = true;
    }
    if (eof_received && empty()) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return unassembled_bytes() == 0; }
