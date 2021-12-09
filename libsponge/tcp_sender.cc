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
    , _stream(capacity) {
    sof = true;
    _window_size = 1;
}

uint64_t TCPSender::bytes_in_flight() const { return bytes_flight; }

void TCPSender::fill_window() {
    if (sof) {
        header.syn = true;
        header.seqno = _isn;
        bytes_flight++;
        _next_seqno++;
        sof = false;
        segment.header() = header;
        outstanding.push_back(segment);
        _segments_out.push(segment);
        return;
    } else if (!_stream.buffer_empty()) {
        header.syn = false;
        header.seqno = wrap(_stream.bytes_read() + 1, _isn);

        bytes_to_send = min(size_t(_window_size - 2), TCPConfig::MAX_PAYLOAD_SIZE);
        bytes_to_send = min(bytes_to_send, _stream.buffer_size());
        payload_str = _stream.read(bytes_to_send);
        bytes_flight += payload_str.size();
        _next_seqno += payload_str.size();
        segment.payload() = Buffer(std::move(payload_str));

        segment.header() = header;
        outstanding.push_back(segment);
        _segments_out.push(segment);
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
        } else {
            ++seg;
        }
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { DUMMY_CODE(ms_since_last_tick); }

unsigned int TCPSender::consecutive_retransmissions() const { return {}; }

void TCPSender::send_empty_segment() {}
