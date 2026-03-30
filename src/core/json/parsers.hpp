#pragma once

#include "models/channel.hpp"
#include "models/guild.hpp"
#include "models/message.hpp"
#include "models/permission_overwrite.hpp"
#include "models/role.hpp"
#include "models/user.hpp"

#include <optional>
#include <QJsonObject>
#include <string>

namespace kind::json_parse {

// Parse a User from a JSON object. Returns std::nullopt on invalid JSON.
std::optional<User> parse_user(const QJsonObject& obj);

// Parse a User from a raw JSON string. Returns std::nullopt on parse failure.
std::optional<User> parse_user(const std::string& json);

// Parse a Guild from a JSON object. Returns std::nullopt on invalid JSON.
std::optional<Guild> parse_guild(const QJsonObject& obj);

// Parse a Channel from a JSON object. Returns std::nullopt on invalid JSON.
std::optional<Channel> parse_channel(const QJsonObject& obj);

// Parse a Message from a JSON object. Returns std::nullopt on invalid JSON.
std::optional<Message> parse_message(const QJsonObject& obj);

// Parse a Message from a raw JSON string. Returns std::nullopt on parse failure.
std::optional<Message> parse_message(const std::string& json);

// Parse a Role from a JSON object. Returns std::nullopt on invalid JSON.
std::optional<Role> parse_role(const QJsonObject& obj);

// Parse a PermissionOverwrite from a JSON object. Returns std::nullopt on invalid JSON.
std::optional<PermissionOverwrite> parse_overwrite(const QJsonObject& obj);

} // namespace kind::json_parse
