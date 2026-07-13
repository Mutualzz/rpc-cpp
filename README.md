# Mutualzz RPC (C++)

C++ client for Mutualzz desktop Rich Presence over local IPC.

Protocol: [`../rpc/PROTOCOL.md`](../rpc/PROTOCOL.md)

## Build

```bash
cmake -S packages/rpc-cpp -B build/rpc-cpp -DMUTUALZZ_RPC_BUILD_EXAMPLES=ON
cmake --build build/rpc-cpp
```

Source tarballs ship on `rpc-v*` GitHub releases from [`.github/workflows/release-rpc.yml`](../../.github/workflows/release-rpc.yml).

## Usage

```cpp
#include <mutualzz/rpc.hpp>

mutualzz::rpc::Options options;
options.clientId = "your-app-id";

mutualzz::rpc::Client rpc(options);
std::string error;
if (!rpc.connect(&error)) {
  // Mutualzz desktop not running / no pipe
}

mutualzz::rpc::Activity activity;
activity.name = "My Game";
activity.details = "Ranked";
activity.state = "In a Match";
rpc.setActivity(&activity);

rpc.clearActivity();
rpc.disconnect();
```

Keep the process connected while presence should show.

## Example

```bash
./build/rpc-cpp/mutualzz_rpc_example
```
