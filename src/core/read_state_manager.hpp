#pragma once

#include "models/snowflake.hpp"

#include <QObject>

#include <unordered_map>
#include <vector>

namespace kind {

struct ReadState {
  Snowflake last_read_id{0};
  int mention_count{0};
  int unread_count{0};
};

class ReadStateManager : public QObject {
  Q_OBJECT

public:
  explicit ReadStateManager(QObject* parent = nullptr);

  // Bulk load from READY payload or database
  void load_read_states(const std::vector<std::pair<Snowflake, ReadState>>& states);

  // Query
  ReadState state(Snowflake channel_id) const;
  int unread_count(Snowflake channel_id) const;
  int mention_count(Snowflake channel_id) const;
  bool has_unreads(Snowflake channel_id) const;

  // Compute guild-level aggregates
  int guild_unread_channels(const std::vector<Snowflake>& channel_ids) const;
  int guild_mention_count(const std::vector<Snowflake>& channel_ids) const;

  // Mutations
  void mark_read(Snowflake channel_id, Snowflake message_id);
  void increment_unread(Snowflake channel_id);
  void increment_mention(Snowflake channel_id, int count = 1);
  void set_mention_count(Snowflake channel_id, int count);

signals:
  void unread_changed(kind::Snowflake channel_id);
  void mention_changed(kind::Snowflake channel_id);

private:
  std::unordered_map<Snowflake, ReadState> states_;
};

} // namespace kind
