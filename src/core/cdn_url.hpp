#pragma once

#include <algorithm>
#include <string>

namespace kind::cdn_url {

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

} // namespace kind::cdn_url
