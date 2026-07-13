#include "mutualzz/rpc.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <random>
#include <sstream>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace mutualzz::rpc {
namespace {

constexpr uint32_t kOpcodeHandshake = 0;
constexpr uint32_t kOpcodeFrame = 1;
constexpr uint32_t kOpcodeClose = 2;
constexpr uint32_t kOpcodePing = 3;
constexpr uint32_t kOpcodePong = 4;
constexpr int kMaxSlots = 10;

std::string pipePath(int slot) {
#ifdef _WIN32
  return "\\\\.\\pipe\\mutualzz-ipc-" + std::to_string(slot);
#else
  return "/tmp/mutualzz-ipc-" + std::to_string(slot);
#endif
}

std::string jsonEscape(std::string_view input) {
  std::string out;
  out.reserve(input.size() + 8);
  for (char c : input) {
    switch (c) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out += c;
        break;
    }
  }
  return out;
}

std::string makeNonce() {
  static thread_local std::mt19937_64 rng{
      static_cast<uint64_t>(
          std::chrono::steady_clock::now().time_since_epoch().count())};
  std::uniform_int_distribution<uint64_t> dist;
  std::ostringstream ss;
  ss << std::hex << dist(rng) << dist(rng);
  return ss.str();
}

std::string activityTypeString(ActivityType type) {
  return type == ActivityType::Listening ? "listening" : "playing";
}

std::string buildActivityJson(const Activity& activity) {
  std::ostringstream ss;
  ss << "{\"name\":\"" << jsonEscape(activity.name) << "\"";
  if (activity.details)
    ss << ",\"details\":\"" << jsonEscape(*activity.details) << "\"";
  if (activity.state)
    ss << ",\"state\":\"" << jsonEscape(*activity.state) << "\"";
  ss << ",\"type\":\"" << activityTypeString(activity.type) << "\"";
  if (activity.applicationId)
    ss << ",\"applicationId\":\"" << jsonEscape(*activity.applicationId)
       << "\"";
  if (activity.timestamps) {
    ss << ",\"timestamps\":{";
    bool first = true;
    if (activity.timestamps->start) {
      ss << "\"start\":" << *activity.timestamps->start;
      first = false;
    }
    if (activity.timestamps->end) {
      if (!first) ss << ",";
      ss << "\"end\":" << *activity.timestamps->end;
    }
    ss << "}";
  }
  ss << "}";
  return ss.str();
}

std::optional<std::string> extractJsonString(std::string_view json,
                                             std::string_view key) {
  const std::string needle = "\"" + std::string(key) + "\"";
  auto keyPos = json.find(needle);
  if (keyPos == std::string_view::npos) return std::nullopt;
  auto colon = json.find(':', keyPos + needle.size());
  if (colon == std::string_view::npos) return std::nullopt;
  auto firstQuote = json.find('"', colon + 1);
  if (firstQuote == std::string_view::npos) return std::nullopt;
  auto secondQuote = firstQuote + 1;
  while (secondQuote < json.size()) {
    if (json[secondQuote] == '"' && json[secondQuote - 1] != '\\') break;
    ++secondQuote;
  }
  if (secondQuote >= json.size()) return std::nullopt;
  return std::string(json.substr(firstQuote + 1, secondQuote - firstQuote - 1));
}

int currentPid() {
#ifdef _WIN32
  return static_cast<int>(GetCurrentProcessId());
#else
  return static_cast<int>(getpid());
#endif
}

}  // namespace

Client::Client(Options options) : options_(std::move(options)) {}

Client::~Client() { disconnect(); }

bool Client::isConnected() const {
#ifdef _WIN32
  return handle_ != nullptr && ready_;
#else
  return fd_ >= 0 && ready_;
#endif
}

const std::string& Client::path() const { return boundPath_; }

bool Client::writeFrame(uint32_t opcode, std::string_view json,
                        std::string* error) {
  std::array<uint8_t, 8> header{};
  const auto length = static_cast<uint32_t>(json.size());
  header[0] = static_cast<uint8_t>(opcode & 0xff);
  header[1] = static_cast<uint8_t>((opcode >> 8) & 0xff);
  header[2] = static_cast<uint8_t>((opcode >> 16) & 0xff);
  header[3] = static_cast<uint8_t>((opcode >> 24) & 0xff);
  header[4] = static_cast<uint8_t>(length & 0xff);
  header[5] = static_cast<uint8_t>((length >> 8) & 0xff);
  header[6] = static_cast<uint8_t>((length >> 16) & 0xff);
  header[7] = static_cast<uint8_t>((length >> 24) & 0xff);

  std::vector<uint8_t> frame;
  frame.reserve(8 + json.size());
  frame.insert(frame.end(), header.begin(), header.end());
  frame.insert(frame.end(), json.begin(), json.end());

#ifdef _WIN32
  if (!handle_) {
    if (error) *error = "not connected";
    return false;
  }
  DWORD written = 0;
  if (!WriteFile(static_cast<HANDLE>(handle_), frame.data(),
                 static_cast<DWORD>(frame.size()), &written, nullptr) ||
      written != frame.size()) {
    if (error) *error = "failed to write frame";
    return false;
  }
#else
  if (fd_ < 0) {
    if (error) *error = "not connected";
    return false;
  }
  size_t offset = 0;
  while (offset < frame.size()) {
    const auto n =
        ::write(fd_, frame.data() + offset, frame.size() - offset);
    if (n <= 0) {
      if (error) *error = "failed to write frame";
      return false;
    }
    offset += static_cast<size_t>(n);
  }
#endif
  return true;
}

bool Client::handleIncoming(std::string* error) {
#ifdef _WIN32
  if (!handle_) return false;
  std::array<char, 8192> buf{};
  DWORD read = 0;
  if (!ReadFile(static_cast<HANDLE>(handle_), buf.data(),
                static_cast<DWORD>(buf.size()), &read, nullptr)) {
    if (error) *error = "failed to read";
    return false;
  }
  if (read == 0) {
    if (error) *error = "connection closed";
    return false;
  }
  readBuffer_.append(buf.data(), read);
#else
  if (fd_ < 0) return false;
  std::array<char, 8192> buf{};
  const auto n = ::read(fd_, buf.data(), buf.size());
  if (n <= 0) {
    if (error) *error = "failed to read";
    return false;
  }
  readBuffer_.append(buf.data(), static_cast<size_t>(n));
#endif
  return true;
}

bool Client::readUntil(const std::string& expectCmd,
                       std::optional<std::string> expectNonce,
                       std::string* outDataJson, std::string* error) {
  using clock = std::chrono::steady_clock;
  const auto deadline =
      clock::now() + std::chrono::milliseconds(options_.connectTimeoutMs);

  while (clock::now() < deadline) {
    while (readBuffer_.size() >= 8) {
      const auto* bytes =
          reinterpret_cast<const uint8_t*>(readBuffer_.data());
      const uint32_t opcode = static_cast<uint32_t>(bytes[0]) |
                              (static_cast<uint32_t>(bytes[1]) << 8) |
                              (static_cast<uint32_t>(bytes[2]) << 16) |
                              (static_cast<uint32_t>(bytes[3]) << 24);
      const uint32_t length = static_cast<uint32_t>(bytes[4]) |
                              (static_cast<uint32_t>(bytes[5]) << 8) |
                              (static_cast<uint32_t>(bytes[6]) << 16) |
                              (static_cast<uint32_t>(bytes[7]) << 24);
      if (readBuffer_.size() < 8 + length) break;

      const std::string payload = readBuffer_.substr(8, length);
      readBuffer_.erase(0, 8 + length);

      if (opcode == kOpcodePing) {
        if (!writeFrame(kOpcodePong, payload.empty() ? "{}" : payload, error))
          return false;
        continue;
      }
      if (opcode == kOpcodeClose) {
        if (error) *error = "host closed connection";
        return false;
      }
      if (opcode != kOpcodeFrame) continue;

      const auto cmd = extractJsonString(payload, "cmd");
      if (!cmd) continue;

      if (*cmd == "READY" && expectCmd == "READY") return true;

      if (*cmd == expectCmd) {
        if (expectNonce) {
          const auto nonce = extractJsonString(payload, "nonce");
          if (!nonce || *nonce != *expectNonce) continue;
        }
        if (outDataJson) *outDataJson = payload;
        return true;
      }
    }

    if (!handleIncoming(error)) return false;
  }

  if (error) *error = "timed out waiting for response";
  return false;
}

bool Client::openSocket(std::string* error) {
  disconnect();

  auto tryPath = [&](const std::string& path) -> bool {
#ifdef _WIN32
    HANDLE handle = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                                nullptr, OPEN_EXISTING, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;
    handle_ = handle;
#else
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return false;
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (path.size() >= sizeof(addr.sun_path)) {
      ::close(fd);
      return false;
    }
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
      ::close(fd);
      return false;
    }
    fd_ = fd;
#endif
    boundPath_ = path;
    return true;
  };

  if (options_.path) {
    if (!tryPath(*options_.path)) {
      if (error) *error = "failed to connect to preferred path";
      return false;
    }
    return true;
  }

  for (int slot = 0; slot < kMaxSlots; ++slot) {
    if (tryPath(pipePath(slot))) return true;
  }

  if (error)
    *error =
        "No Mutualzz RPC pipe found (is the Mutualzz desktop app running?)";
  return false;
}

bool Client::handshake(std::string* error) {
  const std::string payload =
      "{\"clientId\":\"" + jsonEscape(options_.clientId) + "\"}";
  if (!writeFrame(kOpcodeHandshake, payload, error)) return false;
  return readUntil("READY", std::nullopt, nullptr, error);
}

bool Client::connect(std::string* error) {
  if (options_.clientId.empty()) {
    if (error) *error = "clientId is required";
    return false;
  }
  if (isConnected()) return true;
  if (!openSocket(error)) return false;
  if (!handshake(error)) {
    disconnect();
    return false;
  }
  ready_ = true;
  return true;
}

bool Client::setActivity(const Activity* activity, std::optional<int> pid,
                         std::string* error) {
  if (!isConnected() && !connect(error)) return false;

  const int usePid = pid.value_or(currentPid());
  const std::string nonce = makeNonce();
  std::ostringstream ss;
  ss << "{\"cmd\":\"SET_ACTIVITY\",\"nonce\":\"" << jsonEscape(nonce)
     << "\",\"args\":{\"pid\":" << usePid << ",\"activity\":";
  if (activity)
    ss << buildActivityJson(*activity);
  else
    ss << "null";
  ss << "}}";

  if (!writeFrame(kOpcodeFrame, ss.str(), error)) return false;
  return readUntil("SET_ACTIVITY", nonce, nullptr, error);
}

bool Client::clearActivity(std::optional<int> pid, std::string* error) {
  return setActivity(nullptr, pid, error);
}

void Client::disconnect() {
  ready_ = false;
  readBuffer_.clear();

#ifdef _WIN32
  if (handle_) {
    writeFrame(kOpcodeClose, "{}", nullptr);
    CloseHandle(static_cast<HANDLE>(handle_));
    handle_ = nullptr;
  }
#else
  if (fd_ >= 0) {
    writeFrame(kOpcodeClose, "{}", nullptr);
    ::close(fd_);
    fd_ = -1;
  }
#endif
  boundPath_.clear();
}

}  // namespace mutualzz::rpc
