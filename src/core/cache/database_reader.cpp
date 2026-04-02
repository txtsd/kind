#include "cache/database_reader.hpp"
#include "read_state_manager.hpp"

#include "logging.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include <algorithm>
#include <functional>

namespace kind {

DatabaseReader::DatabaseReader(const std::string& db_path) : db_path_(db_path) {
  connection_name_ = QString("reader_%1").arg(reinterpret_cast<quintptr>(this));
  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connection_name_);
  db.setDatabaseName(QString::fromStdString(db_path_));
  if (!db.open()) {
    log::cache()->error("Reader: failed to open database: {}",
                        db.lastError().text().toStdString());
  }
}

DatabaseReader::~DatabaseReader() {
  {
    QSqlDatabase db = QSqlDatabase::database(connection_name_);
    if (db.isOpen()) {
      db.close();
    }
  }
  QSqlDatabase::removeDatabase(connection_name_);
}

std::vector<Guild> DatabaseReader::guilds() const {
  std::vector<Guild> result;
  QSqlDatabase db = QSqlDatabase::database(connection_name_);
  QSqlQuery q(db);
  q.exec("SELECT id, name, icon, owner_id FROM guilds");
  while (q.next()) {
    Guild guild;
    guild.id = static_cast<Snowflake>(q.value(0).toLongLong());
    guild.name = q.value(1).toString().toStdString();
    guild.icon_hash = q.value(2).toString().toStdString();
    guild.owner_id = static_cast<Snowflake>(q.value(3).toLongLong());
    result.push_back(std::move(guild));
  }
  log::cache()->debug("DB read: {} guilds", result.size());
  return result;
}

std::vector<Snowflake> DatabaseReader::guild_order() const {
  std::vector<Snowflake> result;
  QSqlDatabase db = QSqlDatabase::database(connection_name_);
  QSqlQuery q(db);
  q.exec("SELECT guild_id FROM guild_order ORDER BY position ASC");
  while (q.next()) {
    result.push_back(static_cast<Snowflake>(q.value(0).toLongLong()));
  }
  log::cache()->debug("DB read: guild order ({} entries)", result.size());
  return result;
}

std::vector<Channel> DatabaseReader::channels(Snowflake guild_id) const {
  std::vector<Channel> result;
  QSqlDatabase db = QSqlDatabase::database(connection_name_);
  QSqlQuery q(db);
  q.prepare("SELECT id, guild_id, type, name, position, parent_id, last_message_id "
            "FROM channels WHERE guild_id = :gid");
  q.bindValue(":gid", static_cast<qint64>(guild_id));
  q.exec();
  while (q.next()) {
    Channel ch;
    ch.id = static_cast<Snowflake>(q.value(0).toLongLong());
    ch.guild_id = static_cast<Snowflake>(q.value(1).toLongLong());
    ch.type = q.value(2).toInt();
    ch.name = q.value(3).toString().toStdString();
    ch.position = q.value(4).toInt();
    if (!q.value(5).isNull()) {
      ch.parent_id = static_cast<Snowflake>(q.value(5).toLongLong());
    }
    ch.last_message_id = static_cast<Snowflake>(q.value(6).toLongLong());
    result.push_back(std::move(ch));
  }
  log::cache()->debug("DB read: {} channels for guild {}", result.size(), guild_id);
  return result;
}

std::vector<Role> DatabaseReader::roles(Snowflake guild_id) const {
  std::vector<Role> result;
  QSqlDatabase db = QSqlDatabase::database(connection_name_);
  QSqlQuery q(db);
  q.prepare("SELECT id, name, permissions, position FROM roles WHERE guild_id = :gid");
  q.bindValue(":gid", static_cast<qint64>(guild_id));
  q.exec();
  while (q.next()) {
    Role role;
    role.id = static_cast<Snowflake>(q.value(0).toLongLong());
    role.name = q.value(1).toString().toStdString();
    role.permissions = static_cast<uint64_t>(q.value(2).toLongLong());
    role.position = q.value(3).toInt();
    result.push_back(std::move(role));
  }
  log::cache()->debug("DB read: {} roles for guild {}", result.size(), guild_id);
  return result;
}

std::vector<PermissionOverwrite> DatabaseReader::permission_overwrites(
    Snowflake channel_id) const {
  std::vector<PermissionOverwrite> result;
  QSqlDatabase db = QSqlDatabase::database(connection_name_);
  QSqlQuery q(db);
  q.prepare("SELECT target_id, type, allow, deny "
            "FROM permission_overwrites WHERE channel_id = :cid");
  q.bindValue(":cid", static_cast<qint64>(channel_id));
  q.exec();
  while (q.next()) {
    PermissionOverwrite ow;
    ow.id = static_cast<Snowflake>(q.value(0).toLongLong());
    ow.type = q.value(1).toInt();
    ow.allow = static_cast<uint64_t>(q.value(2).toLongLong());
    ow.deny = static_cast<uint64_t>(q.value(3).toLongLong());
    result.push_back(ow);
  }
  log::cache()->debug("DB read: {} overwrites for channel {}", result.size(), channel_id);
  return result;
}

std::vector<Snowflake> DatabaseReader::member_roles(Snowflake guild_id) const {
  std::vector<Snowflake> result;
  QSqlDatabase db = QSqlDatabase::database(connection_name_);
  QSqlQuery q(db);
  q.prepare("SELECT roles FROM members WHERE guild_id = :gid AND user_id = 0");
  q.bindValue(":gid", static_cast<qint64>(guild_id));
  q.exec();
  if (q.next()) {
    auto json_str = q.value(0).toString();
    auto doc = QJsonDocument::fromJson(json_str.toUtf8());
    if (doc.isArray()) {
      for (const auto& val : doc.array()) {
        result.push_back(static_cast<Snowflake>(val.toString().toULongLong()));
      }
    }
  }
  log::cache()->debug("DB read: {} member roles for guild {}", result.size(), guild_id);
  return result;
}

std::vector<Channel> DatabaseReader::dm_channels() const {
  std::vector<Channel> result;
  QSqlDatabase db = QSqlDatabase::database(connection_name_);
  QSqlQuery q(db);
  q.exec("SELECT id, type, name, last_message_id FROM channels WHERE guild_id = 0 AND type = 1");
  while (q.next()) {
    Channel ch;
    ch.id = static_cast<Snowflake>(q.value(0).toLongLong());
    ch.type = q.value(1).toInt();
    ch.name = q.value(2).toString().toStdString();
    ch.last_message_id = static_cast<Snowflake>(q.value(3).toLongLong());
    ch.guild_id = 0;

    // Load recipients
    QSqlQuery rq(db);
    rq.prepare("SELECT user_id, username, avatar FROM dm_recipients WHERE channel_id = :cid");
    rq.bindValue(":cid", static_cast<qint64>(ch.id));
    rq.exec();
    while (rq.next()) {
      User user;
      user.id = static_cast<Snowflake>(rq.value(0).toLongLong());
      user.username = rq.value(1).toString().toStdString();
      user.avatar_hash = rq.value(2).toString().toStdString();
      ch.recipients.push_back(std::move(user));
    }
    result.push_back(std::move(ch));
  }
  log::cache()->debug("DB read: {} dm channels", result.size());
  return result;
}

std::optional<User> DatabaseReader::current_user() const {
  QSqlDatabase db = QSqlDatabase::database(connection_name_);

  QSqlQuery sq(db);
  sq.prepare("SELECT value FROM app_state WHERE key = 'current_user_id'");
  sq.exec();
  if (!sq.next()) {
    log::cache()->debug("DB read: no current user");
    return std::nullopt;
  }

  auto user_id = static_cast<Snowflake>(sq.value(0).toString().toULongLong());

  QSqlQuery q(db);
  q.prepare("SELECT id, username, discriminator, avatar, bot FROM users WHERE id = :id");
  q.bindValue(":id", static_cast<qint64>(user_id));
  q.exec();
  if (!q.next()) {
    log::cache()->debug("DB read: no current user");
    return std::nullopt;
  }

  User user;
  user.id = static_cast<Snowflake>(q.value(0).toLongLong());
  user.username = q.value(1).toString().toStdString();
  user.discriminator = q.value(2).toString().toStdString();
  user.avatar_hash = q.value(3).toString().toStdString();
  user.bot = q.value(4).toBool();
  log::cache()->debug("DB read: current user {}", user.username);
  return user;
}

std::vector<Message> DatabaseReader::messages(Snowflake channel_id,
                                              std::optional<Snowflake> before,
                                              int limit) const {
  std::vector<Message> result;
  QSqlDatabase db = QSqlDatabase::database(connection_name_);
  QSqlQuery q(db);

  if (before) {
    q.prepare(
        "SELECT m.id, m.channel_id, m.author_id, m.content, m.timestamp, "
        "       m.edited_at, m.pinned, m.deleted, "
        "       u.username, u.discriminator, u.avatar, u.bot, "
        "       m.type, m.ref_msg_id, m.data "
        "FROM messages m "
        "LEFT JOIN users u ON m.author_id = u.id "
        "WHERE m.channel_id = :cid AND m.id < :before "
        "ORDER BY m.id DESC LIMIT :limit");
    q.bindValue(":before", static_cast<qint64>(*before));
  } else {
    q.prepare(
        "SELECT m.id, m.channel_id, m.author_id, m.content, m.timestamp, "
        "       m.edited_at, m.pinned, m.deleted, "
        "       u.username, u.discriminator, u.avatar, u.bot, "
        "       m.type, m.ref_msg_id, m.data "
        "FROM messages m "
        "LEFT JOIN users u ON m.author_id = u.id "
        "WHERE m.channel_id = :cid "
        "ORDER BY m.id DESC LIMIT :limit");
  }
  q.bindValue(":cid", static_cast<qint64>(channel_id));
  q.bindValue(":limit", limit);
  q.exec();

  while (q.next()) {
    Message msg;
    msg.id = static_cast<Snowflake>(q.value(0).toLongLong());
    msg.channel_id = static_cast<Snowflake>(q.value(1).toLongLong());
    msg.author.id = static_cast<Snowflake>(q.value(2).toLongLong());
    msg.content = q.value(3).toString().toStdString();
    msg.timestamp = q.value(4).toString().toStdString();
    if (!q.value(5).isNull()) {
      msg.edited_timestamp = q.value(5).toString().toStdString();
    }
    msg.pinned = q.value(6).toBool();
    msg.deleted = q.value(7).toBool();
    msg.author.username = q.value(8).toString().toStdString();
    msg.author.discriminator = q.value(9).toString().toStdString();
    msg.author.avatar_hash = q.value(10).toString().toStdString();
    msg.author.bot = q.value(11).toBool();
    msg.type = q.value(12).toInt();
    if (!q.value(13).isNull()) {
      msg.referenced_message_id = static_cast<Snowflake>(q.value(13).toLongLong());
    }

    auto data_str = q.value(14).toString();
    if (!data_str.isEmpty()) {
      auto data_doc = QJsonDocument::fromJson(data_str.toUtf8());
      if (data_doc.isObject()) {
        auto data = data_doc.object();

        msg.mention_everyone = data["mention_everyone"].toBool(false);
        for (const auto& val : data["mentions"].toArray()) {
          auto mobj = val.toObject();
          Mention mention;
          mention.id = static_cast<Snowflake>(mobj["id"].toString().toULongLong());
          mention.username = mobj["username"].toString().toStdString();
          msg.mentions.push_back(std::move(mention));
        }
        for (const auto& val : data["mention_roles"].toArray()) {
          msg.mention_roles.push_back(static_cast<Snowflake>(val.toString().toULongLong()));
        }

        for (const auto& val : data["reactions"].toArray()) {
          auto robj = val.toObject();
          Reaction reaction;
          reaction.emoji_name = robj["emoji_name"].toString().toStdString();
          if (robj.contains("emoji_id")) {
            reaction.emoji_id = static_cast<Snowflake>(robj["emoji_id"].toString().toULongLong());
          }
          reaction.count = robj["count"].toInt();
          reaction.me = robj["me"].toBool();
          msg.reactions.push_back(std::move(reaction));
        }

        for (const auto& val : data["sticker_items"].toArray()) {
          auto sobj = val.toObject();
          StickerItem sticker;
          sticker.id = static_cast<Snowflake>(sobj["id"].toString().toULongLong());
          sticker.name = sobj["name"].toString().toStdString();
          sticker.format_type = sobj["format_type"].toInt(1);
          msg.sticker_items.push_back(std::move(sticker));
        }

        std::function<Component(const QJsonObject&)> parse_comp;
        parse_comp = [&](const QJsonObject& cobj) -> Component {
          Component comp;
          comp.type = cobj["type"].toInt();
          if (cobj.contains("custom_id")) comp.custom_id = cobj["custom_id"].toString().toStdString();
          if (cobj.contains("label")) comp.label = cobj["label"].toString().toStdString();
          comp.style = cobj["style"].toInt();
          comp.disabled = cobj["disabled"].toBool();
          for (const auto& child : cobj["components"].toArray()) {
            comp.children.push_back(parse_comp(child.toObject()));
          }
          return comp;
        };
        for (const auto& val : data["components"].toArray()) {
          msg.components.push_back(parse_comp(val.toObject()));
        }

        if (data.contains("embeds")) {
          for (const auto& val : data["embeds"].toArray()) {
            auto eobj = val.toObject();
            Embed embed;
            if (eobj.contains("title")) embed.title = eobj["title"].toString().toStdString();
            if (eobj.contains("description"))
              embed.description = eobj["description"].toString().toStdString();
            if (eobj.contains("url")) embed.url = eobj["url"].toString().toStdString();
            if (eobj.contains("color")) embed.color = eobj["color"].toInt();
            if (eobj.contains("author") && eobj["author"].isObject()) {
              auto aobj = eobj["author"].toObject();
              EmbedAuthor author;
              author.name = aobj["name"].toString().toStdString();
              if (aobj.contains("url")) author.url = aobj["url"].toString().toStdString();
              embed.author = std::move(author);
            }
            if (eobj.contains("footer") && eobj["footer"].isObject()) {
              EmbedFooter footer;
              footer.text = eobj["footer"].toObject()["text"].toString().toStdString();
              embed.footer = std::move(footer);
            }
            if (eobj.contains("image") && eobj["image"].isObject()) {
              auto iobj = eobj["image"].toObject();
              EmbedImage image;
              image.url = iobj["url"].toString().toStdString();
              if (iobj.contains("width")) image.width = iobj["width"].toInt();
              if (iobj.contains("height")) image.height = iobj["height"].toInt();
              embed.image = std::move(image);
            }
            if (eobj.contains("thumbnail") && eobj["thumbnail"].isObject()) {
              auto tobj = eobj["thumbnail"].toObject();
              EmbedImage thumb;
              thumb.url = tobj["url"].toString().toStdString();
              if (tobj.contains("width")) thumb.width = tobj["width"].toInt();
              if (tobj.contains("height")) thumb.height = tobj["height"].toInt();
              embed.thumbnail = std::move(thumb);
            }
            for (const auto& fval : eobj["fields"].toArray()) {
              auto fobj = fval.toObject();
              EmbedField field;
              field.name = fobj["name"].toString().toStdString();
              field.value = fobj["value"].toString().toStdString();
              field.inline_field = fobj["inline"].toBool(false);
              embed.fields.push_back(std::move(field));
            }
            msg.embeds.push_back(std::move(embed));
          }
        }

        if (data.contains("attachments")) {
          for (const auto& val : data["attachments"].toArray()) {
            auto aobj = val.toObject();
            Attachment att;
            att.id = static_cast<Snowflake>(aobj["id"].toString().toULongLong());
            att.filename = aobj["filename"].toString().toStdString();
            att.url = aobj["url"].toString().toStdString();
            att.size = static_cast<std::size_t>(aobj["size"].toInteger(0));
            if (aobj.contains("width")) att.width = aobj["width"].toInt();
            if (aobj.contains("height")) att.height = aobj["height"].toInt();
            msg.attachments.push_back(std::move(att));
          }
        }
      }
    }

    result.push_back(std::move(msg));
  }

  std::ranges::reverse(result);
  log::cache()->debug("DB read: {} messages for channel {} (before={}, limit={})",
                      result.size(), channel_id,
                      before ? std::to_string(*before) : "none", limit);
  return result;
}

std::vector<std::pair<Snowflake, ReadState>> DatabaseReader::read_states() const {
  std::vector<std::pair<Snowflake, ReadState>> result;
  QSqlDatabase db = QSqlDatabase::database(connection_name_);
  QSqlQuery q(db);
  q.exec("SELECT channel_id, last_read_id, mention_count, unread_count, last_message_id "
         "FROM read_state");
  while (q.next()) {
    auto channel_id = static_cast<Snowflake>(q.value(0).toLongLong());
    ReadState rs;
    rs.last_read_id = static_cast<Snowflake>(q.value(1).toLongLong());
    rs.mention_count = q.value(2).toInt();
    rs.unread_count = q.value(3).toInt();
    rs.last_message_id = static_cast<Snowflake>(q.value(4).toLongLong());
    result.emplace_back(channel_id, rs);
  }
  log::cache()->debug("DB read: {} read states", result.size());
  return result;
}

std::vector<std::tuple<Snowflake, int, bool>> DatabaseReader::mute_states() const {
  std::vector<std::tuple<Snowflake, int, bool>> result;
  QSqlDatabase db = QSqlDatabase::database(connection_name_);
  QSqlQuery q(db);
  q.exec("SELECT id, type, muted FROM mute_state");
  while (q.next()) {
    auto id = static_cast<Snowflake>(q.value(0).toLongLong());
    int type = q.value(1).toInt();
    bool muted = q.value(2).toBool();
    result.emplace_back(id, type, muted);
  }
  log::cache()->debug("DB read: {} mute states", result.size());
  return result;
}

std::optional<std::string> DatabaseReader::app_state(const std::string& key) const {
  QSqlDatabase db = QSqlDatabase::database(connection_name_);
  QSqlQuery q(db);
  q.prepare("SELECT value FROM app_state WHERE key = :key");
  q.bindValue(":key", QString::fromStdString(key));
  q.exec();
  if (q.next()) {
    auto val = q.value(0).toString().toStdString();
    log::cache()->debug("DB read: app_state[{}] = {}", key, val);
    return val;
  }
  log::cache()->debug("DB read: app_state[{}] not found", key);
  return std::nullopt;
}

} // namespace kind
