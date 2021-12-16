#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return bytes_flight; }

void TCPSender::fill_window() {
    if (_window_size == 0)
        fake_window_size = 1;
    if (sof) {
        send_segment(1, 0, _isn, 0);
        sof = false;
        return;
    }
    while (!_stream.buffer_empty()) {
        bytes_to_send = min(size_t(fake_window_size - bytes_flight), TCPConfig::MAX_PAYLOAD_SIZE);
        bytes_to_send = min(bytes_to_send, _stream.buffer_size());
        if (bytes_to_send == 0)
            break;
        if (_stream.input_ended() && _stream.buffer_size() + 1 <= fake_window_size - bytes_flight &&
            bytes_to_send == _stream.buffer_size() && eof == false) {
            // send the last few bytes with fin
            send_segment(0, 1, wrap(_stream.bytes_read() + 1, _isn), bytes_to_send);
            eof = true;
        } else {  // keep sending bytes until bytes_to_send is 0 or buffer is empty
            send_segment(0, 0, wrap(_stream.bytes_read() + 1, _isn), bytes_to_send);
        }
    }
    if (_stream.input_ended() && eof == false && _stream.buffer_size() == 0 && fake_window_size > bytes_flight) {
        send_segment(0, 1, wrap(_next_seqno, _isn), 0);
        eof = true;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    if (ackno.raw_value() <= _isn.raw_value() || wrap(_next_seqno, _isn) - ackno < 0)
        return;
    _ackno = ackno;
    _window_size = window_size;
    fake_window_size = window_size;
    for (auto seg = outstanding.begin(); seg != outstanding.end();) {
        size_t len = seg->payload().size();
        if (seg->header().fin) {
            len++;
        }
        if (wrap(len, seg->header().seqno) - ackno <= 0) {
            if (seg->header().syn || seg->header().fin) {
                --bytes_flight;
            }
            bytes_flight -= seg->payload().size();
            outstanding.erase(seg++);
            consecutive_tx_times = 0;
            rto_val = _initial_retransmission_timeout;
            rto_timer = _initial_retransmission_timeout;
        } else {
            ++seg;
        }
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (rto_timer - int32_t(ms_since_last_tick) > 0) {
        rto_timer -= ms_since_last_tick;
    } else {
        if (outstanding.size() > 0)
            _segments_out.push(outstanding.front());
        
        if (_window_size != 0) {
            ++consecutive_tx_times;
            rto_val *= 2;
        }
        rto_timer_start = false;
        start_RTO_timer();
    }
}

void TCPSender::start_RTO_timer() {
    if (!rto_timer_start) {
        rto_timer = rto_val;
        rto_timer_start = true;
    }
}

void TCPSender::send_segment(const bool syn, const bool fin, WrappingInt32 seqno, size_t bytes_send) {
    segment.header().syn = syn;
    segment.header().fin = fin;
    segment.header().seqno = seqno;
    if (bytes_send != 0) {
        payload_str = _stream.read(bytes_to_send);
    } else {
        payload_str = "";
    }
    if (syn || fin) {
        bytes_flight++;
        _next_seqno++;
    }
    bytes_flight += payload_str.size();
    _next_seqno += payload_str.size();
    segment.payload() = Buffer(std::move(payload_str));
    outstanding.push_back(segment);
    _segments_out.push(segment);
    start_RTO_timer();
}

unsigned int TCPSender::consecutive_retransmissions() const { return consecutive_tx_times; }

void TCPSender::send_empty_segment() {
    segment.header().rst = true;
    segment.header().syn = false;
    segment.header().fin = false;
    segment.header().seqno = wrap(_stream.bytes_read() + 1, _isn);
    payload_str = "";
    segment.payload() = Buffer(std::move(payload_str));
    outstanding.push_back(segment);
    _segments_out.push(segment);
}
