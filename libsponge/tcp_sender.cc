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
    if (sof) {
        send_segment(true, false, _isn, 0);
        sof = false;
    } else if (!_stream.input_ended() && !_stream.buffer_empty() && _window_size - bytes_flight > 0) {
        bytes_to_send = min(size_t(_window_size - bytes_flight), TCPConfig::MAX_PAYLOAD_SIZE);
        bytes_to_send = min(bytes_to_send, _stream.buffer_size());
        send_segment(false, false, wrap(_stream.bytes_read() + 1, _isn), bytes_to_send);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    if (ackno.raw_value() <= _isn.raw_value())
        return;
    _ackno = ackno;
    _window_size = window_size;
    // _next_seqno = unwrap(_ackno, _isn, _next_seqno);
    for (auto seg = outstanding.begin(); seg != outstanding.end();) {
        if (wrap(seg->payload().size(), seg->header().seqno) - ackno <= 0) {
            if (seg->header().syn || seg->header().fin) {
                --bytes_flight;
            } else {
                bytes_flight -= seg->payload().size();
            }
            outstanding.erase(seg++);
            consecutive_tx_times = 0;
            rto_val = _initial_retransmission_timeout;
        } else {
            ++seg;
        }
    }
    if (_stream.input_ended()) {
        if (_stream.buffer_size() + 1 <= window_size - bytes_flight) {
            bytes_to_send = min(size_t(_window_size - bytes_flight), TCPConfig::MAX_PAYLOAD_SIZE);
            bytes_to_send = min(bytes_to_send, _stream.buffer_size());
            send_segment(false, true, wrap(_stream.bytes_read() + 1, _isn), bytes_to_send);
        } else if (_stream.buffer_size() == 0 && _window_size - bytes_flight > 0) {
            send_segment(false, true, _isn, 0);
        }
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (rto_timer - int32_t(ms_since_last_tick) > 0) {
        rto_timer -= ms_since_last_tick;
    } else {
        _segments_out.push(outstanding.front());
        ++consecutive_tx_times;
        rto_val *= 2;
        start_RTO_timer();
    }
}

void TCPSender::start_RTO_timer() {
    // if (rto_timer <= 0)
    rto_timer = rto_val;
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
    } else {
        bytes_flight += payload_str.size();
        _next_seqno += payload_str.size();
    }
    segment.payload() = Buffer(std::move(payload_str));
    outstanding.push_back(segment);
    _segments_out.push(segment);
    start_RTO_timer();
}

unsigned int TCPSender::consecutive_retransmissions() const { return consecutive_tx_times; }

void TCPSender::send_empty_segment() {}
