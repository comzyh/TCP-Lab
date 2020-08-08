#include "wrapping_integers.hh"

#include <iostream>

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) { return WrappingInt32{uint32_t(n + isn.raw_value())}; }

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    WrappingInt32 wrap_checkpoint = wrap(checkpoint, isn);
    uint32_t diff = n - wrap_checkpoint;
    // This case should never be persent. Unforunately, it's include in test cases.
    // This case occurs when diff < 0 and checkpoint < -diff, 
    // which means we are just starting the TCP connection, but the absolute ackno has exceed 2^31,
    // which means we have over 2GB data on flying, and TCP 32bit acno is not enough.
    // considering this case we can ONLY deal with the first 1 or 2 packet but not always.
    if (diff & 0x80000000 && diff + checkpoint <= UINT32_MAX) {
        return checkpoint + diff;
    }
    // this can deal with both diff > 0 and diff < 0 scenarios.
    return checkpoint + static_cast<int32_t>(diff);
}
