#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (eof) {
        recv_eof_flag = true;
        last_byte = index + data.length(); //min(index + data.length(), _capacity + _output.bytes_read());
    }

    if (index + data.length() < _output.bytes_written()) {
        return;
    }
    if (next_idx == index) {
        _output.write(data);
        next_idx = _output.bytes_written();
    } else if (next_idx > index) {
        int offset = next_idx - index;
        _output.write(data.substr(offset, data.length() - offset));
        next_idx = _output.bytes_written();
    } else {
        size_t len = data.length();
        if (len > _capacity - unassem_bytes - _output.buffer_size()) {
            len = _capacity - unassem_bytes - _output.buffer_size();
        }
        for (auto i = buffer.begin(); i != buffer.end();) {
            int overlap = detect_overlap(index, data.length(), i->first, i->second.length());
            if (overlap == -1) {
                buffer_overlap_flag = true;
                break;
            } else if (overlap == 1) {
                unassem_bytes -= i->second.length();
                buffer.erase(i++);
            } else {
                ++i;
            }
        }
        if (!buffer_overlap_flag) {
            buffer.insert(make_pair(index, data.substr(0, len)));
            unassem_bytes += len;
        }
    }

    for (auto i = buffer.begin(); i != buffer.end();) {
        if (i->first <= _output.bytes_written() && i->first + i->second.length() > _output.bytes_written()) {
            int offset = next_idx - i->first;
            _output.write(i->second.substr(offset, i->second.length() - offset));
            unassem_bytes -= i->second.length() - offset;
            buffer.erase(i++);
            next_idx = _output.bytes_written();
        } else if (i->first + i->second.length() <= _output.bytes_written()) {
            unassem_bytes -= i->second.length();
            buffer.erase(i++);
        } else {
            ++i;
        }
    }
    if (recv_eof_flag && last_byte == _output.bytes_written())
        _output.end_input();
}

int StreamReassembler::detect_overlap(size_t a_idx, size_t a_len, size_t b_idx, size_t b_len) {
    bool a_overlap = (a_idx >= b_idx) && (a_idx + a_len <= b_idx + b_len);
    bool b_overlap = (b_idx >= a_idx) && (b_idx + b_len <= a_idx + a_len);
    if (a_overlap)
        return -1;
    else if (b_overlap)
        return 1;
    else
        return 0;
}

size_t StreamReassembler::unassembled_bytes() const { return unassem_bytes; }

bool StreamReassembler::empty() const { return unassem_bytes == 0;; }
