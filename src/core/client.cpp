#include "client.hpp"

#include "auth/auth_manager.hpp"
#include "auth/keychain_token_store.hpp"
#include "cache/database_manager.hpp"
#include "cache/database_reader.hpp"
#include "cache/database_writer.hpp"
#include "config/platform_paths.hpp"
#include "config/config_manager.hpp"
#include "gateway/gateway_client.hpp"
#include "gateway/gateway_events.hpp"
#include "gateway/qt_gateway_client.hpp"
#include "rest/endpoints.hpp"
#include "rest/qt_rest_client.hpp"
#include "rest/rest_client.hpp"
#include "store/data_store.hpp"

#include "json/parsers.hpp"
#include "discord_user_settings.qpb.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProtobufSerializer>
#include <QTimer>
#include <QFutureWatcher>
#include <QtConcurrent>
#include "logging.hpp"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace kind {

static Snowflake safe_stoull(const std::string& str) {
  Snowflake result = 0;
  auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), result);
  return (ec == std::errc{}) ? result : 0;
}

// ============================================================
// AuthBridge: translates AuthManager events into gateway
// actions and forwards notifications to external observers
// ============================================================

class Client::AuthBridge : public AuthObserver {
public:
  AuthBridge(Client& client) : client_(client) {}

  void on_login_success(const User& user) override {
    client_.store_->set_current_user(user);

    // Initialize per-account database now that we know the user ID
    if (!client_.test_mode_ && !client_.db_manager_) {
      client_.init_account_db(user.id);
      client_.load_cache();
    }

    if (client_.db_writer_) {
      emit client_.db_writer_->current_user_write_requested(user);
    }

    auto token = client_.auth_->token();
    auto token_type = client_.auth_->token_type();
    bool is_bot = (token_type == "bot" || token_type == "Bot");
    client_.gateway_->set_bot_mode(is_bot);

    auto gateway_url = client_.config_.get_or<std::string>("network.gateway_url", "");
    if (gateway_url.empty()) {
      gateway_url = "wss://gateway.discord.gg/";
    }
    log::client()->debug("connect: starting gateway connection (token length={})", token.size());
    client_.gateway_->connect(gateway_url, token);

    client_.auth_observers_.notify([&user](AuthObserver* obs) { obs->on_login_success(user); });
  }

  void on_login_failure(std::string_view reason) override {
    client_.auth_observers_.notify([reason](AuthObserver* obs) { obs->on_login_failure(reason); });
  }

  void on_mfa_required() override {
    client_.auth_observers_.notify([](AuthObserver* obs) { obs->on_mfa_required(); });
  }

  void on_logout() override {
    client_.gateway_->disconnect();
    client_.auth_observers_.notify([](AuthObserver* obs) { obs->on_logout(); });
  }

private:
  Client& client_;
};

// ============================================================
// GatewayBridge: parses gateway events, updates DataStore,
// and forwards structured notifications to external observers
// ============================================================

class Client::GatewayBridge {
public:
  GatewayBridge(Client& client) : client_(client) {}

  void on_event(std::string_view event_name, const std::string& data_json) {
    if (event_name == gateway::events::Ready) {
      handle_ready(data_json);
    } else if (event_name == gateway::events::MessageCreate) {
      handle_message_create(data_json);
    } else if (event_name == gateway::events::MessageUpdate) {
      handle_message_update(data_json);
    } else if (event_name == gateway::events::MessageDelete) {
      handle_message_delete(data_json);
    } else if (event_name == gateway::events::GuildCreate) {
      handle_guild_create(data_json);
    } else if (event_name == gateway::events::ChannelUpdate) {
      handle_channel_update(data_json);
    } else if (event_name == gateway::events::MessageReactionAdd) {
      handle_reaction_add(data_json);
    } else if (event_name == gateway::events::MessageReactionRemove) {
      handle_reaction_remove(data_json);
    } else if (event_name == gateway::events::TypingStart) {
      handle_typing_start(data_json);
    } else if (event_name == gateway::events::PresenceUpdate) {
      handle_presence_update(data_json);
    } else if (event_name == "__DISCONNECT") {
      client_.gateway_observers_.notify([&data_json](GatewayObserver* obs) { obs->on_gateway_disconnect(data_json); });
    } else if (event_name == "__RECONNECTING") {
      client_.gateway_observers_.notify([](GatewayObserver* obs) { obs->on_gateway_reconnecting(); });
    }
  }

private:
  // Parsed READY payload data, extracted on a worker thread.
  struct ReadyData {
    std::vector<Guild> guilds;
    std::unordered_map<Snowflake, std::vector<Snowflake>> member_roles;
    QByteArray settings_proto; // raw protobuf bytes for guild ordering
    std::vector<std::pair<Snowflake, ReadState>> read_states;
    std::unordered_map<Snowflake, Snowflake> channel_last_message_ids;
    std::vector<GuildMuteSettings> mute_settings;
    std::vector<Channel> private_channels;
  };

  void handle_ready(const std::string& data_json) {
    // Copy the JSON payload into a shared buffer so the worker thread
    // owns its own data and the gateway can reuse its receive buffer.
    auto shared_json = std::make_shared<std::string>(data_json);

    auto future = QtConcurrent::run([shared_json]() -> std::shared_ptr<ReadyData> {
      log::client()->debug("READY: parsing on worker thread");
      auto doc = QJsonDocument::fromJson(
          QByteArray::fromRawData(shared_json->data(), shared_json->size()));
      if (doc.isNull() || !doc.isObject()) {
        log::client()->warn("Failed to parse READY JSON on worker thread");
        return nullptr;
      }
      auto obj = doc.object();

      auto data = std::make_shared<ReadyData>();

      // Parse guilds
      auto guilds_array = obj["guilds"].toArray();
      data->guilds.reserve(guilds_array.size());
      for (const auto& val : guilds_array) {
        auto guild_obj = val.toObject();
        if (guild_obj["unavailable"].toBool(false)) {
          continue;
        }
        // Extract last_message_id from each channel before parse_guild
        auto guild_channels = guild_obj["channels"].toArray();
        for (const auto& ch_val : guild_channels) {
          auto ch_obj = ch_val.toObject();
          auto ch_id = static_cast<Snowflake>(ch_obj["id"].toString().toULongLong());
          auto lmid = static_cast<Snowflake>(ch_obj["last_message_id"].toString().toULongLong());
          if (ch_id != 0 && lmid != 0) {
            data->channel_last_message_ids[ch_id] = lmid;
          }
        }

        auto guild = json_parse::parse_guild(guild_obj);
        if (guild) {
          data->guilds.push_back(std::move(*guild));
        }
      }

      // Parse merged_members for current user's role IDs per guild.
      // merged_members indices correspond to guilds_array (including
      // unavailable entries), not the filtered guilds vector.
      auto merged_members = obj["merged_members"].toArray();
      for (int i = 0; i < merged_members.size() && i < guilds_array.size(); ++i) {
        auto guild_obj = guilds_array[i].toObject();
        if (guild_obj["unavailable"].toBool(false)) {
          continue;
        }
        auto guild_id = static_cast<Snowflake>(guild_obj["id"].toString().toULongLong());

        auto member_array = merged_members[i].toArray();
        if (member_array.isEmpty()) {
          continue;
        }
        auto member_obj = member_array[0].toObject();
        auto roles_array = member_obj["roles"].toArray();
        std::vector<Snowflake> role_ids;
        role_ids.reserve(roles_array.size());
        for (const auto& rv : roles_array) {
          role_ids.push_back(static_cast<Snowflake>(rv.toString().toULongLong()));
        }
        data->member_roles[guild_id] = std::move(role_ids);
      }

      // Extract raw protobuf bytes for guild ordering (decoded on main thread)
      auto settings_proto_b64 = obj["user_settings_proto"].toString();
      if (!settings_proto_b64.isEmpty()) {
        data->settings_proto = QByteArray::fromBase64(settings_proto_b64.toUtf8());
      }

      // Parse read_state entries
      {
        auto read_state_obj = obj["read_state"].toObject();
        auto entries = read_state_obj["entries"].toArray();
        if (entries.isEmpty()) {
          entries = obj["read_state"].toArray();
        }
        for (const auto& val : entries) {
          auto entry = val.toObject();
          auto channel_id = static_cast<Snowflake>(entry["id"].toString().toULongLong());
          if (channel_id == 0) continue;
          ReadState rs;
          rs.last_read_id = static_cast<Snowflake>(entry["last_message_id"].toString().toULongLong());
          rs.mention_count = entry["mention_count"].toInt(0);
          data->read_states.emplace_back(channel_id, rs);
        }
      }

      // Parse user_guild_settings for mute state
      {
        auto settings_obj = obj["user_guild_settings"].toObject();
        auto entries = settings_obj["entries"].toArray();
        if (entries.isEmpty()) {
          entries = obj["user_guild_settings"].toArray();
        }
        for (const auto& val : entries) {
          auto entry = val.toObject();
          GuildMuteSettings gs;
          gs.guild_id = static_cast<Snowflake>(entry["guild_id"].toString().toULongLong());
          gs.muted = entry["muted"].toBool(false);

          auto overrides = entry["channel_overrides"].toArray();
          gs.channel_overrides.reserve(overrides.size());
          for (const auto& ov : overrides) {
            auto ov_obj = ov.toObject();
            GuildMuteSettings::ChannelOverride co;
            co.channel_id = static_cast<Snowflake>(ov_obj["channel_id"].toString().toULongLong());
            co.muted = ov_obj["muted"].toBool(false);
            gs.channel_overrides.push_back(co);
          }
          data->mute_settings.push_back(std::move(gs));
        }
      }

      // Parse private channels (DMs)
      auto private_channels_array = obj["private_channels"].toArray();
      for (const auto& val : private_channels_array) {
        auto channel = json_parse::parse_channel(val.toObject());
        if (channel && channel->type == 1) {  // Only 1-on-1 DMs for now
          data->private_channels.push_back(std::move(*channel));
        }
      }

      for (const auto& pc : data->private_channels) {
        if (pc.id != 0 && pc.last_message_id != 0) {
          data->channel_last_message_ids[pc.id] = pc.last_message_id;
        }
      }

      log::client()->debug("READY: parsed {} private channels", data->private_channels.size());

      log::client()->debug("READY: parsed {} guilds, {} read_states, {} mute_settings, {} channel_last_message_ids",
                           data->guilds.size(), data->read_states.size(),
                           data->mute_settings.size(), data->channel_last_message_ids.size());
      return data;
    });

    // Deliver results back to the main thread via QFutureWatcher,
    // using read_state_manager_ as the QObject context (same pattern
    // as load_cache).
    auto* rsm = client_.read_state_manager_.get();
    auto* watcher = new QFutureWatcher<std::shared_ptr<ReadyData>>(rsm);
    auto* store = client_.store_.get();
    auto* msm = client_.mute_state_manager_.get();
    auto* dbw = client_.db_writer_.get();
    auto& observers = client_.gateway_observers_;

    QObject::connect(watcher, &QFutureWatcher<std::shared_ptr<ReadyData>>::finished,
                     rsm, [watcher, store, rsm, msm, dbw, &observers]() {
      watcher->deleteLater();
      auto data = watcher->result();
      if (!data) {
        return;
      }

      // Emit DB write signals for each guild (before bulk upsert moves the data)
      if (dbw) {
        for (const auto& guild : data->guilds) {
          emit dbw->guild_write_requested(guild);
          emit dbw->roles_write_requested(guild.id, guild.roles);
          for (const auto& ch : guild.channels) {
            emit dbw->channel_write_requested(ch);
            if (!ch.permission_overwrites.empty()) {
              emit dbw->overwrites_write_requested(ch.id, ch.permission_overwrites);
            }
          }
        }
      }

      // Collect fresh guild IDs before bulk upsert moves the data
      std::unordered_set<Snowflake> fresh_ids;
      fresh_ids.reserve(data->guilds.size());
      for (const auto& guild : data->guilds) {
        fresh_ids.insert(guild.id);
      }

      // Bulk upsert into store (fires observer once instead of per guild)
      auto guilds_count = data->guilds.size();
      store->bulk_upsert_guilds(std::move(data->guilds));
      log::client()->debug("READY: upserted {} guilds into store", guilds_count);

      // Reconcile: remove guilds no longer present in READY
      {
        auto old_guilds = store->guilds();
        for (const auto& old : old_guilds) {
          if (fresh_ids.find(old.id) == fresh_ids.end()) {
            store->remove_guild(old.id);
            if (dbw) {
              emit dbw->guild_delete_requested(old.id);
            }
          }
        }
      }

      // Apply member roles
      for (auto& [guild_id, role_ids] : data->member_roles) {
        if (dbw) {
          emit dbw->member_roles_write_requested(guild_id, role_ids);
        }
        store->set_member_roles(guild_id, std::move(role_ids));
      }

      log::client()->debug("READY: set member roles for {} guilds", data->member_roles.size());

      // Decode guild ordering from protobuf (fast, stays on main thread)
      if (!data->settings_proto.isEmpty()) {
        kind::proto::PreloadedUserSettings settings;
        QProtobufSerializer serializer;
        if (serializer.deserialize(&settings, data->settings_proto)) {
          if (settings.hasGuildFolders()) {
            auto& gf = settings.guildFolders();

            std::vector<Snowflake> folder_ids;
            std::set<Snowflake> in_folder;
            for (const auto& folder : gf.folders()) {
              for (auto gid : folder.guildIds()) {
                auto id = static_cast<Snowflake>(gid);
                folder_ids.push_back(id);
                in_folder.insert(id);
              }
            }

            std::vector<Snowflake> unsorted;
            for (auto id : fresh_ids) {
              if (in_folder.find(id) == in_folder.end()) {
                unsorted.push_back(id);
              }
            }
            std::reverse(unsorted.begin(), unsorted.end());

            std::vector<Snowflake> ordered_ids;
            ordered_ids.reserve(unsorted.size() + folder_ids.size());
            ordered_ids.insert(ordered_ids.end(), unsorted.begin(), unsorted.end());
            ordered_ids.insert(ordered_ids.end(), folder_ids.begin(), folder_ids.end());

            if (!ordered_ids.empty()) {
              store->set_guild_order(ordered_ids);
              log::client()->debug("READY: applied guild ordering ({} guilds)", ordered_ids.size());
              if (dbw) {
                emit dbw->guild_order_write_requested(ordered_ids);
              }
            }
          }
        } else {
          log::client()->warn("Failed to decode user_settings_proto: {}",
                       serializer.lastErrorString().toStdString());
        }
      }

      // Reconcile read states against cached data and READY payload
      // (done before observer notification so recompute_all_caches has correct data)
      if (!data->read_states.empty()) {
        log::client()->debug("READY: reconciling {} read_states against {} channel_last_message_ids",
                             data->read_states.size(), data->channel_last_message_ids.size());
        rsm->reconcile_ready(data->read_states, data->channel_last_message_ids);
        log::client()->debug("READY: persisting {} reconciled read_states", rsm->all_states().size());
        if (dbw) {
          for (const auto& [cid, rs] : rsm->all_states()) {
            emit dbw->read_state_write_requested(
                cid, rs.last_read_id, rs.mention_count,
                rs.unread_count, rs.last_message_id);
          }
        }
      }

      // Load mute states (before observer notification for same reason)
      if (!data->mute_settings.empty()) {
        msm->load_guild_settings(data->mute_settings);
        log::client()->debug("READY: loaded {} mute_settings", data->mute_settings.size());
        if (dbw) {
          std::vector<std::tuple<Snowflake, int, bool>> db_entries;
          for (const auto& gs : data->mute_settings) {
            if (gs.guild_id != 0) {
              db_entries.emplace_back(gs.guild_id, 0, gs.muted);
            }
            for (const auto& co : gs.channel_overrides) {
              if (co.channel_id != 0) {
                db_entries.emplace_back(co.channel_id, 1, co.muted);
              }
            }
          }
          if (!db_entries.empty()) {
            emit dbw->mute_state_bulk_write_requested(std::move(db_entries));
          }
        }
      }

      // Upsert private channels (DMs) into store
      if (!data->private_channels.empty()) {
        auto pc_count = data->private_channels.size();
        store->bulk_upsert_private_channels(std::move(data->private_channels));
        log::client()->debug("READY: upserted {} private channels", pc_count);
        if (dbw) {
          for (const auto& pc : store->private_channels()) {
            emit dbw->channel_write_requested(pc);
            emit dbw->dm_recipients_write_requested(pc.id, pc.recipients);
          }
        }
      }

      // Notify observers with the final guild list (respecting ordering)
      // Read state and mute state are already populated, so recompute_all_caches
      // will have correct data on the first pass.
      auto guilds = store->guilds();
      log::client()->debug("READY: notifying observers with {} guilds", guilds.size());
      observers.notify([&guilds](GatewayObserver* obs) { obs->on_ready(guilds); });
    });

    watcher->setFuture(future);
  }

  void handle_message_create(const std::string& data_json) {
    auto msg = json_parse::parse_message(data_json);
    if (!msg) {
      return;
    }
    log::client()->debug("MESSAGE_CREATE: channel={}, id={}, author={}", msg->channel_id, msg->id, msg->author.username);
    client_.store_->add_message(*msg);
    if (client_.db_writer_) {
      emit client_.db_writer_->message_write_requested(*msg);
    }

    // Track unread state for channels that are not currently active
    if (msg->channel_id != client_.active_channel_id_.load()) {
      log::client()->debug("MESSAGE_CREATE: channel {} not active, incrementing unread", msg->channel_id);
      client_.read_state_manager_->increment_unread(msg->channel_id, msg->id);

      // Check if this message mentions the current user
      auto current = client_.store_->current_user();
      if (current) {
        bool mentioned = msg->mention_everyone;
        if (!mentioned) {
          for (const auto& m : msg->mentions) {
            if (m.id == current->id) {
              mentioned = true;
              break;
            }
          }
        }
        if (mentioned) {
          log::client()->debug("MESSAGE_CREATE: mention detected in channel {}", msg->channel_id);
          client_.read_state_manager_->increment_mention(msg->channel_id);
        }
      }
    }

    client_.gateway_observers_.notify([&msg](GatewayObserver* obs) { obs->on_message_create(*msg); });
  }

  void handle_message_update(const std::string& data_json) {
    auto msg = json_parse::parse_message(data_json);
    if (!msg) {
      return;
    }
    log::client()->debug("MESSAGE_UPDATE: channel={}, id={}", msg->channel_id, msg->id);
    client_.store_->update_message(*msg);
    if (client_.db_writer_) {
      emit client_.db_writer_->message_write_requested(*msg);
    }
    client_.gateway_observers_.notify([&msg](GatewayObserver* obs) { obs->on_message_update(*msg); });
  }

  void handle_message_delete(const std::string& data_json) {
    auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(data_json));
    if (doc.isNull() || !doc.isObject()) {
      log::client()->warn("Failed to parse MESSAGE_DELETE JSON");
      return;
    }
    auto obj = doc.object();
    auto channel_id = static_cast<Snowflake>(obj["channel_id"].toString().toULongLong());
    auto message_id = static_cast<Snowflake>(obj["id"].toString().toULongLong());
    log::client()->debug("MESSAGE_DELETE: channel={}, id={}", channel_id, message_id);
    client_.store_->remove_message(channel_id, message_id);
    if (client_.db_writer_) {
      emit client_.db_writer_->message_delete_requested(channel_id, message_id);
    }
    client_.gateway_observers_.notify(
        [channel_id, message_id](GatewayObserver* obs) { obs->on_message_delete(channel_id, message_id); });
  }

  void handle_guild_create(const std::string& data_json) {
    auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(data_json));
    if (doc.isNull() || !doc.isObject()) {
      log::client()->warn("Failed to parse GUILD_CREATE JSON");
      return;
    }
    auto guild = json_parse::parse_guild(doc.object());
    if (!guild) {
      return;
    }
    log::client()->debug("GUILD_CREATE: id={}", guild->id);
    client_.store_->upsert_guild(*guild);
    if (client_.db_writer_) {
      emit client_.db_writer_->guild_write_requested(*guild);
      emit client_.db_writer_->roles_write_requested(guild->id, guild->roles);
      for (const auto& ch : guild->channels) {
        emit client_.db_writer_->channel_write_requested(ch);
        if (!ch.permission_overwrites.empty()) {
          emit client_.db_writer_->overwrites_write_requested(ch.id, ch.permission_overwrites);
        }
      }
    }
    client_.gateway_observers_.notify([&guild](GatewayObserver* obs) { obs->on_guild_create(*guild); });
  }

  void handle_channel_update(const std::string& data_json) {
    auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(data_json));
    if (doc.isNull() || !doc.isObject()) {
      log::client()->warn("Failed to parse CHANNEL_UPDATE JSON");
      return;
    }
    auto channel = json_parse::parse_channel(doc.object());
    if (!channel) {
      return;
    }
    log::client()->debug("CHANNEL_UPDATE: id={}, guild={}", channel->id, channel->guild_id);
    client_.store_->upsert_channel(*channel);
    if (client_.db_writer_) {
      emit client_.db_writer_->channel_write_requested(*channel);
      if (!channel->permission_overwrites.empty()) {
        emit client_.db_writer_->overwrites_write_requested(channel->id, channel->permission_overwrites);
      }
    }
    client_.gateway_observers_.notify([&channel](GatewayObserver* obs) { obs->on_channel_update(*channel); });
  }

  void handle_reaction_add(const std::string& data_json) {
    auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(data_json));
    if (doc.isNull() || !doc.isObject()) {
      return;
    }
    auto obj = doc.object();
    auto channel_id = static_cast<Snowflake>(obj["channel_id"].toString().toULongLong());
    auto message_id = static_cast<Snowflake>(obj["message_id"].toString().toULongLong());
    auto user_id = static_cast<Snowflake>(obj["user_id"].toString().toULongLong());
    auto emoji_obj = obj["emoji"].toObject();
    auto emoji_name = emoji_obj["name"].toString().toStdString();
    std::optional<Snowflake> emoji_id;
    if (!emoji_obj["id"].isNull()) {
      emoji_id = static_cast<Snowflake>(emoji_obj["id"].toString().toULongLong());
    }
    log::client()->debug("REACTION_ADD: channel={}, message={}, emoji={}", channel_id, message_id, emoji_name);
    auto current = client_.store_->current_user();
    bool is_me = current && current->id == user_id;
    auto updated = client_.store_->update_reaction(channel_id, message_id, emoji_name, emoji_id, 1, is_me);
    if (updated) {
      client_.gateway_observers_.notify([&updated](GatewayObserver* obs) { obs->on_message_update(*updated); });
    }
  }

  void handle_reaction_remove(const std::string& data_json) {
    auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(data_json));
    if (doc.isNull() || !doc.isObject()) {
      return;
    }
    auto obj = doc.object();
    auto channel_id = static_cast<Snowflake>(obj["channel_id"].toString().toULongLong());
    auto message_id = static_cast<Snowflake>(obj["message_id"].toString().toULongLong());
    auto user_id = static_cast<Snowflake>(obj["user_id"].toString().toULongLong());
    auto emoji_obj = obj["emoji"].toObject();
    auto emoji_name = emoji_obj["name"].toString().toStdString();
    std::optional<Snowflake> emoji_id;
    if (!emoji_obj["id"].isNull()) {
      emoji_id = static_cast<Snowflake>(emoji_obj["id"].toString().toULongLong());
    }
    log::client()->debug("REACTION_REMOVE: channel={}, message={}, emoji={}", channel_id, message_id, emoji_name);
    auto current = client_.store_->current_user();
    bool is_me = current && current->id == user_id;
    auto updated = client_.store_->update_reaction(channel_id, message_id, emoji_name, emoji_id, -1, is_me);
    if (updated) {
      client_.gateway_observers_.notify([&updated](GatewayObserver* obs) { obs->on_message_update(*updated); });
    }
  }

  void handle_typing_start(const std::string& data_json) {
    auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(data_json));
    if (doc.isNull() || !doc.isObject()) {
      log::client()->warn("Failed to parse TYPING_START JSON");
      return;
    }
    auto obj = doc.object();
    auto channel_id = static_cast<Snowflake>(obj["channel_id"].toString().toULongLong());
    auto user_id = static_cast<Snowflake>(obj["user_id"].toString().toULongLong());
    client_.gateway_observers_.notify(
        [channel_id, user_id](GatewayObserver* obs) { obs->on_typing_start(channel_id, user_id); });
  }

  void handle_presence_update(const std::string& data_json) {
    auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(data_json));
    if (doc.isNull() || !doc.isObject()) {
      log::client()->warn("Failed to parse PRESENCE_UPDATE JSON");
      return;
    }
    auto obj = doc.object();
    auto user_obj = obj["user"].toObject();
    auto user_id = static_cast<Snowflake>(user_obj["id"].toString().toULongLong());
    auto status = obj["status"].toString().toStdString();
    client_.gateway_observers_.notify(
        [user_id, status](GatewayObserver* obs) { obs->on_presence_update(user_id, status); });
  }

  Client& client_;
};

// ============================================================
// Client construction
// ============================================================

Client::Client(ConfigManager& config, const std::string& keychain_service,
               const std::string& db_path_override) : config_(config) {
  auto api_base = config.get_or<std::string>("network.api_base_url", std::string(endpoints::api_base));
  auto max_messages = static_cast<std::size_t>(config.get_or<int64_t>("behavior.max_messages_per_channel", 500));
  auto reconnect_base = config.get_or<int64_t>("behavior.reconnect_base_delay_ms", 1000);
  auto reconnect_max = config.get_or<int64_t>("behavior.reconnect_max_delay_ms", 30000);
  auto reconnect_retries = config.get_or<int64_t>("behavior.reconnect_max_retries", 10);

  keychain_service_ = keychain_service;
  db_path_override_ = db_path_override;
  token_store_ = std::make_unique<KeychainTokenStore>(keychain_service);

  auto qt_rest = std::make_unique<QtRestClient>();
  qt_rest->set_base_url(api_base);
  rest_ = std::move(qt_rest);

  GatewayConfig gw_config;
  gw_config.base_reconnect_delay_ms = static_cast<int>(reconnect_base);
  gw_config.max_reconnect_delay_ms = static_cast<int>(reconnect_max);
  gw_config.max_retries = static_cast<int>(reconnect_retries);
  gateway_ = std::make_unique<QtGatewayClient>(nullptr, gw_config);

  // clang-format off
  constexpr uint32_t default_intents =
      (1 << 0)  |  // GUILDS
      (1 << 9)  |  // GUILD_MESSAGES
      (1 << 12) |  // DIRECT_MESSAGES
      (1 << 15);   // MESSAGE_CONTENT
  // clang-format on
  gateway_->set_intents(default_intents);

  store_ = std::make_unique<DataStore>(max_messages);
  image_cache_ = std::make_unique<ImageCache>(platform_paths().cache_dir / "images");
  read_state_manager_ = std::make_unique<ReadStateManager>();
  mute_state_manager_ = std::make_unique<MuteStateManager>();

  // DB is NOT created here — it's deferred to init_account_db() after login
  // so the path can be scoped by user ID.

  auth_ = std::make_unique<AuthManager>(*rest_, *token_store_);

  wire_bridges();
}

Client::Client(ConfigManager& config, ClientDeps deps)
    : config_(config),
      test_mode_(true),
      token_store_(std::move(deps.token_store)),
      rest_(std::move(deps.rest)),
      gateway_(std::move(deps.gateway)),
      auth_(std::move(deps.auth)),
      store_(std::move(deps.store)),
      read_state_manager_(std::make_unique<ReadStateManager>()),
      mute_state_manager_(std::make_unique<MuteStateManager>()) {
  wire_bridges();
}

Client::~Client() {
  // Disconnect auth bridge before destroying components
  if (auth_ && auth_bridge_) {
    auth_->remove_observer(auth_bridge_.get());
  }
}

void Client::wire_bridges() {
  auth_bridge_ = std::make_unique<AuthBridge>(*this);
  gateway_bridge_ = std::make_unique<GatewayBridge>(*this);

  auth_->add_observer(auth_bridge_.get());
  gateway_->set_event_callback([this](std::string_view event_name, const std::string& data_json) {
    gateway_bridge_->on_event(event_name, data_json);
  });
}

// ============================================================
// Observer registration
// ============================================================

void Client::add_auth_observer(AuthObserver* obs) {
  auth_observers_.add(obs);
}
void Client::add_gateway_observer(GatewayObserver* obs) {
  gateway_observers_.add(obs);
}
void Client::add_store_observer(StoreObserver* obs) {
  store_->add_observer(obs);
}
void Client::remove_auth_observer(AuthObserver* obs) {
  auth_observers_.remove(obs);
}
void Client::remove_gateway_observer(GatewayObserver* obs) {
  gateway_observers_.remove(obs);
}
void Client::remove_store_observer(StoreObserver* obs) {
  store_->remove_observer(obs);
}

// ============================================================
// Actions
// ============================================================

bool Client::try_saved_login() {
  return try_saved_login(token_store_->load_token());
}

bool Client::try_saved_login(std::optional<TokenStore::StoredToken> saved) {
  if (!saved) {
    return false;
  }
  log::client()->debug("auto_login: attempting with stored token");
  auth_->login_with_token(saved->token, saved->token_type);
  return true;
}

std::optional<TokenStore::StoredToken> Client::saved_token() const {
  return token_store_->load_token();
}

void Client::login_with_token(std::string_view token, std::string_view token_type) {
  auth_->login_with_token(token, token_type);
}

void Client::login_with_credentials(std::string_view email, std::string_view password) {
  auth_->login_with_credentials(email, password);
}

void Client::submit_mfa_code(std::string_view code) {
  auth_->submit_mfa_code(code);
}

void Client::toggle_reaction(Snowflake channel_id, Snowflake message_id,
                             const std::string& emoji, bool add) {
  auto path = endpoints::reaction_url(channel_id, message_id, emoji);
  if (add) {
    rest_->put(path, "", [channel_id, message_id](RestClient::Response response) {
      if (!response) {
        log::client()->warn("Failed to add reaction on message {} in channel {}: {}", message_id, channel_id,
                            response.error().message);
      }
    });
  } else {
    rest_->del(path, [channel_id, message_id](RestClient::Response response) {
      if (!response) {
        log::client()->warn("Failed to remove reaction on message {} in channel {}: {}", message_id, channel_id,
                            response.error().message);
      }
    });
  }
}

void Client::ack_message(Snowflake channel_id, Snowflake message_id) {
  log::client()->debug("ack_message: channel={}, message={}", channel_id, message_id);
  auto path = endpoints::channel_messages(channel_id) + "/" + std::to_string(message_id) + "/ack";
  rest_->post(path, R"({"token":null})", [this, channel_id, message_id](RestClient::Response response) {
    if (!response) {
      log::client()->debug("ack_message: failed for channel={}, message={}: {}", channel_id, message_id, response.error().message);
      return;
    }
    log::client()->debug("ack_message: success for channel={}", channel_id);
    read_state_manager_->mark_read(channel_id, message_id);
    if (db_writer_) {
      auto rs = read_state_manager_->state(channel_id);
      emit db_writer_->read_state_write_requested(
          channel_id, message_id, 0, 0, rs.last_message_id);
    }
  });
}

void Client::send_message(Snowflake channel_id, std::string_view content) {
  QJsonObject body;
  body["content"] = QString::fromUtf8(content.data(), static_cast<int>(content.size()));
  std::string payload = QJsonDocument(body).toJson(QJsonDocument::Compact).toStdString();

  rest_->post(endpoints::channel_messages(channel_id), payload, [channel_id](RestClient::Response response) {
    if (!response) {
      log::client()->warn("Failed to send message to channel {}: {}", channel_id, response.error().message);
    }
  });
}

void Client::fetch_single_message(Snowflake channel_id, Snowflake message_id) {
  // First check the local store
  auto all_msgs = store_->messages(channel_id);
  for (auto& local : all_msgs) {
    if (local.id == message_id) {
      // Found locally, update referencing messages
      for (auto& msg : all_msgs) {
        if (msg.referenced_message_id == message_id
            && !msg.referenced_message_author.has_value()) {
          msg.referenced_message_author = local.author.username;
          msg.referenced_message_content = local.content;
          store_->update_message(msg);
          gateway_observers_.notify([&msg](GatewayObserver* obs) {
            obs->on_message_update(msg);
          });
        }
      }
      return;
    }
  }

  // Not in local store. Use GET /channels/{id}/messages?around={id}&limit=1
  // which works with user tokens (unlike the singular /messages/{id} endpoint).
  auto path = endpoints::channel_messages(channel_id)
              + "?around=" + std::to_string(message_id) + "&limit=1";
  rest_->get(path, [this, channel_id, message_id](RestClient::Response response) {
    if (!response) {
      log::client()->debug("Failed to fetch message around {} in channel {}: {}",
                           message_id, channel_id, response.error().message);
      return;
    }
    auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(response.value()));
    if (!doc.isArray()) {
      return;
    }
    // Find the target message in the returned array
    for (const auto& val : doc.array()) {
      auto parsed = json_parse::parse_message(
          QJsonDocument(val.toObject()).toJson(QJsonDocument::Compact).toStdString());
      if (!parsed || parsed->id != message_id) {
        continue;
      }
      // Update referencing messages in the store
      auto msgs = store_->messages(channel_id);
      for (auto& msg : msgs) {
        if (msg.referenced_message_id == message_id
            && !msg.referenced_message_author.has_value()) {
          msg.referenced_message_author = parsed->author.username;
          msg.referenced_message_content = parsed->content;
          store_->update_message(msg);
          gateway_observers_.notify([&msg](GatewayObserver* obs) {
            obs->on_message_update(msg);
          });
        }
      }
      return;
    }
  });
}

void Client::select_guild(Snowflake guild_id) {
  active_guild_id_.store(guild_id);

  rest_->get(endpoints::guild_channels(guild_id), [this, guild_id](RestClient::Response response) {
    // Discard if user has already switched to a different guild
    if (active_guild_id_.load() != guild_id) {
      return;
    }

    if (!response) {
      log::client()->warn("Failed to fetch channels for guild {}: {}", guild_id, response.error().message);
      return;
    }

    auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(response.value()));
    if (doc.isNull()) {
      log::client()->warn("Failed to parse guild channels JSON: document is null");
      return;
    }
    if (!doc.isArray()) {
      log::client()->warn("Failed to parse guild channels JSON: expected array");
      return;
    }

    auto arr = doc.array();
    std::vector<Channel> channels;
    channels.reserve(arr.size());
    for (const auto& val : arr) {
      auto channel = json_parse::parse_channel(val.toObject());
      if (channel) {
        channel->guild_id = guild_id;
        if (db_writer_) {
          emit db_writer_->channel_write_requested(*channel);
          if (!channel->permission_overwrites.empty()) {
            emit db_writer_->overwrites_write_requested(channel->id, channel->permission_overwrites);
          }
        }
        channels.push_back(std::move(*channel));
      }
    }
    store_->bulk_upsert_channels(guild_id, std::move(channels));
  });
}

void Client::select_channel(Snowflake channel_id) {
  active_channel_id_.store(channel_id);

  std::string path = endpoints::channel_messages(channel_id);
  rest_->get(path, [this, channel_id](RestClient::Response response) {
    // Discard if user has already switched to a different channel
    if (active_channel_id_.load() != channel_id) {
      return;
    }

    if (!response) {
      log::client()->warn("Failed to fetch messages for channel {}: {}", channel_id, response.error().message);
      return;
    }

    auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(response.value()));
    if (doc.isNull() || !doc.isArray()) {
      log::client()->warn("Failed to parse message history JSON");
      return;
    }

    auto arr = doc.array();
    std::vector<Message> messages;
    messages.reserve(arr.size());
    for (const auto& val : arr) {
      auto msg = json_parse::parse_message(val.toObject());
      if (msg) {
        msg->channel_id = channel_id;
        messages.push_back(std::move(*msg));
      }
    }
    if (db_writer_) {
      for (const auto& msg : messages) {
        emit db_writer_->message_write_requested(msg);
      }
    }
    store_->set_messages(channel_id, std::move(messages));
  });
}

void Client::fetch_message_history(Snowflake channel_id, std::optional<Snowflake> before) {
  // Serve from database immediately for instant display
  if (db_reader_) {
    auto db_msgs = db_reader_->messages(channel_id, before, 50);
    if (!db_msgs.empty()) {
      store_->add_messages_before(channel_id, std::move(db_msgs));
    }
  }

  // Always validate against REST in the background
  std::string path = endpoints::channel_messages(channel_id);
  if (before) {
    path += "?before=" + std::to_string(*before);
  }

  rest_->get(path, [this, channel_id](RestClient::Response response) {
    if (!response) {
      log::client()->warn("Failed to fetch messages for channel {}: {}", channel_id, response.error().message);
      return;
    }

    auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(response.value()));
    if (doc.isNull() || !doc.isArray()) {
      log::client()->warn("Failed to parse message history JSON");
      return;
    }

    auto arr = doc.array();
    std::vector<Message> messages;
    messages.reserve(arr.size());
    for (const auto& val : arr) {
      auto msg = json_parse::parse_message(val.toObject());
      if (msg) {
        msg->channel_id = channel_id;
        messages.push_back(std::move(*msg));
      }
    }
    if (db_writer_) {
      for (const auto& msg : messages) {
        emit db_writer_->message_write_requested(msg);
      }
    }
    store_->add_messages_before(channel_id, std::move(messages));
  });
}

void Client::logout() {
  auth_->logout();
}

bool Client::try_load_last_account() {
  if (!db_path_override_.empty()) {
    // Test mode: use the override path directly
    init_account_db(0);
    return db_reader_ != nullptr;
  }

  auto last_account_path = platform_paths().state_dir / "last_account";
  std::ifstream in(last_account_path);
  if (!in) {
    return false;
  }

  std::string id_str;
  if (!std::getline(in, id_str) || id_str.empty()) {
    return false;
  }

  Snowflake user_id = safe_stoull(id_str);
  if (user_id == 0) {
    return false;
  }

  init_account_db(user_id);
  return db_reader_ != nullptr;
}

void Client::init_account_db(Snowflake user_id) {
  if (db_manager_) {
    return; // Already initialized
  }

  std::filesystem::path db_path;
  if (!db_path_override_.empty()) {
    db_path = std::filesystem::path(db_path_override_);
  } else {
    db_path = platform_paths().state_dir / "accounts" / std::to_string(user_id) / "kind.db";
  }

  db_manager_ = std::make_unique<DatabaseManager>(db_path);
  db_manager_->initialize();
  db_writer_ = std::make_unique<DatabaseWriter>(db_path.string());
  db_reader_ = std::make_unique<DatabaseReader>(db_path.string());

  // Save this as the last active account (skip for test overrides)
  if (db_path_override_.empty()) {
    auto global_state_dir = platform_paths().state_dir;
    std::filesystem::create_directories(global_state_dir);
    std::ofstream last_account(global_state_dir / "last_account");
    if (last_account) {
      last_account << user_id << '\n';
    }
  }

  // Persist read state changes (unread increments) to the database
  QObject::connect(read_state_manager_.get(), &ReadStateManager::persist_requested,
                   read_state_manager_.get(), [this](Snowflake channel_id, const ReadState& state) {
    if (db_writer_) {
      emit db_writer_->read_state_write_requested(
          channel_id, state.last_read_id, state.mention_count,
          state.unread_count, state.last_message_id);
    }
  });

  log::cache()->info("Account database initialized for user {} at {}", user_id, db_path.string());
}

void Client::load_cache(std::function<void()> on_complete) {
  if (!db_reader_) {
    if (on_complete) on_complete();
    return;
  }

  log::client()->debug("load_cache: starting async DB read");

  // All DB reads happen on a worker thread. Results are delivered back
  // to the main thread via the ReadStateManager (a QObject) as context.
  auto* reader = db_reader_.get();
  auto* store = store_.get();
  auto* rsm = read_state_manager_.get();
  auto* msm = mute_state_manager_.get();

  struct CacheData {
    std::optional<User> user;
    std::vector<Guild> guilds;
    std::vector<Snowflake> guild_order;
    std::unordered_map<Snowflake, std::vector<Channel>> guild_channels;
    std::unordered_map<Snowflake, std::vector<Snowflake>> guild_member_roles;
    std::vector<Channel> dm_channels;
    std::vector<std::pair<Snowflake, ReadState>> read_states;
    std::vector<std::tuple<Snowflake, int, bool>> mute_states;
  };

  auto db_path = db_reader_->db_path();
  auto future = QtConcurrent::run([db_path]() -> std::shared_ptr<CacheData> {
    // Create a thread-local DB reader for this worker thread
    DatabaseReader reader(db_path);
    auto data = std::make_shared<CacheData>();

    data->user = reader.current_user();
    data->guilds = reader.guilds();
    for (auto& guild : data->guilds) {
      guild.roles = reader.roles(guild.id);
    }
    data->guild_order = reader.guild_order();

    for (const auto& guild : data->guilds) {
      auto channels = reader.channels(guild.id);
      for (auto& ch : channels) {
        ch.permission_overwrites = reader.permission_overwrites(ch.id);
      }
      data->guild_channels[guild.id] = std::move(channels);

      auto role_ids = reader.member_roles(guild.id);
      if (!role_ids.empty()) {
        data->guild_member_roles[guild.id] = std::move(role_ids);
      }
    }

    data->dm_channels = reader.dm_channels();
    data->read_states = reader.read_states();
    data->mute_states = reader.mute_states();
    log::client()->debug("load_cache worker: read {} guilds, {} channels across {} guilds, {} dm_channels, {} read_states, {} mute_states",
                         data->guilds.size(), data->guild_channels.size(),
                         data->guild_channels.size(), data->dm_channels.size(),
                         data->read_states.size(), data->mute_states.size());
    return data;
  });

  auto* watcher = new QFutureWatcher<std::shared_ptr<CacheData>>(rsm);
  QObject::connect(watcher, &QFutureWatcher<std::shared_ptr<CacheData>>::finished,
                   rsm, [watcher, store, rsm, msm, on_complete = std::move(on_complete)]() {
    watcher->deleteLater();
    auto data = watcher->result();

    if (data->user) {
      store->set_current_user(*data->user);
      log::client()->debug("load_cache: set current user: {}", data->user->username);
    }

    // Load read states and mute states BEFORE upserting guilds so that
    // when store observers fire and the guild model recomputes caches,
    // the unread and mute data is already available.
    if (!data->read_states.empty()) {
      rsm->load_read_states(data->read_states);
    }
    log::client()->debug("load_cache: loaded {} read states", data->read_states.size());
    if (!data->mute_states.empty()) {
      msm->load_from_db(data->mute_states);
    }
    log::client()->debug("load_cache: loaded {} mute states", data->mute_states.size());

    // Suppress store observers during bulk cache load. The on_complete
    // callback handles GUI updates once all data is in place. Without
    // suppression, observer signals fire before channels are loaded,
    // producing guild snapshots with empty channel lists.
    store->set_suppress_observers(true);
    auto guilds_count = data->guilds.size();
    store->bulk_upsert_guilds(std::move(data->guilds));
    log::client()->debug("load_cache: upserted {} guilds", guilds_count);
    if (!data->guild_order.empty()) {
      store->set_guild_order(data->guild_order);
    }
    log::client()->debug("load_cache: set guild order ({} guilds)", data->guild_order.size());
    for (auto& [guild_id, channels] : data->guild_channels) {
      store->bulk_upsert_channels(guild_id, std::move(channels));
    }
    log::client()->debug("load_cache: upserted channels for {} guilds", data->guild_channels.size());
    for (const auto& [guild_id, role_ids] : data->guild_member_roles) {
      store->set_member_roles(guild_id, role_ids);
    }
    log::client()->debug("load_cache: set member roles for {} guilds", data->guild_member_roles.size());
    if (!data->dm_channels.empty()) {
      store->bulk_upsert_private_channels(std::move(data->dm_channels));
    }
    log::client()->debug("load_cache: loaded {} DM channels", store->private_channels().size());
    store->set_suppress_observers(false);

    log::cache()->info("Loaded cache from database");

    log::client()->debug("load_cache: complete, calling on_complete");
    if (on_complete) {
      on_complete();
    }
  });
  watcher->setFuture(future);
}

void Client::save_cache() {
  if (db_writer_) {
    db_writer_->flush_sync();
  }
}

void Client::save_last_selection(Snowflake guild_id, Snowflake channel_id) {
  if (db_writer_) {
    emit db_writer_->app_state_write_requested("last_guild_id", QString::number(guild_id));
    emit db_writer_->app_state_write_requested("last_channel_id", QString::number(channel_id));
  }
}

void Client::save_guild_channel(Snowflake guild_id, Snowflake channel_id) {
  if (db_writer_) {
    auto key = "guild_channel_" + std::to_string(guild_id);
    emit db_writer_->app_state_write_requested(QString::fromStdString(key), QString::number(channel_id));
  }
}

Snowflake Client::last_channel_for_guild(Snowflake guild_id) const {
  if (db_reader_) {
    auto key = "guild_channel_" + std::to_string(guild_id);
    auto val = db_reader_->app_state(key);
    if (val) {
      return safe_stoull(*val);
    }
  }
  return 0;
}

Client::LastSelection Client::last_selection() const {
  LastSelection sel;
  if (db_reader_) {
    auto guild = db_reader_->app_state("last_guild_id");
    if (guild) {
      sel.guild_id = safe_stoull(*guild);
    }
    auto channel = db_reader_->app_state("last_channel_id");
    if (channel) {
      sel.channel_id = safe_stoull(*channel);
    }
  }
  return sel;
}

bool Client::is_connected() const {
  return gateway_ && gateway_->is_connected();
}

int Client::latency_ms() const {
  return gateway_ ? gateway_->latency_ms() : -1;
}

// ============================================================
// State accessors
// ============================================================

std::vector<Guild> Client::guilds() const {
  return store_->guilds();
}
std::optional<Guild> Client::guild(Snowflake guild_id) const {
  return store_->guild(guild_id);
}
std::vector<Channel> Client::channels(Snowflake guild_id) const {
  return store_->channels(guild_id);
}
std::vector<Message> Client::messages(Snowflake channel_id) const {
  return store_->messages(channel_id);
}
std::vector<Message> Client::messages(Snowflake channel_id,
                                       std::optional<Snowflake> before,
                                       int limit) const {
  auto cached = store_->messages(channel_id, before, limit);
  if (!cached.empty()) {
    return cached;
  }
  if (db_reader_) {
    return db_reader_->messages(channel_id, before, limit);
  }
  return {};
}
std::optional<User> Client::current_user() const {
  return store_->current_user();
}
std::vector<Channel> Client::private_channels() const {
  return store_->private_channels();
}
std::vector<Snowflake> Client::member_roles(Snowflake guild_id) const {
  return store_->member_roles(guild_id);
}

} // namespace kind
