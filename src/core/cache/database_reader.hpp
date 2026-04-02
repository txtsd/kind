#pragma once

#include "models/channel.hpp"
#include "models/guild.hpp"
#include "models/message.hpp"
#include "models/permission_overwrite.hpp"
#include "models/role.hpp"
#include "models/snowflake.hpp"
#include "models/user.hpp"
#include "read_state_manager.hpp"

#include <QString>

#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace kind {

class DatabaseReader {
public:
  explicit DatabaseReader(const std::string& db_path);
  ~DatabaseReader();

  DatabaseReader(const DatabaseReader&) = delete;
  DatabaseReader& operator=(const DatabaseReader&) = delete;

  std::vector<Guild> guilds() const;
  std::vector<Snowflake> guild_order() const;
  std::vector<Channel> channels(Snowflake guild_id) const;
  std::unordered_map<Snowflake, std::vector<Channel>> all_guild_channels() const;
  std::vector<Role> roles(Snowflake guild_id) const;
  std::unordered_map<Snowflake, std::vector<Role>> all_roles() const;
  std::vector<PermissionOverwrite> permission_overwrites(Snowflake channel_id) const;
  std::unordered_map<Snowflake, std::vector<PermissionOverwrite>> all_permission_overwrites() const;
  std::vector<Snowflake> member_roles(Snowflake guild_id) const;
  std::unordered_map<Snowflake, std::vector<Snowflake>> all_member_roles() const;
  std::optional<User> current_user() const;
  std::vector<Message> messages(Snowflake channel_id,
                                std::optional<Snowflake> before = {},
                                int limit = 50) const;
  std::vector<std::pair<Snowflake, ReadState>> read_states() const;
  std::vector<Channel> dm_channels() const;
  std::vector<std::tuple<Snowflake, int, bool>> mute_states() const;
  std::optional<std::string> app_state(const std::string& key) const;

  std::string db_path() const { return db_path_; }

private:
  std::string db_path_;
  QString connection_name_;
};

} // namespace kind
