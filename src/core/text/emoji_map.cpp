#include "text/emoji_map.hpp"
#include "config/platform_paths.hpp"
#include "logging.hpp"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>

#include <filesystem>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace kind {

static std::shared_mutex emoji_mutex;
static std::unordered_map<std::string, std::string> emoji_map;
static bool loaded = false;

static std::filesystem::path cache_path() {
  return platform_paths().cache_dir / "emoji.json";
}

static bool load_from_json(const QByteArray& json_data) {
  QJsonParseError parse_error;
  auto doc = QJsonDocument::fromJson(json_data, &parse_error);
  if (!doc.isObject()) {
    log::client()->warn("emoji: invalid JSON ({})",
                        parse_error.errorString().toStdString());
    return false;
  }
  auto obj = doc.object();
  std::unordered_map<std::string, std::string> new_map;
  new_map.reserve(static_cast<size_t>(obj.size()));
  for (auto it = obj.begin(); it != obj.end(); ++it) {
    new_map.emplace(it.key().toStdString(), it.value().toString().toStdString());
  }
  {
    std::unique_lock lock(emoji_mutex);
    emoji_map = std::move(new_map);
    loaded = true;
  }
  log::client()->debug("emoji: loaded {} shortcodes", emoji_map.size());
  return true;
}

static QByteArray parse_typescript_to_json(const QByteArray& ts_data) {
  // Parse TypeScript key-value pairs: "name": "emoji",
  QRegularExpression re(R"~~("([a-zA-Z0-9_]+)":\s*"(.+?)")~~");
  QJsonObject obj;
  auto it = re.globalMatch(QString::fromUtf8(ts_data));
  int count = 0;
  while (it.hasNext()) {
    auto match = it.next();
    auto key = match.captured(1);
    auto value = match.captured(2);
    if (!obj.contains(key)) { // first occurrence wins
      obj.insert(key, value);
      ++count;
    }
  }
  log::client()->debug("emoji: parsed {} shortcodes from TypeScript source",
                        count);
  return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

static void load_emoji_map() {
  std::unique_lock lock(emoji_mutex);
  if (loaded) return;
  lock.unlock(); // Don't hold lock during file I/O

  auto path = cache_path();
  QFile cache_file(QString::fromStdString(path.string()));
  if (cache_file.open(QIODevice::ReadOnly)) {
    auto data = cache_file.readAll();
    cache_file.close();
    if (load_from_json(data)) {
      log::client()->debug("emoji: loaded from cache ({})", path.string());
      return;
    }
  }

  log::client()->debug(
      "emoji: no cached data, will download on background thread");
  // Mark as loaded with empty map so we don't block.
  // The background download will populate it when done.
  std::unique_lock mark_lock(emoji_mutex);
  loaded = true;
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

void download_emoji_data(QNetworkAccessManager* nam) {
  static const QString url =
      "https://raw.githubusercontent.com/xCykrix/discord_emoji/main/mod.ts";

  log::client()->debug("emoji: downloading from {}", url.toStdString());

  auto* reply = nam->get(QNetworkRequest(QUrl(url)));
  QObject::connect(reply, &QNetworkReply::finished, [reply]() {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
      log::client()->warn("emoji: download failed: {}",
                          reply->errorString().toStdString());
      return;
    }

    auto ts_data = reply->readAll();
    auto json_data = parse_typescript_to_json(ts_data);

    if (json_data.isEmpty()) {
      log::client()->warn(
          "emoji: parsed empty data from TypeScript source");
      return;
    }

    // Save to cache
    auto path = cache_path();
    std::filesystem::create_directories(path.parent_path());
    QFile file(QString::fromStdString(path.string()));
    if (file.open(QIODevice::WriteOnly)) {
      file.write(json_data);
      file.close();
      log::client()->debug("emoji: saved {} bytes to cache ({})",
                            json_data.size(), path.string());
    } else {
      log::client()->warn("emoji: failed to write cache file");
    }

    // Load into memory
    if (load_from_json(json_data)) {
      log::client()->info("emoji: downloaded and loaded {} shortcodes",
                          emoji_map.size());
    }
  });
}

} // namespace kind
