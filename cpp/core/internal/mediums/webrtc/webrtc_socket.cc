#include "core/internal/mediums/webrtc/webrtc_socket.h"

#include "platform/public/logging.h"
#include "platform/public/mutex_lock.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

// OutputStreamImpl
Exception WebRtcSocket::OutputStreamImpl::Write(const ByteArray& data) {
  if (data.size() > kMaxDataSize) {
    NEARBY_LOG(WARNING, "Sending data larger than 1MB");
    return {Exception::kIo};
  }

  socket_->BlockUntilSufficientSpaceInBuffer(data.size());

  if (socket_->IsClosed()) {
    NEARBY_LOG(WARNING, "Tried sending message while socket is closed");
    return {Exception::kIo};
  }

  if (!socket_->SendMessage(data)) {
    return {Exception::kIo};
  }
  return {Exception::kSuccess};
}

Exception WebRtcSocket::OutputStreamImpl::Flush() {
  // Java implementation is empty.
  return {Exception::kSuccess};
}

Exception WebRtcSocket::OutputStreamImpl::Close() {
  socket_->Close();
  return {Exception::kSuccess};
}

// WebRtcSocket
WebRtcSocket::WebRtcSocket(
    const std::string& name,
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel)
    : name_(name), data_channel_(std::move(data_channel)) {}

InputStream& WebRtcSocket::GetInputStream() { return pipe_.GetInputStream(); }

OutputStream& WebRtcSocket::GetOutputStream() { return output_stream_; }

void WebRtcSocket::Close() {
  if (IsClosed()) return;

  closed_.Set(true);
  pipe_.GetInputStream().Close();
  pipe_.GetOutputStream().Close();
  data_channel_->Close();
  WakeUpWriter();
  socket_closed_listener_.socket_closed_cb();
}

void WebRtcSocket::NotifyDataChannelMsgReceived(const ByteArray& message) {
  if (!pipe_.GetOutputStream().Write(message).Ok()) {
    Close();
    return;
  }

  if (!pipe_.GetOutputStream().Flush().Ok()) Close();
}

void WebRtcSocket::NotifyDataChannelBufferedAmountChanged() { WakeUpWriter(); }

bool WebRtcSocket::SendMessage(const ByteArray& data) {
  return data_channel_->Send(
      webrtc::DataBuffer(std::string(data.data(), data.size())));
}

bool WebRtcSocket::IsClosed() { return closed_.Get(); }

void WebRtcSocket::WakeUpWriter() {
  MutexLock lock(&backpressure_mutex_);
  buffer_variable_.Notify();
}

void WebRtcSocket::SetOnSocketClosedListener(SocketClosedListener&& listener) {
  socket_closed_listener_ = std::move(listener);
}

void WebRtcSocket::BlockUntilSufficientSpaceInBuffer(int length) {
  MutexLock lock(&backpressure_mutex_);
  while (!IsClosed() &&
         (data_channel_->buffered_amount() + length > kMaxDataSize)) {
    // TODO(himanshujaju): Add wait with timeout.
    buffer_variable_.Wait();
  }
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
