#include "text/emoji_map.hpp"
#include "logging.hpp"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace kind {

static std::shared_mutex emoji_mutex;
static std::unordered_map<std::string, std::string> emoji_map;
static bool loaded = false;

static void load_emoji_map() {
  std::unique_lock lock(emoji_mutex);
  if (loaded) return;

  QFile file(":/emoji.json");
  if (!file.open(QIODevice::ReadOnly)) {
    log::client()->warn("emoji: failed to open bundled emoji.json");
    loaded = true;
    return;
  }

  auto data = file.readAll();
  QJsonParseError parse_error;
  auto doc = QJsonDocument::fromJson(data, &parse_error);
  if (!doc.isObject()) {
    log::client()->warn("emoji: invalid JSON ({})",
                        parse_error.errorString().toStdString());
    loaded = true;
    return;
  }

  auto obj = doc.object();
  emoji_map.reserve(static_cast<size_t>(obj.size()));
  for (auto it = obj.begin(); it != obj.end(); ++it) {
    emoji_map.emplace(it.key().toStdString(), it.value().toString().toStdString());
  }
  loaded = true;
  log::client()->debug("emoji: loaded {} shortcodes from bundled data",
                        emoji_map.size());
}

void replace_emoji_shortcodes(std::string& text) {
  if (text.empty()) return;
  if (!loaded) {
    load_emoji_map();
  }

  std::shared_lock lock(emoji_mutex);
  if (emoji_map.empty()) return;

  std::string result;
  result.reserve(text.size());

  size_t i = 0;
  while (i < text.size()) {
    if (text[i] == ':' && i + 1 < text.size()) {
      size_t end = text.find(':', i + 1);
      if (end != std::string::npos && end > i + 1) {
        auto name = text.substr(i + 1, end - i - 1);
        if (name.find(' ') == std::string::npos) {
          auto it = emoji_map.find(name);
          if (it != emoji_map.end()) {
            result += it->second;
            i = end + 1;
            continue;
          }
        }
      }
    }
    result += text[i];
    ++i;
  }
  text = std::move(result);
}

void reload_emoji_map() {
  {
    std::unique_lock lock(emoji_mutex);
    loaded = false;
    emoji_map.clear();
  }
  load_emoji_map();
}

} // namespace kind
