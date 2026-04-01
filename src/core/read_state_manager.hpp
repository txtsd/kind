#pragma once

#include "models/snowflake.hpp"

#include <QObject>

#include <unordered_map>
#include <vector>

namespace kind {

enum class UnreadQualifier {
  Exact,    // "5"  - known count
  AtLeast,  // "5+" - more messages may have arrived
  Unknown,  // "?"  - read elsewhere, count is stale
};

struct ReadState {
  Snowflake last_read_id{0};
  int mention_count{0};
  int unread_count{0};
  Snowflake last_message_id{0};
  UnreadQualifier qualifier{UnreadQualifier::Exact};
};

class ReadStateManager : public QObject {
  Q_OBJECT

public:
  explicit ReadStateManager(QObject* parent = nullptr);

  // Bulk load from READY payload or database
  void load_read_states(const std::vector<std::pair<Snowflake, ReadState>>& states);

  // Reconcile cached states against READY payload data
  void reconcile_ready(const std::vector<std::pair<Snowflake, ReadState>>& ready_states,
                       const std::unordered_map<Snowflake, Snowflake>& channel_last_message_ids);

  // Query
  ReadState state(Snowflake channel_id) const;
  UnreadQualifier qualifier(Snowflake channel_id) const;
  int unread_count(Snowflake channel_id) const;
  int mention_count(Snowflake channel_id) const;
  bool has_unreads(Snowflake channel_id) const;
  const std::unordered_map<Snowflake, ReadState>& all_states() const { return states_; }

  // Compute guild-level aggregates
  int guild_unread_channels(const std::vector<Snowflake>& channel_ids) const;
  int guild_mention_count(const std::vector<Snowflake>& channel_ids) const;
  UnreadQualifier guild_qualifier(const std::vector<Snowflake>& channel_ids) const;

  // Mutations
  void mark_read(Snowflake channel_id, Snowflake message_id);
  void increment_unread(Snowflake channel_id, Snowflake message_id);
  void increment_mention(Snowflake channel_id, int count = 1);
  void set_mention_count(Snowflake channel_id, int count);

signals:
  void unread_changed(kind::Snowflake channel_id);
  void mention_changed(kind::Snowflake channel_id);
  void persist_requested(kind::Snowflake channel_id, kind::ReadState state);
  void bulk_loaded();

private:
  std::unordered_map<Snowflake, ReadState> states_;
};

} // namespace kind

Q_DECLARE_METATYPE(kind::ReadState)
