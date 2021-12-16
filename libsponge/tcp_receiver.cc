#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    if (seg.header().syn) {
        if (seg.header().fin) {
            _reassembler.stream_out().end_input();
        }
        sof = true;
        isn = seg.header().seqno;
        chkpoint = 0;
    }
    if (_reassembler.stream_out().input_ended()) {
        // sof = false;
    }
    if (sof) {
        if (seg.length_in_sequence_space() > 0) {
            if (seg.payload().size() != 0 && seg.header().syn) {
                absolute_seq_num = 1;
            } else {
                absolute_seq_num = unwrap(seg.header().seqno, isn, chkpoint);
            }
        }
        stream_idx = absolute_seq_num - 1;
        chkpoint = absolute_seq_num;
        if (seg.header().fin) {
            if (seg.payload().size() != 0) {
                if (_reassembler.stream_out().bytes_written() + fin_redundent_bytes == stream_idx) {
                    fin_redundent_bytes++;
                } else {  // deal with fin with data but unassenmbled
                    fin_unass_flag = true;
                }
            } else {
                fin_redundent_bytes++;
            }
        }
        _reassembler.push_substring(seg.payload().copy(), stream_idx, seg.header().fin);
        if (_reassembler.unassembled_bytes() == 0 && fin_unass_flag) {
            fin_redundent_bytes++;
            fin_unass_flag = false;
        }
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!sof)
        return nullopt;
    else
        return wrap(_reassembler.stream_out().bytes_written() + 1 + fin_redundent_bytes, isn);
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
