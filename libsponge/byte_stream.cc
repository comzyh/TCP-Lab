#include "byte_stream.hh"

#include <algorithm>
#include <iterator>
#include <stdexcept>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity):_capacity(capacity), buf(capacity, 0), rpos(0), used(0), _end_input(false)  { 
}

size_t ByteStream::write(const string &data) {
    if (used == _capacity || _end_input) {
        return 0;
    }
    const size_t size =  min(data.size(), _capacity - used);

    size_t p = (rpos + used) % _capacity; // copy dest of buf
    size_t pd = 0; // position to data for copy
    size_t r = size; // reamin to copy
    size_t len = min(p + r, _capacity) - p; // copy length
    used += size;
    buf.replace(buf.begin() + p, buf.begin() + p + len, data.begin() + pd, data.begin() + pd + len);
    if (len < r) {
        p = 0;
        pd = len;
        r -= len;
        buf.replace(buf.begin() + p, buf.begin() + p + r, data.begin() + pd, data.begin() + pd + r);
    }
    return size;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    const size_t size = min(len, used);
    string ret;
    ret.reserve(size);
    size_t l1 = min(rpos % _capacity + used, _capacity) - rpos % _capacity;
    ret.append(buf.begin() + rpos % _capacity, buf.begin() + rpos % _capacity + l1);
    
    if (ret.size() < size) {
        ret.append(buf.begin(), buf.begin() + size - l1);
    }
    return ret;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { 
    size_t size = min(len, used);
    rpos += size;
    used -= size;
 }

void ByteStream::end_input() { _end_input = true; }

bool ByteStream::input_ended() const { return _end_input; }

size_t ByteStream::buffer_size() const { return used; }

bool ByteStream::buffer_empty() const { return buffer_size() == 0; }

bool ByteStream::eof() const { return input_ended() &&  buffer_empty(); }

size_t ByteStream::bytes_written() const { return rpos + used; }

size_t ByteStream::bytes_read() const { return rpos; }

size_t ByteStream::remaining_capacity() const { 
    return _capacity - used;
}


