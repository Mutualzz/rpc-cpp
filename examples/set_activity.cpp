#include <chrono>
#include <iostream>
#include <mutualzz/rpc.hpp>
#include <thread>

int main() {
  mutualzz::rpc::Options options;
  options.clientId = "cpp-example";

  mutualzz::rpc::Client rpc(options);

  std::string error;
  if (!rpc.connect(&error)) {
    std::cerr << "connect failed: " << error << "\n";
    return 1;
  }

  mutualzz::rpc::Timestamps timestamps;
  timestamps.start =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();

  mutualzz::rpc::Activity activity;
  activity.name = "C++ Example";
  activity.details = "Ranked";
  activity.state = "In a Match";
  activity.type = mutualzz::rpc::ActivityType::Playing;
  activity.timestamps = timestamps;

  if (!rpc.setActivity(&activity, std::nullopt, &error)) {
    std::cerr << "setActivity failed: " << error << "\n";
    return 1;
  }

  std::cout << "Connected to " << rpc.path()
            << " — holding 30s\n";
  std::this_thread::sleep_for(std::chrono::seconds(30));
  rpc.clearActivity();
  rpc.disconnect();
  return 0;
}
