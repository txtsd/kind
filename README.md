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

The source is right here under GPLv3. If you can build it yourself, you are welcome to do so.

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
