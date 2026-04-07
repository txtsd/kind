# kind

A cross-platform third-party Discord client built with Qt6 and C++23. It talks to Discord directly and gives you a native desktop experience without the weight of Electron. It is designed to be fast, responsive, and easy on your hardware.

## The Name

**k**ind **i**s **n**ot **d**iscord. If that looks familiar, you know who to thank.

## Features

- **Guild and channel navigation** with unread indicators, mention counts, and mute state
- **Rich message rendering** with markdown, embeds, attachments, reactions, stickers, replies, and custom emoji
- **Interactive components** including buttons, select menus, and ephemeral notices
- **Direct messages** with a dedicated conversation list
- **Image caching** with memory and disk tiers, viewport-prioritized downloads, and conditional revalidation
- **SQLite message cache** with read state tracking
- **Account-scoped state** so logs, cache, and config stay separated per account
- **System keychain integration** for token storage
- **Gateway WebSocket** with heartbeat, resume, and reconnect
- **REST client** with rate limit handling
- **Timestamp column** with configurable visibility
- **Status bar** with connection state
- **Preferences dialog** for runtime configuration
- **Diagnostic tooling** via `kind-analyze`, a log analysis suite (see [tools/](tools/))

## Getting It

Sponsoring me on GitHub is currently the best way to get access to pre-built binaries. This is how I fund continued development.

The source is right here under GPLv3. If you can build it yourself, you are welcome to do so. See **Building from Source** below.

## Building from Source

### Requirements

- **CMake** 4.0 or later
- **C++23 compiler** (GCC 14+, Clang 18+)
- **Qt** 6.10+ with the following modules: Concurrent, Core, Network, WebSockets, Sql, Widgets, Protobuf, ProtobufWellKnownTypes
- **QtKeychain** (qtkeychain, built with Qt6)
- **protobuf** (protoc compiler and development headers)
- **libsecret** (Linux)

These are fetched automatically via CMake's FetchContent if not found on your system:
- spdlog 1.17
- toml++ 3.4
- GoogleTest 1.17 (tests only)

Optional:
- **mold** linker (auto-detected, speeds up linking)
- **mimalloc** or **jemalloc** (custom allocator, see CMake options)

### Linux

Install the system dependencies for your distribution. On Debian/Ubuntu:

```sh
sudo apt install protobuf-compiler libprotobuf-dev libsecret-1-dev \
  libgl1-mesa-dev libxkbcommon-dev mold
```

On Arch:

```sh
sudo pacman -S protobuf libsecret mold
```

Qt 6.10 is not yet in most distro repos. You may need to install it from [the Qt installer](https://www.qt.io/download-qt-installer-oss) or build it yourself. QtKeychain must also be built against your Qt6 installation if not packaged.

### Configure and Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The binary lands at `build/src/gui/kind-gui`.

### Running Tests

```sh
ctest --test-dir build
```

### CMake Options

| Option | Default | Description |
|---|---|---|
| `KIND_BUILD_GUI` | ON | Build the Qt6 GUI frontend |
| `KIND_BUILD_TUI` | OFF | Build the FTXUI TUI frontend (future feature) |
| `KIND_BUILD_TESTS` | ON | Build the test suite |
| `KIND_ENABLE_ASAN` | OFF | Enable AddressSanitizer |
| `KIND_ENABLE_TSAN` | OFF | Enable ThreadSanitizer |
| `KIND_ENABLE_UBSAN` | OFF | Enable UndefinedBehaviorSanitizer |
| `KIND_ALLOCATOR` | none | Custom allocator: `mimalloc`, `jemalloc`, or `none` |

## Contributing

I am not accepting pull requests at this time. The codebase is still maturing and I have a specific vision for its architecture and quality that I need to get right before outside contributions make sense. Once I am confident in the foundation, I will open it up.

If you find a bug or have a suggestion, issues are welcome.

## On AI

This project is built with AI assistance. All of it. I set the architecture,
make design decisions, define quality standards, and evaluate the result. The AI
writes code to a standard I enforce through testing, MVC compliance, logging,
and refusing to accept anything that blocks the UI or cuts corners.

I have [more to say about this](https://kind.ihavea.quest/on-ai/) if you are
interested.

## License

[GNU General Public License v3.0](LICENSE)
