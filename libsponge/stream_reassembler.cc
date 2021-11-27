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
        last_byte = min(index + data.length(), _capacity + _output.bytes_read());
    }
    if(index + data.length() < _output.bytes_read()) {
        return;
    }
    if (next_idx == index) {
        _output.write(data);
        next_idx = _output.bytes_written();
    } else if (next_idx > index) {
        int offset =  next_idx - index;
        _output.write(data.substr(offset, data.length() - offset));
        next_idx = _output.bytes_written();
    } else {
        size_t len = data.length();
        if (len > _capacity - (index - _output.bytes_read())) {
            len = _capacity - (index - _output.bytes_read());
        }
        buffer.insert(make_pair(index, data.substr(0, len)));
        unassem_bytes += len;
        if (buffer.find(next_idx) != buffer.end()) {
            _output.write(buffer[next_idx]);
            unassem_bytes -= buffer[next_idx].length();
            buffer.erase(next_idx);
            next_idx = _output.bytes_written();
        }
    }

    for(auto d : buffer) {
        if(d.first <= _output.bytes_written() && d.second.length() + d.first >= _output.bytes_written()) {
            int offset =  next_idx - d.first;
            _output.write(d.second.substr(offset, d.second.length() - offset));
            //unassem_bytes -= d.second.length();
            //buffer.erase(d.first);
            next_idx = _output.bytes_written();
        }
    }
    if(recv_eof_flag && last_byte == _output.bytes_written())
        _output.end_input();
}

size_t StreamReassembler::unassembled_bytes() const { return unassem_bytes; }

bool StreamReassembler::empty() const { return {}; }
