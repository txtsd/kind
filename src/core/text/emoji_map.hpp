#pragma once

#include <string>

class QNetworkAccessManager;

namespace kind {

// Replace emoji shortcodes like :eggplant: with their Unicode equivalents.
// Loads shortcode data from cached JSON (downloaded from GitHub on first launch).
// Unknown shortcodes are left as-is.
void replace_emoji_shortcodes(std::string& text);

// Reload the emoji map from disk (e.g. after downloading an update).
void reload_emoji_map();

// Download emoji data from GitHub, parse TypeScript source to JSON, save to cache.
// Uses the provided QNetworkAccessManager for the HTTP request. The download and
// parsing happen asynchronously; the emoji map is populated when the reply arrives.
void download_emoji_data(QNetworkAccessManager* nam);

} // namespace kind
