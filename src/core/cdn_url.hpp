#pragma once

#include <algorithm>
#include <string>
#include <utility>

namespace kind::cdn_url {

/// Scale dimensions to fit within max_w x max_h while preserving aspect ratio.
/// Returns the constrained {width, height} pair.
inline std::pair<int, int> constrain_dimensions(int width, int height,
                                                 int max_w, int max_h) {
  width = std::max(width, 1);
  height = std::max(height, 1);
  if (width > max_w) {
    height = height * max_w / width;
    width = max_w;
  }
  if (height > max_h) {
    width = width * max_h / height;
    height = max_h;
  }
  return {std::max(width, 1), std::max(height, 1)};
}

/// Append appropriate size/dimension parameters to Discord image URLs.
///
/// Asset endpoints (icons, avatars, stickers, etc.) on cdn.discordapp.com
/// use `?size=N` with a power-of-2 value clamped to [16, 4096].
///
/// Attachment endpoints on cdn.discordapp.com use `&width=W&height=H`
/// because they are pre-signed and `?size=` is silently ignored.
///
/// Proxy endpoints (discordapp.net, discord.com) use `width=W&height=H`.
///
/// Non-Discord URLs are returned unchanged.
inline std::string add_image_size(const std::string& url, int display_width, int display_height = 0) {
  if (url.empty()) {
    return url;
  }

  display_width = std::max(display_width, 1);
  if (display_height <= 0) {
    display_height = display_width;
  }

  // If URL already has query params, use & as separator.
  // Skip separator entirely if URL already ends with & or ?.
  std::string sep;
  if (url.back() == '&' || url.back() == '?') {
    sep = "";
  } else if (url.contains('?')) {
    sep = "&";
  } else {
    sep = "?";
  }

  if (url.contains("cdn.discordapp.com")) {
    // Attachment URLs are pre-signed and ignore ?size=, so they need
    // width/height parameters instead.
    if (url.contains("/attachments/")) {
      return url + sep + "width=" + std::to_string(display_width)
             + "&height=" + std::to_string(display_height);
    }

    // Asset endpoint: use power-of-2 size.
    int size = 16;
    if (display_width > 0) {
      while (size < display_width && size < 4096) {
        size *= 2;
      }
    }
    return url + sep + "size=" + std::to_string(size);
  }

  if (url.contains("discordapp.net")
      || url.contains("discord.com")) {
    return url + sep + "width=" + std::to_string(display_width)
           + "&height=" + std::to_string(display_height);
  }

  return url;
}

/// Strip volatile signature parameters from Discord attachment URLs to produce
/// a stable cache key. The path plus size params (width, height, format)
/// uniquely identify the content. Non-attachment URLs are returned unchanged.
inline std::string normalize_cache_key(const std::string& url) {
  // Only normalize Discord attachment URLs (they have rotating signatures)
  bool is_discord = url.contains("cdn.discordapp.com")
                    || url.contains("discordapp.net")
                    || url.contains("discord.com");
  if (!is_discord || !url.contains("/attachments/")) {
    return url;
  }

  auto qpos = url.find('?');
  if (qpos == std::string::npos) {
    return url;
  }

  std::string base = url.substr(0, qpos);
  std::string_view query(url.data() + qpos + 1, url.size() - qpos - 1);

  // Keep only content-affecting params: width, height, format
  std::string kept;
  size_t pos = 0;
  while (pos < query.size()) {
    auto amp = query.find('&', pos);
    auto param = query.substr(pos, amp == std::string_view::npos
                                       ? std::string_view::npos
                                       : amp - pos);

    if (param.starts_with("width=") || param.starts_with("height=")
        || param.starts_with("format=")) {
      if (!kept.empty()) kept += '&';
      kept += param;
    }

    if (amp == std::string_view::npos) break;
    pos = amp + 1;
  }

  return kept.empty() ? base : base + "?" + kept;
}

} // namespace kind::cdn_url
