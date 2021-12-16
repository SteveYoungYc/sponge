#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _receiver.window_size(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return {}; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    if (seg.header().rst) {
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        reset = true;
    }
    _receiver.segment_received(seg);
    if (seg.header().ack == true) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }

    if (seg.length_in_sequence_space() > 0) {
        if (seg.header().syn) {
            if (!seg.header().ack) {
                if (_sender.next_seqno_absolute() == 0) {  // listen
                    _sender.fill_window();
                    _sender.segments_out().front().header().ack = true;
                    _sender.segments_out().front().header().win = _receiver.window_size();
                    _sender.segments_out().front().header().ackno = seg.header().seqno + 1;
                    _segments_out.push(_sender.segments_out().front());
                    _sender.segments_out().pop();
                    sender_start = true;
                } else {
                    send_segment(0, 1, 0, 0, _receiver.window_size(), seg.header().seqno + 1);
                }
            } else {
                send_segment(0, 1, 0, 0, 0, seg.header().seqno + 1);
            }
        } else {
            send_segment(0, 1, 0, 0, _receiver.window_size(), seg.header().seqno + seg.length_in_sequence_space());
        }
    }
    if (_receiver.ackno().has_value() and (seg.length_in_sequence_space() == 0) and
        seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
    }
    if (_receiver.stream_out().eof()) {
        _linger_after_streams_finish = false;
    }
}

bool TCPConnection::active() const { return !reset; }

size_t TCPConnection::write(const string &data) {
    size_t last_bytes_written = _sender.stream_in().bytes_written();
    _sender.stream_in().write(data);
    _sender.fill_window();
    send_sender_segments();
    return _sender.stream_in().bytes_written() - last_bytes_written;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _sender.tick(ms_since_last_tick);
    if (sender_start)
        _sender.fill_window();
    send_sender_segments();
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        // _sender.send_empty_segment();
        _segments_out.pop();
        send_segment(1, 0, 0, 0, 0, WrappingInt32{0});
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        reset = true;
    }
}

void TCPConnection::end_input_stream() { _sender.stream_in().end_input(); }

void TCPConnection::connect() {
    _sender.fill_window();
    _segments_out.push(_sender.segments_out().front());
    _sender.segments_out().pop();
    sender_start = true;
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            send_segment(1, 0, 0, 0, 0, WrappingInt32{0});
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::send_sender_segments() {
    while (!_sender.segments_out().empty()) {
        if (_receiver.ackno().has_value()) {
            _sender.segments_out().front().header().ack = true;
            _sender.segments_out().front().header().ackno = _receiver.ackno().value();
        }
        if (_receiver.window_size() > std::numeric_limits<uint16_t>::max()) {
            _sender.segments_out().front().header().win = std::numeric_limits<uint16_t>::max();
        } else {
            _sender.segments_out().front().header().win = _receiver.window_size();
        }
        _segments_out.push(_sender.segments_out().front());
        _sender.segments_out().pop();
    }
    // cout << " TCPConnection " << _receiver.window_size();
}

void TCPConnection::send_segment(const bool rst,
                                 const bool ack,
                                 const bool syn,
                                 const bool fin,
                                 uint16_t win,
                                 WrappingInt32 ackno) {
    segment.header().rst = rst;
    segment.header().ack = ack;
    segment.header().syn = syn;
    segment.header().fin = fin;
    segment.header().win = win;
    segment.header().ackno = ackno;
    _segments_out.push(segment);
}

void TCPConnection::send_segment(const bool rst,
                                 const bool ack,
                                 const bool syn,
                                 const bool fin,
                                 uint16_t win,
                                 WrappingInt32 seqno,
                                 WrappingInt32 ackno) {
    segment.header().rst = rst;
    segment.header().ack = ack;
    segment.header().syn = syn;
    segment.header().fin = fin;
    segment.header().win = win;
    segment.header().seqno = seqno;
    segment.header().ackno = ackno;
    _segments_out.push(segment);
}
