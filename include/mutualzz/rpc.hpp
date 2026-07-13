#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace mutualzz::rpc {

enum class ActivityType { Playing, Listening };

struct Timestamps {
  std::optional<int64_t> start;
  std::optional<int64_t> end;
};

struct Activity {
  std::string name;
  std::optional<std::string> details;
  std::optional<std::string> state;
  ActivityType type = ActivityType::Playing;
  std::optional<std::string> applicationId;
  std::optional<Timestamps> timestamps;
};

struct Options {
  std::string clientId;
  int connectTimeoutMs = 5000;
  std::optional<std::string> path;
};

class Client {
 public:
  explicit Client(Options options);
  ~Client();

  Client(const Client&) = delete;
  Client& operator=(const Client&) = delete;

  bool connect(std::string* error = nullptr);
  bool setActivity(const Activity* activity, std::optional<int> pid = std::nullopt,
                   std::string* error = nullptr);
  bool clearActivity(std::optional<int> pid = std::nullopt,
                     std::string* error = nullptr);
  void disconnect();

  bool isConnected() const;
  const std::string& path() const;

 private:
  bool openSocket(std::string* error);
  bool handshake(std::string* error);
  bool writeFrame(uint32_t opcode, std::string_view json, std::string* error);
  bool readUntil(const std::string& expectCmd, std::optional<std::string> expectNonce,
                 std::string* outDataJson, std::string* error);
  bool handleIncoming(std::string* error);

  Options options_;
  std::string boundPath_;
  bool ready_ = false;
  std::string readBuffer_;

#ifdef _WIN32
  void* handle_ = nullptr;
#else
  int fd_ = -1;
#endif
};

}  // namespace mutualzz::rpc
