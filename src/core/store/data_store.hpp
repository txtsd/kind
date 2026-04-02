#pragma once
#include "interfaces/observer_list.hpp"
#include "interfaces/store_observer.hpp"
#include "models/channel.hpp"
#include "models/guild.hpp"
#include "models/message.hpp"
#include "models/snowflake.hpp"
#include "models/user.hpp"

#include <deque>
#include <map>
#include <optional>
#include <shared_mutex>
#include <unordered_set>
#include <vector>

namespace kind {

class DataStore {
public:
  explicit DataStore(std::size_t max_messages_per_channel = 500);

  // Thread-safe reads (return copies)
  std::vector<Guild> guilds() const;
  std::optional<Guild> guild(Snowflake guild_id) const;
  std::vector<Channel> channels(Snowflake guild_id) const;
  std::vector<Message> messages(Snowflake channel_id) const;
  std::vector<Message> messages(Snowflake channel_id,
                                std::optional<Snowflake> before,
                                int limit) const;
  std::optional<User> user(Snowflake user_id) const;
  std::optional<User> current_user() const;
  std::vector<User> all_users() const;

  // Private channels (DMs)
  std::vector<Channel> private_channels() const;
  void upsert_private_channel(Channel channel);
  void remove_private_channel(Snowflake id);
  void bulk_upsert_private_channels(std::vector<Channel> channels);
  void update_private_channel_last_message(Snowflake channel_id, Snowflake message_id);

  // Guild ordering (from user_settings_proto)
  void set_guild_order(const std::vector<Snowflake>& ordered_ids);

  // Member roles (from merged_members in READY)
  void set_member_roles(Snowflake guild_id, std::vector<Snowflake> role_ids);
  std::vector<Snowflake> member_roles(Snowflake guild_id) const;

  // Channel-to-guild reverse lookup
  Snowflake guild_id_for_channel(Snowflake channel_id) const;

  // Mutations (called by gateway event handlers)
  void set_current_user(User user);
  void upsert_guild(Guild guild);
  void bulk_upsert_guilds(std::vector<Guild> guilds);
  void remove_guild(Snowflake id);
  void upsert_channel(Channel channel);
  void bulk_upsert_channels(Snowflake guild_id, std::vector<Channel> channels);
  void remove_channel(Snowflake id);
  void add_message(Message msg);
  void update_message(Message msg);
  std::optional<Message> update_reaction(Snowflake channel_id, Snowflake message_id,
                                         const std::string& emoji_name,
                                         std::optional<Snowflake> emoji_id,
                                         int delta, bool me_changed);
  void remove_message(Snowflake channel_id, Snowflake message_id);
  void set_messages(Snowflake channel_id, std::vector<Message> msgs);
  void add_messages_before(Snowflake channel_id, std::vector<Message> msgs);

  // Observer management
  void add_observer(StoreObserver* obs);
  void remove_observer(StoreObserver* obs);

  // Suppress observer notifications during bulk loading.
  // While suppressed, mutations still update internal state
  // but don't fire on_guilds_updated / on_channels_updated.
  void set_suppress_observers(bool suppress);

private:
  mutable std::shared_mutex mutex_;
  std::size_t max_messages_per_channel_;

  User current_user_;
  std::map<Snowflake, Guild> guilds_;
  std::map<Snowflake, std::vector<Channel>> guild_channels_;
  std::map<Snowflake, std::deque<Message>> channel_messages_;
  std::map<Snowflake, User> users_;
  std::vector<Snowflake> guild_order_;
  std::vector<Channel> private_channels_;
  std::map<Snowflake, std::vector<Snowflake>> member_roles_;
  std::map<Snowflake, std::unordered_set<Snowflake>> pending_deletes_;
  bool suppress_observers_{false};

  ObserverList<StoreObserver> observers_;

  // Build ordered guild snapshot with channels re-attached.
  // Must be called with mutex_ held.
  std::vector<Guild> build_guild_snapshot_locked() const;
};

} // namespace kind
