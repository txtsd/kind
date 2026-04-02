#include "json/parsers.hpp"

#include <functional>
#include <QJsonArray>
#include <QJsonDocument>
#include "logging.hpp"

namespace kind::json_parse {

std::optional<User> parse_user(const QJsonObject& obj) {
  if (obj.isEmpty()) {
    log::client()->warn("Failed to parse User: empty JSON object");
    return std::nullopt;
  }

  User user;
  user.id = static_cast<Snowflake>(obj["id"].toString().toULongLong());
  user.username = obj["username"].toString().toStdString();
  user.discriminator = obj["discriminator"].toString().toStdString();
  user.avatar_hash = obj["avatar"].toString().toStdString();
  user.bot = obj["bot"].toBool(false);
  return user;
}

void merge_user(User& existing, const QJsonObject& obj) {
  if (obj.contains("id")) {
    existing.id = static_cast<Snowflake>(obj["id"].toString().toULongLong());
  }
  if (obj.contains("username")) {
    existing.username = obj["username"].toString().toStdString();
  }
  if (obj.contains("discriminator")) {
    existing.discriminator = obj["discriminator"].toString().toStdString();
  }
  if (obj.contains("avatar")) {
    existing.avatar_hash = obj["avatar"].toString().toStdString();
  }
  if (obj.contains("bot")) {
    existing.bot = obj["bot"].toBool(false);
  }
}

std::optional<User> parse_user(const std::string& json) {
  auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(json));
  if (doc.isNull()) {
    log::client()->warn("Failed to parse User JSON: document is null");
    return std::nullopt;
  }
  if (!doc.isObject()) {
    log::client()->warn("Failed to parse User JSON: expected object");
    return std::nullopt;
  }
  return parse_user(doc.object());
}

std::optional<Guild> parse_guild(const QJsonObject& obj) {
  if (obj.isEmpty()) {
    log::client()->warn("Failed to parse Guild: empty JSON object");
    return std::nullopt;
  }

  Guild guild;
  guild.id = static_cast<Snowflake>(obj["id"].toString().toULongLong());

  // User tokens nest guild metadata under "properties"; bot tokens use top-level fields
  if (obj.contains("properties") && obj["properties"].isObject()) {
    auto props = obj["properties"].toObject();
    guild.name = props["name"].toString().toStdString();
    guild.icon_hash = props["icon"].toString().toStdString();
    guild.owner_id = static_cast<Snowflake>(props["owner_id"].toString().toULongLong());
  } else {
    guild.name = obj["name"].toString().toStdString();
    guild.icon_hash = obj["icon"].toString().toStdString();
    guild.owner_id = static_cast<Snowflake>(obj["owner_id"].toString().toULongLong());
  }

  auto roles_array = obj["roles"].toArray();
  for (const auto& val : roles_array) {
    auto role = parse_role(val.toObject());
    if (role) {
      guild.roles.push_back(std::move(*role));
    }
  }

  auto channels_array = obj["channels"].toArray();
  for (const auto& val : channels_array) {
    auto channel = parse_channel(val.toObject());
    if (channel) {
      channel->guild_id = guild.id;
      guild.channels.push_back(std::move(*channel));
    }
  }
  return guild;
}

std::optional<Channel> parse_channel(const QJsonObject& obj) {
  if (obj.isEmpty()) {
    log::client()->warn("Failed to parse Channel: empty JSON object");
    return std::nullopt;
  }

  Channel channel;
  channel.id = static_cast<Snowflake>(obj["id"].toString().toULongLong());
  channel.guild_id = static_cast<Snowflake>(obj["guild_id"].toString().toULongLong());
  channel.name = obj["name"].toString().toStdString();
  channel.type = obj["type"].toInt();
  channel.position = obj["position"].toInt();
  if (obj.contains("parent_id") && !obj["parent_id"].isNull()) {
    channel.parent_id = static_cast<Snowflake>(obj["parent_id"].toString().toULongLong());
  }

  auto overwrites_array = obj["permission_overwrites"].toArray();
  for (const auto& val : overwrites_array) {
    auto ow = parse_overwrite(val.toObject());
    if (ow) {
      channel.permission_overwrites.push_back(std::move(*ow));
    }
  }

  auto recipients_array = obj["recipients"].toArray();
  for (const auto& val : recipients_array) {
    auto user = parse_user(val.toObject());
    if (user) {
      channel.recipients.push_back(std::move(*user));
    }
  }

  if (obj.contains("last_message_id") && !obj["last_message_id"].isNull()) {
    channel.last_message_id = static_cast<Snowflake>(obj["last_message_id"].toString().toULongLong());
  }

  return channel;
}

std::optional<Message> parse_message(const QJsonObject& obj) {
  if (obj.isEmpty()) {
    log::client()->warn("Failed to parse Message: empty JSON object");
    return std::nullopt;
  }

  Message msg;
  msg.id = static_cast<Snowflake>(obj["id"].toString().toULongLong());
  msg.channel_id = static_cast<Snowflake>(obj["channel_id"].toString().toULongLong());
  msg.content = obj["content"].toString().toStdString();
  msg.timestamp = obj["timestamp"].toString().toStdString();
  if (obj.contains("edited_timestamp") && !obj["edited_timestamp"].isNull()) {
    msg.edited_timestamp = obj["edited_timestamp"].toString().toStdString();
  }
  msg.pinned = obj["pinned"].toBool(false);

  if (obj.contains("author")) {
    auto author = parse_user(obj["author"].toObject());
    if (author) {
      msg.author = std::move(*author);
    }
  }

  // Message type
  msg.type = obj["type"].toInt(0);

  // Referenced message (reply target)
  if (obj.contains("message_reference") && obj["message_reference"].isObject()) {
    auto ref = obj["message_reference"].toObject();
    if (ref.contains("message_id")) {
      msg.referenced_message_id = static_cast<Snowflake>(ref["message_id"].toString().toULongLong());
    }
  }
  if (obj.contains("referenced_message") && obj["referenced_message"].isObject()) {
    auto ref_msg = obj["referenced_message"].toObject();
    if (ref_msg.contains("author") && ref_msg["author"].isObject()) {
      msg.referenced_message_author = ref_msg["author"].toObject()["username"].toString().toStdString();
    }
    if (ref_msg.contains("content")) {
      msg.referenced_message_content = ref_msg["content"].toString().toStdString();
    }
    if (!msg.referenced_message_id.has_value() && ref_msg.contains("id")) {
      msg.referenced_message_id = static_cast<Snowflake>(ref_msg["id"].toString().toULongLong());
    }
  }

  // Mentions
  msg.mention_everyone = obj["mention_everyone"].toBool(false);
  for (const auto& val : obj["mentions"].toArray()) {
    auto mobj = val.toObject();
    Mention mention;
    mention.id = static_cast<Snowflake>(mobj["id"].toString().toULongLong());
    mention.username = mobj["username"].toString().toStdString();
    msg.mentions.push_back(std::move(mention));
  }
  for (const auto& val : obj["mention_roles"].toArray()) {
    msg.mention_roles.push_back(static_cast<Snowflake>(val.toString().toULongLong()));
  }

  // Attachments
  for (const auto& val : obj["attachments"].toArray()) {
    auto aobj = val.toObject();
    Attachment att;
    att.id = static_cast<Snowflake>(aobj["id"].toString().toULongLong());
    att.filename = aobj["filename"].toString().toStdString();
    att.url = aobj["url"].toString().toStdString();
    att.proxy_url = aobj["proxy_url"].toString().toStdString();
    att.size = static_cast<std::size_t>(aobj["size"].toInteger(0));
    if (aobj.contains("content_type")) {
      att.content_type = aobj["content_type"].toString().toStdString();
    }
    if (aobj.contains("width") && !aobj["width"].isNull()) {
      att.width = aobj["width"].toInt();
    }
    if (aobj.contains("height") && !aobj["height"].isNull()) {
      att.height = aobj["height"].toInt();
    }
    msg.attachments.push_back(std::move(att));
  }

  // Embeds
  for (const auto& val : obj["embeds"].toArray()) {
    auto eobj = val.toObject();
    Embed embed;
    embed.type = eobj["type"].toString("rich").toStdString();
    if (eobj.contains("title")) embed.title = eobj["title"].toString().toStdString();
    if (eobj.contains("description")) embed.description = eobj["description"].toString().toStdString();
    if (eobj.contains("url")) embed.url = eobj["url"].toString().toStdString();
    if (eobj.contains("color")) embed.color = eobj["color"].toInt();

    if (eobj.contains("provider") && eobj["provider"].isObject()) {
      auto pobj = eobj["provider"].toObject();
      EmbedProvider provider;
      provider.name = pobj["name"].toString().toStdString();
      if (pobj.contains("url")) provider.url = pobj["url"].toString().toStdString();
      embed.provider = std::move(provider);
    }
    if (eobj.contains("author") && eobj["author"].isObject()) {
      auto aobj = eobj["author"].toObject();
      EmbedAuthor embed_author;
      embed_author.name = aobj["name"].toString().toStdString();
      if (aobj.contains("url")) embed_author.url = aobj["url"].toString().toStdString();
      embed.author = std::move(embed_author);
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
      if (iobj.contains("proxy_url")) image.proxy_url = iobj["proxy_url"].toString().toStdString();
      if (iobj.contains("width")) image.width = iobj["width"].toInt();
      if (iobj.contains("height")) image.height = iobj["height"].toInt();
      embed.image = std::move(image);
    }
    if (eobj.contains("thumbnail") && eobj["thumbnail"].isObject()) {
      auto tobj = eobj["thumbnail"].toObject();
      EmbedImage thumb;
      thumb.url = tobj["url"].toString().toStdString();
      if (tobj.contains("proxy_url")) thumb.proxy_url = tobj["proxy_url"].toString().toStdString();
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

  // Reactions
  for (const auto& val : obj["reactions"].toArray()) {
    auto robj = val.toObject();
    Reaction reaction;
    auto emoji_obj = robj["emoji"].toObject();
    reaction.emoji_name = emoji_obj["name"].toString().toStdString();
    if (emoji_obj.contains("id") && !emoji_obj["id"].isNull()) {
      reaction.emoji_id = static_cast<Snowflake>(emoji_obj["id"].toString().toULongLong());
    }
    reaction.count = robj["count"].toInt(0);
    reaction.me = robj["me"].toBool(false);
    msg.reactions.push_back(std::move(reaction));
  }

  // Sticker items
  for (const auto& val : obj["sticker_items"].toArray()) {
    auto sobj = val.toObject();
    StickerItem sticker;
    sticker.id = static_cast<Snowflake>(sobj["id"].toString().toULongLong());
    sticker.name = sobj["name"].toString().toStdString();
    sticker.format_type = sobj["format_type"].toInt(1);
    msg.sticker_items.push_back(std::move(sticker));
  }

  // Components (recursive: ActionRows contain children)
  std::function<Component(const QJsonObject&)> parse_component;
  parse_component = [&](const QJsonObject& cobj) -> Component {
    Component comp;
    comp.type = cobj["type"].toInt();
    if (cobj.contains("custom_id")) comp.custom_id = cobj["custom_id"].toString().toStdString();
    if (cobj.contains("label")) comp.label = cobj["label"].toString().toStdString();
    comp.style = cobj["style"].toInt(0);
    comp.disabled = cobj["disabled"].toBool(false);
    for (const auto& child : cobj["components"].toArray()) {
      comp.children.push_back(parse_component(child.toObject()));
    }
    return comp;
  };
  for (const auto& val : obj["components"].toArray()) {
    msg.components.push_back(parse_component(val.toObject()));
  }

  return msg;
}

std::optional<Message> parse_message(const std::string& json) {
  auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(json));
  if (doc.isNull()) {
    log::client()->warn("Failed to parse Message JSON: document is null");
    return std::nullopt;
  }
  if (!doc.isObject()) {
    log::client()->warn("Failed to parse Message JSON: expected object");
    return std::nullopt;
  }
  return parse_message(doc.object());
}

std::optional<Role> parse_role(const QJsonObject& obj) {
  if (obj.isEmpty()) {
    return std::nullopt;
  }

  Role role;
  role.id = static_cast<Snowflake>(obj["id"].toString().toULongLong());
  role.name = obj["name"].toString().toStdString();
  role.permissions = obj["permissions"].toString().toULongLong();
  role.position = obj["position"].toInt();
  return role;
}

std::optional<PermissionOverwrite> parse_overwrite(const QJsonObject& obj) {
  if (obj.isEmpty()) {
    return std::nullopt;
  }

  PermissionOverwrite ow;
  ow.id = static_cast<Snowflake>(obj["id"].toString().toULongLong());
  ow.type = obj["type"].toInt();
  ow.allow = obj["allow"].toString().toULongLong();
  ow.deny = obj["deny"].toString().toULongLong();
  return ow;
}

} // namespace kind::json_parse
