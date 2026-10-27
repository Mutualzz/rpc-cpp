<div align="center">

# Mutualzz is developed at [git.mutualzz.com](https://git.mutualzz.com/mutualzz/rpc-cpp)

**GitHub is a read-only mirror. Issues and pull requests opened on GitHub are not monitored.**

Sign in with your GitHub account. No new password needed.

</div>

# Mutualzz RPC (C++)

C++ client for Mutualzz desktop Rich Presence over local IPC.

Requires the **Mutualzz desktop app** to be running.

Protocol: [`PROTOCOL.md`](https://git.mutualzz.com/mutualzz/rpc/src/branch/master/PROTOCOL.md)

## Build

```bash
cmake -S packages/rpc-cpp -B build/rpc-cpp -DMUTUALZZ_RPC_BUILD_EXAMPLES=ON
cmake --build build/rpc-cpp
```

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
