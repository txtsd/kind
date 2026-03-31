#include "json/parsers.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <gtest/gtest.h>

static QJsonObject make_base_message() {
  QJsonObject obj;
  obj["id"] = "12345";
  obj["channel_id"] = "67890";
  obj["content"] = "hello";
  obj["timestamp"] = "2026-01-01T00:00:00.000Z";
  obj["type"] = 0;
  QJsonObject author;
  author["id"] = "111";
  author["username"] = "testuser";
  author["discriminator"] = "0001";
  author["avatar"] = "abc";
  author["bot"] = false;
  obj["author"] = author;
  return obj;
}

TEST(MessageParse, ParsesType) {
  auto obj = make_base_message();
  obj["type"] = 7;
  auto msg = kind::json_parse::parse_message(obj);
  ASSERT_TRUE(msg.has_value());
  EXPECT_EQ(msg->type, 7);
}

TEST(MessageParse, ParsesReferencedMessageId) {
  auto obj = make_base_message();
  QJsonObject ref;
  ref["message_id"] = "99999";
  obj["message_reference"] = ref;
  auto msg = kind::json_parse::parse_message(obj);
  ASSERT_TRUE(msg.has_value());
  ASSERT_TRUE(msg->referenced_message_id.has_value());
  EXPECT_EQ(*msg->referenced_message_id, 99999u);
}

TEST(MessageParse, NoMessageReferenceYieldsNullopt) {
  auto obj = make_base_message();
  auto msg = kind::json_parse::parse_message(obj);
  ASSERT_TRUE(msg.has_value());
  EXPECT_FALSE(msg->referenced_message_id.has_value());
}

TEST(MessageParse, ParsesMentions) {
  auto obj = make_base_message();
  QJsonArray mentions;
  QJsonObject m;
  m["id"] = "222";
  m["username"] = "mentioned";
  mentions.append(m);
  obj["mentions"] = mentions;
  obj["mention_everyone"] = true;
  auto msg = kind::json_parse::parse_message(obj);
  ASSERT_TRUE(msg.has_value());
  EXPECT_TRUE(msg->mention_everyone);
  ASSERT_EQ(msg->mentions.size(), 1u);
  EXPECT_EQ(msg->mentions[0].id, 222u);
  EXPECT_EQ(msg->mentions[0].username, "mentioned");
}

TEST(MessageParse, ParsesMentionRoles) {
  auto obj = make_base_message();
  QJsonArray roles;
  roles.append("333");
  roles.append("444");
  obj["mention_roles"] = roles;
  auto msg = kind::json_parse::parse_message(obj);
  ASSERT_TRUE(msg.has_value());
  ASSERT_EQ(msg->mention_roles.size(), 2u);
  EXPECT_EQ(msg->mention_roles[0], 333u);
  EXPECT_EQ(msg->mention_roles[1], 444u);
}

TEST(MessageParse, ParsesAttachments) {
  auto obj = make_base_message();
  QJsonArray atts;
  QJsonObject att;
  att["id"] = "555";
  att["filename"] = "photo.png";
  att["url"] = "https://cdn.example.com/photo.png";
  att["size"] = 1024;
  att["width"] = 800;
  att["height"] = 600;
  atts.append(att);
  obj["attachments"] = atts;
  auto msg = kind::json_parse::parse_message(obj);
  ASSERT_TRUE(msg.has_value());
  ASSERT_EQ(msg->attachments.size(), 1u);
  EXPECT_EQ(msg->attachments[0].filename, "photo.png");
  EXPECT_EQ(msg->attachments[0].url, "https://cdn.example.com/photo.png");
  EXPECT_EQ(msg->attachments[0].size, 1024u);
  ASSERT_TRUE(msg->attachments[0].width.has_value());
  EXPECT_EQ(*msg->attachments[0].width, 800);
  ASSERT_TRUE(msg->attachments[0].height.has_value());
  EXPECT_EQ(*msg->attachments[0].height, 600);
}

TEST(MessageParse, AttachmentWithoutDimensions) {
  auto obj = make_base_message();
  QJsonArray atts;
  QJsonObject att;
  att["id"] = "555";
  att["filename"] = "readme.txt";
  att["url"] = "https://cdn.example.com/readme.txt";
  att["size"] = 256;
  atts.append(att);
  obj["attachments"] = atts;
  auto msg = kind::json_parse::parse_message(obj);
  ASSERT_TRUE(msg.has_value());
  ASSERT_EQ(msg->attachments.size(), 1u);
  EXPECT_FALSE(msg->attachments[0].width.has_value());
  EXPECT_FALSE(msg->attachments[0].height.has_value());
}

TEST(MessageParse, ParsesFullEmbed) {
  auto obj = make_base_message();
  QJsonObject embed;
  embed["title"] = "Embed Title";
  embed["description"] = "Embed desc";
  embed["url"] = "https://example.com";
  embed["color"] = 0xFF0000;
  QJsonObject author;
  author["name"] = "Author Name";
  author["url"] = "https://author.com";
  embed["author"] = author;
  QJsonObject footer;
  footer["text"] = "Footer text";
  embed["footer"] = footer;
  QJsonObject image;
  image["url"] = "https://img.com/img.png";
  image["width"] = 400;
  image["height"] = 300;
  embed["image"] = image;
  QJsonObject thumb;
  thumb["url"] = "https://img.com/thumb.png";
  embed["thumbnail"] = thumb;
  QJsonArray fields;
  QJsonObject field;
  field["name"] = "Field 1";
  field["value"] = "Value 1";
  field["inline"] = true;
  fields.append(field);
  embed["fields"] = fields;
  QJsonArray embeds;
  embeds.append(embed);
  obj["embeds"] = embeds;

  auto msg = kind::json_parse::parse_message(obj);
  ASSERT_TRUE(msg.has_value());
  ASSERT_EQ(msg->embeds.size(), 1u);
  auto& e = msg->embeds[0];
  ASSERT_TRUE(e.title.has_value());
  EXPECT_EQ(*e.title, "Embed Title");
  ASSERT_TRUE(e.description.has_value());
  EXPECT_EQ(*e.description, "Embed desc");
  ASSERT_TRUE(e.url.has_value());
  EXPECT_EQ(*e.url, "https://example.com");
  ASSERT_TRUE(e.color.has_value());
  EXPECT_EQ(*e.color, 0xFF0000);
  ASSERT_TRUE(e.author.has_value());
  EXPECT_EQ(e.author->name, "Author Name");
  ASSERT_TRUE(e.author->url.has_value());
  EXPECT_EQ(*e.author->url, "https://author.com");
  ASSERT_TRUE(e.footer.has_value());
  EXPECT_EQ(e.footer->text, "Footer text");
  ASSERT_TRUE(e.image.has_value());
  EXPECT_EQ(e.image->url, "https://img.com/img.png");
  ASSERT_TRUE(e.image->width.has_value());
  EXPECT_EQ(*e.image->width, 400);
  ASSERT_TRUE(e.image->height.has_value());
  EXPECT_EQ(*e.image->height, 300);
  ASSERT_TRUE(e.thumbnail.has_value());
  EXPECT_EQ(e.thumbnail->url, "https://img.com/thumb.png");
  EXPECT_FALSE(e.thumbnail->width.has_value());
  ASSERT_EQ(e.fields.size(), 1u);
  EXPECT_EQ(e.fields[0].name, "Field 1");
  EXPECT_EQ(e.fields[0].value, "Value 1");
  EXPECT_TRUE(e.fields[0].inline_field);
}

TEST(MessageParse, ParsesMinimalEmbed) {
  auto obj = make_base_message();
  QJsonObject embed;
  embed["description"] = "Just a description";
  QJsonArray embeds;
  embeds.append(embed);
  obj["embeds"] = embeds;

  auto msg = kind::json_parse::parse_message(obj);
  ASSERT_TRUE(msg.has_value());
  ASSERT_EQ(msg->embeds.size(), 1u);
  auto& e = msg->embeds[0];
  EXPECT_FALSE(e.title.has_value());
  ASSERT_TRUE(e.description.has_value());
  EXPECT_EQ(*e.description, "Just a description");
  EXPECT_FALSE(e.color.has_value());
  EXPECT_FALSE(e.author.has_value());
  EXPECT_FALSE(e.footer.has_value());
  EXPECT_FALSE(e.image.has_value());
  EXPECT_FALSE(e.thumbnail.has_value());
  EXPECT_TRUE(e.fields.empty());
}

TEST(MessageParse, ParsesUnicodeReaction) {
  auto obj = make_base_message();
  QJsonArray reactions;
  QJsonObject r;
  QJsonObject emoji;
  emoji["name"] = "\xF0\x9F\x91\x8D"; // thumbs up
  emoji["id"] = QJsonValue::Null;
  r["emoji"] = emoji;
  r["count"] = 5;
  r["me"] = true;
  reactions.append(r);
  obj["reactions"] = reactions;
  auto msg = kind::json_parse::parse_message(obj);
  ASSERT_TRUE(msg.has_value());
  ASSERT_EQ(msg->reactions.size(), 1u);
  EXPECT_EQ(msg->reactions[0].emoji_name, "\xF0\x9F\x91\x8D");
  EXPECT_FALSE(msg->reactions[0].emoji_id.has_value());
  EXPECT_EQ(msg->reactions[0].count, 5);
  EXPECT_TRUE(msg->reactions[0].me);
}

TEST(MessageParse, ParsesCustomEmojiReaction) {
  auto obj = make_base_message();
  QJsonArray reactions;
  QJsonObject r;
  QJsonObject emoji;
  emoji["name"] = "custom";
  emoji["id"] = "777";
  r["emoji"] = emoji;
  r["count"] = 2;
  r["me"] = false;
  reactions.append(r);
  obj["reactions"] = reactions;
  auto msg = kind::json_parse::parse_message(obj);
  ASSERT_TRUE(msg.has_value());
  ASSERT_EQ(msg->reactions.size(), 1u);
  ASSERT_TRUE(msg->reactions[0].emoji_id.has_value());
  EXPECT_EQ(*msg->reactions[0].emoji_id, 777u);
  EXPECT_EQ(msg->reactions[0].count, 2);
  EXPECT_FALSE(msg->reactions[0].me);
}

TEST(MessageParse, ParsesStickerItems) {
  auto obj = make_base_message();
  QJsonArray stickers;
  QJsonObject s;
  s["id"] = "888";
  s["name"] = "wave";
  s["format_type"] = 2;
  stickers.append(s);
  obj["sticker_items"] = stickers;
  auto msg = kind::json_parse::parse_message(obj);
  ASSERT_TRUE(msg.has_value());
  ASSERT_EQ(msg->sticker_items.size(), 1u);
  EXPECT_EQ(msg->sticker_items[0].id, 888u);
  EXPECT_EQ(msg->sticker_items[0].name, "wave");
  EXPECT_EQ(msg->sticker_items[0].format_type, 2);
}

TEST(MessageParse, ParsesComponents) {
  auto obj = make_base_message();
  QJsonArray components;
  QJsonObject action_row;
  action_row["type"] = 1;
  QJsonArray buttons;
  QJsonObject btn;
  btn["type"] = 2;
  btn["custom_id"] = "btn_1";
  btn["label"] = "Click me";
  btn["style"] = 1;
  btn["disabled"] = false;
  buttons.append(btn);
  action_row["components"] = buttons;
  components.append(action_row);
  obj["components"] = components;

  auto msg = kind::json_parse::parse_message(obj);
  ASSERT_TRUE(msg.has_value());
  ASSERT_EQ(msg->components.size(), 1u);
  EXPECT_EQ(msg->components[0].type, 1);
  ASSERT_EQ(msg->components[0].children.size(), 1u);
  EXPECT_EQ(msg->components[0].children[0].type, 2);
  ASSERT_TRUE(msg->components[0].children[0].custom_id.has_value());
  EXPECT_EQ(*msg->components[0].children[0].custom_id, "btn_1");
  ASSERT_TRUE(msg->components[0].children[0].label.has_value());
  EXPECT_EQ(*msg->components[0].children[0].label, "Click me");
  EXPECT_EQ(msg->components[0].children[0].style, 1);
  EXPECT_FALSE(msg->components[0].children[0].disabled);
}

TEST(MessageParse, EmptyArraysOmitted) {
  auto obj = make_base_message();
  auto msg = kind::json_parse::parse_message(obj);
  ASSERT_TRUE(msg.has_value());
  EXPECT_TRUE(msg->mentions.empty());
  EXPECT_TRUE(msg->mention_roles.empty());
  EXPECT_TRUE(msg->attachments.empty());
  EXPECT_TRUE(msg->embeds.empty());
  EXPECT_TRUE(msg->reactions.empty());
  EXPECT_TRUE(msg->sticker_items.empty());
  EXPECT_TRUE(msg->components.empty());
  EXPECT_FALSE(msg->mention_everyone);
  EXPECT_EQ(msg->type, 0);
}
