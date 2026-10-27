<div align="center">

# Mutualzz development moved to [git.mutualzz.com](https://git.mutualzz.com/mutualzz/mutualzz)

**GitHub is a read-only mirror. Issues and pull requests opened on GitHub are not monitored.**

Sign in with your GitHub account. No new password needed.

</div>

# Mutualzz RPC (C++)

C++ client for Mutualzz desktop Rich Presence over local IPC.

Requires the **Mutualzz desktop app** to be running.

Protocol: [`PROTOCOL.md`](https://github.com/Mutualzz/mutualzz/blob/master/packages/rpc/PROTOCOL.md)

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
