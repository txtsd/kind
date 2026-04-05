#include "json/parsers.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <gtest/gtest.h>

using namespace kind::json_parse;

// ============================================================================
// Tier 1: Basic valid parsing, field verification, empty-object rejection
// ============================================================================

TEST(ParseUser, ValidObjectParsesAllFields) {
  QJsonObject obj;
  obj["id"] = "123456789012345678";
  obj["username"] = "testuser";
  obj["discriminator"] = "1234";
  obj["avatar"] = "a_deadbeef";
  obj["bot"] = true;

  auto user = parse_user(obj);
  ASSERT_TRUE(user.has_value());
  EXPECT_EQ(user->id, 123456789012345678ULL);
  EXPECT_EQ(user->username, "testuser");
  EXPECT_EQ(user->discriminator, "1234");
  EXPECT_EQ(user->avatar_hash, "a_deadbeef");
  EXPECT_TRUE(user->bot);
}

TEST(ParseUser, NonBotDefaultsFalse) {
  QJsonObject obj;
  obj["id"] = "100";
  obj["username"] = "human";
  obj["discriminator"] = "0001";
  obj["avatar"] = "abc";

  auto user = parse_user(obj);
  ASSERT_TRUE(user.has_value());
  EXPECT_FALSE(user->bot);
}

TEST(ParseUser, EmptyObjectReturnsNullopt) {
  QJsonObject obj;
  auto user = parse_user(obj);
  EXPECT_FALSE(user.has_value());
}

TEST(ParseUser, FromValidJsonString) {
  std::string json = R"({"id":"42","username":"fromstr","discriminator":"0","avatar":"hash","bot":false})";
  auto user = parse_user(json);
  ASSERT_TRUE(user.has_value());
  EXPECT_EQ(user->id, 42ULL);
  EXPECT_EQ(user->username, "fromstr");
  EXPECT_EQ(user->avatar_hash, "hash");
}

TEST(ParseRole, ValidObjectParsesAllFields) {
  QJsonObject obj;
  obj["id"] = "999888777";
  obj["name"] = "Moderator";
  obj["permissions"] = "1073741824";
  obj["position"] = 5;
  obj["color"] = 0xFF0000;

  auto role = parse_role(obj);
  ASSERT_TRUE(role.has_value());
  EXPECT_EQ(role->id, 999888777ULL);
  EXPECT_EQ(role->name, "Moderator");
  EXPECT_EQ(role->permissions, 1073741824ULL);
  EXPECT_EQ(role->position, 5);
  EXPECT_EQ(role->color, 0xFF0000u);
}

TEST(ParseRole, EmptyObjectReturnsNullopt) {
  auto role = parse_role(QJsonObject{});
  EXPECT_FALSE(role.has_value());
}

TEST(ParseOverwrite, ValidObjectParsesAllFields) {
  QJsonObject obj;
  obj["id"] = "555";
  obj["type"] = 0;
  obj["allow"] = "1024";
  obj["deny"] = "2048";

  auto ow = parse_overwrite(obj);
  ASSERT_TRUE(ow.has_value());
  EXPECT_EQ(ow->id, 555ULL);
  EXPECT_EQ(ow->type, 0);
  EXPECT_EQ(ow->allow, 1024ULL);
  EXPECT_EQ(ow->deny, 2048ULL);
}

TEST(ParseOverwrite, EmptyObjectReturnsNullopt) {
  auto ow = parse_overwrite(QJsonObject{});
  EXPECT_FALSE(ow.has_value());
}

TEST(ParseChannel, ValidObjectParsesAllFields) {
  QJsonObject obj;
  obj["id"] = "100200300";
  obj["guild_id"] = "400500600";
  obj["name"] = "general";
  obj["type"] = 0;
  obj["position"] = 3;
  obj["parent_id"] = "700800900";
  obj["last_message_id"] = "111222333";

  auto ch = parse_channel(obj);
  ASSERT_TRUE(ch.has_value());
  EXPECT_EQ(ch->id, 100200300ULL);
  EXPECT_EQ(ch->guild_id, 400500600ULL);
  EXPECT_EQ(ch->name, "general");
  EXPECT_EQ(ch->type, 0);
  EXPECT_EQ(ch->position, 3);
  ASSERT_TRUE(ch->parent_id.has_value());
  EXPECT_EQ(*ch->parent_id, 700800900ULL);
  EXPECT_EQ(ch->last_message_id, 111222333ULL);
}

TEST(ParseChannel, EmptyObjectReturnsNullopt) {
  auto ch = parse_channel(QJsonObject{});
  EXPECT_FALSE(ch.has_value());
}

TEST(ParseGuild, FlatBotTokenFormatParsesFields) {
  QJsonObject obj;
  obj["id"] = "111222333";
  obj["name"] = "Test Guild";
  obj["icon"] = "iconhash";
  obj["owner_id"] = "444555666";
  obj["roles"] = QJsonArray{};
  obj["channels"] = QJsonArray{};

  auto guild = parse_guild(obj);
  ASSERT_TRUE(guild.has_value());
  EXPECT_EQ(guild->id, 111222333ULL);
  EXPECT_EQ(guild->name, "Test Guild");
  EXPECT_EQ(guild->icon_hash, "iconhash");
  EXPECT_EQ(guild->owner_id, 444555666ULL);
  EXPECT_TRUE(guild->roles.empty());
  EXPECT_TRUE(guild->channels.empty());
}

TEST(ParseGuild, EmptyObjectReturnsNullopt) {
  auto guild = parse_guild(QJsonObject{});
  EXPECT_FALSE(guild.has_value());
}

// ============================================================================
// Tier 2: Edge cases around partial updates, nested formats, optional fields,
//         and sub-object arrays
// ============================================================================

TEST(MergeUser, PartialUpdateOnlyChangesProvidedFields) {
  kind::User existing;
  existing.id = 1;
  existing.username = "original";
  existing.discriminator = "0001";
  existing.avatar_hash = "oldhash";
  existing.bot = false;

  QJsonObject patch;
  patch["username"] = "updated";

  merge_user(existing, patch);

  EXPECT_EQ(existing.id, 1ULL);
  EXPECT_EQ(existing.username, "updated");
  EXPECT_EQ(existing.discriminator, "0001");
  EXPECT_EQ(existing.avatar_hash, "oldhash");
  EXPECT_FALSE(existing.bot);
}

TEST(MergeUser, EmptyPatchChangesNothing) {
  kind::User existing;
  existing.id = 10;
  existing.username = "keep";
  existing.discriminator = "9999";
  existing.avatar_hash = "keep_hash";
  existing.bot = true;

  merge_user(existing, QJsonObject{});

  EXPECT_EQ(existing.id, 10ULL);
  EXPECT_EQ(existing.username, "keep");
  EXPECT_EQ(existing.discriminator, "9999");
  EXPECT_EQ(existing.avatar_hash, "keep_hash");
  EXPECT_TRUE(existing.bot);
}

TEST(MergeUser, CanChangeMultipleFields) {
  kind::User existing;
  existing.id = 1;
  existing.username = "old";
  existing.discriminator = "0001";
  existing.avatar_hash = "old_hash";
  existing.bot = false;

  QJsonObject patch;
  patch["username"] = "new_name";
  patch["avatar"] = "new_hash";
  patch["bot"] = true;

  merge_user(existing, patch);

  EXPECT_EQ(existing.username, "new_name");
  EXPECT_EQ(existing.avatar_hash, "new_hash");
  EXPECT_TRUE(existing.bot);
  EXPECT_EQ(existing.discriminator, "0001");
}

TEST(ParseGuild, UserTokenNestedPropertiesFormat) {
  QJsonObject props;
  props["name"] = "Nested Guild";
  props["icon"] = "nested_icon";
  props["owner_id"] = "777888999";

  QJsonObject obj;
  obj["id"] = "111";
  obj["properties"] = props;
  obj["roles"] = QJsonArray{};
  obj["channels"] = QJsonArray{};

  auto guild = parse_guild(obj);
  ASSERT_TRUE(guild.has_value());
  EXPECT_EQ(guild->name, "Nested Guild");
  EXPECT_EQ(guild->icon_hash, "nested_icon");
  EXPECT_EQ(guild->owner_id, 777888999ULL);
}

TEST(ParseGuild, FlatFieldsIgnoredWhenPropertiesPresent) {
  // When "properties" key exists, the top-level name/icon/owner_id should
  // be ignored in favor of the nested values.
  QJsonObject props;
  props["name"] = "correct";
  props["icon"] = "correct_icon";
  props["owner_id"] = "1";

  QJsonObject obj;
  obj["id"] = "1";
  obj["name"] = "wrong";
  obj["icon"] = "wrong_icon";
  obj["owner_id"] = "999";
  obj["properties"] = props;
  obj["roles"] = QJsonArray{};
  obj["channels"] = QJsonArray{};

  auto guild = parse_guild(obj);
  ASSERT_TRUE(guild.has_value());
  EXPECT_EQ(guild->name, "correct");
  EXPECT_EQ(guild->icon_hash, "correct_icon");
  EXPECT_EQ(guild->owner_id, 1ULL);
}

TEST(ParseChannel, NullParentIdYieldsNullopt) {
  QJsonObject obj;
  obj["id"] = "1";
  obj["name"] = "orphan";
  obj["type"] = 0;
  obj["position"] = 0;
  obj["parent_id"] = QJsonValue(QJsonValue::Null);

  auto ch = parse_channel(obj);
  ASSERT_TRUE(ch.has_value());
  EXPECT_FALSE(ch->parent_id.has_value());
}

TEST(ParseChannel, AbsentParentIdYieldsNullopt) {
  QJsonObject obj;
  obj["id"] = "1";
  obj["name"] = "no_parent";
  obj["type"] = 0;
  obj["position"] = 0;
  // parent_id not set at all

  auto ch = parse_channel(obj);
  ASSERT_TRUE(ch.has_value());
  EXPECT_FALSE(ch->parent_id.has_value());
}

TEST(ParseChannel, WithPermissionOverwritesArray) {
  QJsonObject ow1;
  ow1["id"] = "10";
  ow1["type"] = 0;
  ow1["allow"] = "1024";
  ow1["deny"] = "0";

  QJsonObject ow2;
  ow2["id"] = "20";
  ow2["type"] = 1;
  ow2["allow"] = "0";
  ow2["deny"] = "2048";

  QJsonObject obj;
  obj["id"] = "1";
  obj["name"] = "restricted";
  obj["type"] = 0;
  obj["position"] = 0;
  obj["permission_overwrites"] = QJsonArray{ow1, ow2};

  auto ch = parse_channel(obj);
  ASSERT_TRUE(ch.has_value());
  ASSERT_EQ(ch->permission_overwrites.size(), 2u);
  EXPECT_EQ(ch->permission_overwrites[0].id, 10ULL);
  EXPECT_EQ(ch->permission_overwrites[0].type, 0);
  EXPECT_EQ(ch->permission_overwrites[0].allow, 1024ULL);
  EXPECT_EQ(ch->permission_overwrites[1].id, 20ULL);
  EXPECT_EQ(ch->permission_overwrites[1].type, 1);
  EXPECT_EQ(ch->permission_overwrites[1].deny, 2048ULL);
}

TEST(ParseChannel, WithRecipientsArray) {
  QJsonObject u1;
  u1["id"] = "50";
  u1["username"] = "alice";
  u1["discriminator"] = "0001";
  u1["avatar"] = "a1";

  QJsonObject u2;
  u2["id"] = "60";
  u2["username"] = "bob";
  u2["discriminator"] = "0002";
  u2["avatar"] = "b2";

  QJsonObject obj;
  obj["id"] = "1";
  obj["name"] = "";
  obj["type"] = 1; // DM
  obj["position"] = 0;
  obj["recipients"] = QJsonArray{u1, u2};

  auto ch = parse_channel(obj);
  ASSERT_TRUE(ch.has_value());
  ASSERT_EQ(ch->recipients.size(), 2u);
  EXPECT_EQ(ch->recipients[0].id, 50ULL);
  EXPECT_EQ(ch->recipients[0].username, "alice");
  EXPECT_EQ(ch->recipients[1].id, 60ULL);
  EXPECT_EQ(ch->recipients[1].username, "bob");
}

TEST(ParseChannel, NullLastMessageIdDefaultsToZero) {
  QJsonObject obj;
  obj["id"] = "1";
  obj["name"] = "test";
  obj["type"] = 0;
  obj["position"] = 0;
  obj["last_message_id"] = QJsonValue(QJsonValue::Null);

  auto ch = parse_channel(obj);
  ASSERT_TRUE(ch.has_value());
  EXPECT_EQ(ch->last_message_id, 0ULL);
}

TEST(ParseGuild, WithRolesAndChannels) {
  QJsonObject role_obj;
  role_obj["id"] = "900";
  role_obj["name"] = "Admin";
  role_obj["permissions"] = "8";
  role_obj["position"] = 1;
  role_obj["color"] = 0x00FF00;

  QJsonObject ch_obj;
  ch_obj["id"] = "800";
  ch_obj["name"] = "lobby";
  ch_obj["type"] = 0;
  ch_obj["position"] = 0;

  QJsonObject obj;
  obj["id"] = "500";
  obj["name"] = "Guild With Stuff";
  obj["icon"] = "";
  obj["owner_id"] = "1";
  obj["roles"] = QJsonArray{role_obj};
  obj["channels"] = QJsonArray{ch_obj};

  auto guild = parse_guild(obj);
  ASSERT_TRUE(guild.has_value());
  ASSERT_EQ(guild->roles.size(), 1u);
  EXPECT_EQ(guild->roles[0].name, "Admin");
  EXPECT_EQ(guild->roles[0].permissions, 8ULL);
  ASSERT_EQ(guild->channels.size(), 1u);
  EXPECT_EQ(guild->channels[0].name, "lobby");
  // parse_guild assigns guild_id to channels
  EXPECT_EQ(guild->channels[0].guild_id, 500ULL);
}

TEST(ParseOverwrite, MemberType) {
  QJsonObject obj;
  obj["id"] = "77";
  obj["type"] = 1;
  obj["allow"] = "0";
  obj["deny"] = "64";

  auto ow = parse_overwrite(obj);
  ASSERT_TRUE(ow.has_value());
  EXPECT_EQ(ow->type, 1);
}

// ============================================================================
// Tier 3: Degenerate input, stress, boundary values, things real developers
//         will inevitably send our way
// ============================================================================

TEST(ParseUser, MissingFieldsDoNotCrashAndProduceDefaults) {
  // Only "id" present. Other string fields should be empty, bot should default false.
  QJsonObject obj;
  obj["id"] = "999";

  auto user = parse_user(obj);
  ASSERT_TRUE(user.has_value());
  EXPECT_EQ(user->id, 999ULL);
  EXPECT_EQ(user->username, "");
  EXPECT_EQ(user->discriminator, "");
  EXPECT_EQ(user->avatar_hash, "");
  EXPECT_FALSE(user->bot);
}

TEST(ParseUser, AllFieldsMissingExceptSomeArbitraryKey) {
  // Object is not empty, but has none of the expected keys.
  // Should still not crash; fields get their type defaults.
  QJsonObject obj;
  obj["totally_unknown"] = "whatever";

  auto user = parse_user(obj);
  ASSERT_TRUE(user.has_value());
  EXPECT_EQ(user->id, 0ULL);
  EXPECT_EQ(user->username, "");
  EXPECT_FALSE(user->bot);
}

TEST(ParseUser, FromNullJsonStringReturnsNullopt) {
  auto user = parse_user(std::string(""));
  EXPECT_FALSE(user.has_value());
}

TEST(ParseUser, FromGarbageJsonStringReturnsNullopt) {
  auto user = parse_user(std::string("not json at all {{{"));
  EXPECT_FALSE(user.has_value());
}

TEST(ParseUser, FromJsonArrayStringReturnsNullopt) {
  auto user = parse_user(std::string(R"([{"id":"1"}])"));
  EXPECT_FALSE(user.has_value());
}

TEST(ParseUser, FromEmptyObjectStringReturnsNullopt) {
  // parse_user(QJsonObject) returns nullopt for empty objects, and the
  // string overload delegates to it, so this should also be nullopt.
  auto user = parse_user(std::string("{}"));
  EXPECT_FALSE(user.has_value());
}

TEST(ParseUser, LargeSnowflakeId) {
  QJsonObject obj;
  // Max realistic Discord snowflake: 18 digits
  obj["id"] = "999999999999999999";
  obj["username"] = "big_id";
  obj["discriminator"] = "0";
  obj["avatar"] = "";

  auto user = parse_user(obj);
  ASSERT_TRUE(user.has_value());
  EXPECT_EQ(user->id, 999999999999999999ULL);
}

TEST(ParseGuild, FiftyRolesAndFiftyChannels) {
  QJsonArray roles;
  for (int i = 0; i < 50; ++i) {
    QJsonObject r;
    r["id"] = QString::number(1000 + i);
    r["name"] = QString("role_%1").arg(i);
    r["permissions"] = "0";
    r["position"] = i;
    r["color"] = 0;
    roles.append(r);
  }

  QJsonArray channels;
  for (int i = 0; i < 50; ++i) {
    QJsonObject c;
    c["id"] = QString::number(2000 + i);
    c["name"] = QString("channel_%1").arg(i);
    c["type"] = 0;
    c["position"] = i;
    channels.append(c);
  }

  QJsonObject obj;
  obj["id"] = "5000";
  obj["name"] = "Big Guild";
  obj["icon"] = "";
  obj["owner_id"] = "1";
  obj["roles"] = roles;
  obj["channels"] = channels;

  auto guild = parse_guild(obj);
  ASSERT_TRUE(guild.has_value());
  EXPECT_EQ(guild->roles.size(), 50u);
  EXPECT_EQ(guild->channels.size(), 50u);

  // Spot-check first and last
  EXPECT_EQ(guild->roles[0].id, 1000ULL);
  EXPECT_EQ(guild->roles[0].name, "role_0");
  EXPECT_EQ(guild->roles[49].id, 1049ULL);
  EXPECT_EQ(guild->roles[49].name, "role_49");

  EXPECT_EQ(guild->channels[0].id, 2000ULL);
  EXPECT_EQ(guild->channels[49].id, 2049ULL);

  // All channels should have guild_id set
  for (const auto& ch : guild->channels) {
    EXPECT_EQ(ch.guild_id, 5000ULL);
  }
}

TEST(ParseGuild, SkipsMalformedRolesAndChannelsGracefully) {
  QJsonObject good_role;
  good_role["id"] = "1";
  good_role["name"] = "good";
  good_role["permissions"] = "0";
  good_role["position"] = 0;
  good_role["color"] = 0;

  // Empty object will be rejected by parse_role
  QJsonObject bad_role;

  QJsonObject good_ch;
  good_ch["id"] = "2";
  good_ch["name"] = "good_ch";
  good_ch["type"] = 0;
  good_ch["position"] = 0;

  QJsonObject bad_ch;

  QJsonObject obj;
  obj["id"] = "1";
  obj["name"] = "Mixed";
  obj["icon"] = "";
  obj["owner_id"] = "1";
  obj["roles"] = QJsonArray{good_role, bad_role, good_role};
  obj["channels"] = QJsonArray{bad_ch, good_ch};

  auto guild = parse_guild(obj);
  ASSERT_TRUE(guild.has_value());
  EXPECT_EQ(guild->roles.size(), 2u);
  EXPECT_EQ(guild->channels.size(), 1u);
}

TEST(ParseRole, ColorZeroIsValid) {
  QJsonObject obj;
  obj["id"] = "1";
  obj["name"] = "@everyone";
  obj["permissions"] = "0";
  obj["position"] = 0;
  obj["color"] = 0;

  auto role = parse_role(obj);
  ASSERT_TRUE(role.has_value());
  EXPECT_EQ(role->color, 0u);
}

TEST(ParseRole, AbsentColorDefaultsToZero) {
  QJsonObject obj;
  obj["id"] = "1";
  obj["name"] = "no_color";
  obj["permissions"] = "0";
  obj["position"] = 0;
  // color not set

  auto role = parse_role(obj);
  ASSERT_TRUE(role.has_value());
  EXPECT_EQ(role->color, 0u);
}

TEST(ParseRole, LargePermissionBitfield) {
  QJsonObject obj;
  obj["id"] = "1";
  obj["name"] = "admin";
  // ADMINISTRATOR (0x8) | MANAGE_GUILD (0x20) | ... combined to a large value
  obj["permissions"] = "562949953421311";
  obj["position"] = 99;
  obj["color"] = 0;

  auto role = parse_role(obj);
  ASSERT_TRUE(role.has_value());
  EXPECT_EQ(role->permissions, 562949953421311ULL);
}

TEST(ParseOverwrite, LargePermissionValues) {
  QJsonObject obj;
  obj["id"] = "1";
  obj["type"] = 0;
  obj["allow"] = "1099511627775";
  obj["deny"] = "1099511627775";

  auto ow = parse_overwrite(obj);
  ASSERT_TRUE(ow.has_value());
  EXPECT_EQ(ow->allow, 1099511627775ULL);
  EXPECT_EQ(ow->deny, 1099511627775ULL);
}

TEST(ParseOverwrite, ZeroPermissions) {
  QJsonObject obj;
  obj["id"] = "1";
  obj["type"] = 0;
  obj["allow"] = "0";
  obj["deny"] = "0";

  auto ow = parse_overwrite(obj);
  ASSERT_TRUE(ow.has_value());
  EXPECT_EQ(ow->allow, 0ULL);
  EXPECT_EQ(ow->deny, 0ULL);
}

TEST(ParseChannel, EmptyOverwritesAndRecipientsArrays) {
  QJsonObject obj;
  obj["id"] = "1";
  obj["name"] = "empty_arrays";
  obj["type"] = 0;
  obj["position"] = 0;
  obj["permission_overwrites"] = QJsonArray{};
  obj["recipients"] = QJsonArray{};

  auto ch = parse_channel(obj);
  ASSERT_TRUE(ch.has_value());
  EXPECT_TRUE(ch->permission_overwrites.empty());
  EXPECT_TRUE(ch->recipients.empty());
}

TEST(ParseChannel, MissingOptionalArraysDefaultEmpty) {
  QJsonObject obj;
  obj["id"] = "1";
  obj["name"] = "bare";
  obj["type"] = 0;
  obj["position"] = 0;

  auto ch = parse_channel(obj);
  ASSERT_TRUE(ch.has_value());
  EXPECT_TRUE(ch->permission_overwrites.empty());
  EXPECT_TRUE(ch->recipients.empty());
}

TEST(ParseChannel, VoiceChannelType) {
  QJsonObject obj;
  obj["id"] = "1";
  obj["guild_id"] = "2";
  obj["name"] = "Voice Chat";
  obj["type"] = 2; // GUILD_VOICE
  obj["position"] = 1;

  auto ch = parse_channel(obj);
  ASSERT_TRUE(ch.has_value());
  EXPECT_EQ(ch->type, 2);
}

TEST(MergeUser, CanOverrideIdItself) {
  kind::User existing;
  existing.id = 1;
  existing.username = "a";

  QJsonObject patch;
  patch["id"] = "999";

  merge_user(existing, patch);
  EXPECT_EQ(existing.id, 999ULL);
  EXPECT_EQ(existing.username, "a");
}

TEST(MergeUser, BotFlagCanBeCleared) {
  kind::User existing;
  existing.bot = true;

  QJsonObject patch;
  patch["bot"] = false;

  merge_user(existing, patch);
  EXPECT_FALSE(existing.bot);
}

TEST(ParseGuild, PropertiesKeyExistsButIsNotObjectFallsBackToFlat) {
  // If "properties" is present but isn't an object (e.g., null or string),
  // the parser should fall through to flat fields.
  QJsonObject obj;
  obj["id"] = "1";
  obj["name"] = "flat_fallback";
  obj["icon"] = "flat_icon";
  obj["owner_id"] = "42";
  obj["properties"] = QJsonValue(QJsonValue::Null);
  obj["roles"] = QJsonArray{};
  obj["channels"] = QJsonArray{};

  auto guild = parse_guild(obj);
  ASSERT_TRUE(guild.has_value());
  EXPECT_EQ(guild->name, "flat_fallback");
  EXPECT_EQ(guild->icon_hash, "flat_icon");
  EXPECT_EQ(guild->owner_id, 42ULL);
}

TEST(ParseChannel, MultipleOverwritesOnSameChannel) {
  // Realistic scenario: @everyone deny + specific role allow + member override
  QJsonArray overwrites;
  for (int i = 0; i < 10; ++i) {
    QJsonObject ow;
    ow["id"] = QString::number(100 + i);
    ow["type"] = (i < 5) ? 0 : 1;
    ow["allow"] = QString::number(1ULL << i);
    ow["deny"] = QString::number(1ULL << (i + 10));
    overwrites.append(ow);
  }

  QJsonObject obj;
  obj["id"] = "1";
  obj["name"] = "locked_down";
  obj["type"] = 0;
  obj["position"] = 0;
  obj["permission_overwrites"] = overwrites;

  auto ch = parse_channel(obj);
  ASSERT_TRUE(ch.has_value());
  ASSERT_EQ(ch->permission_overwrites.size(), 10u);
  EXPECT_EQ(ch->permission_overwrites[0].allow, 1ULL);
  EXPECT_EQ(ch->permission_overwrites[0].deny, 1024ULL);
  EXPECT_EQ(ch->permission_overwrites[9].allow, 512ULL);
  EXPECT_EQ(ch->permission_overwrites[9].deny, 524288ULL);
}

TEST(ParseUser, UnicodeUsername) {
  QJsonObject obj;
  obj["id"] = "1";
  obj["username"] = QString::fromUtf8(u8"\u00e9\u00e8\u00ea\u00eb \U0001f525 \u4e16\u754c");
  obj["discriminator"] = "0";
  obj["avatar"] = "";

  auto user = parse_user(obj);
  ASSERT_TRUE(user.has_value());
  // Verify round-trip: it should contain the Unicode characters intact
  EXPECT_FALSE(user->username.empty());
  EXPECT_NE(user->username.find("\xc3\xa9"), std::string::npos); // e-acute in UTF-8
}

TEST(ParseUser, EmptyStringId) {
  // Discord IDs are strings, but an empty string converts to 0 via toULongLong
  QJsonObject obj;
  obj["id"] = "";
  obj["username"] = "no_id";
  obj["discriminator"] = "";
  obj["avatar"] = "";

  auto user = parse_user(obj);
  ASSERT_TRUE(user.has_value());
  EXPECT_EQ(user->id, 0ULL);
}

TEST(ParseGuild, EmptyPropertiesObjectUsesEmptyValues) {
  QJsonObject obj;
  obj["id"] = "1";
  obj["properties"] = QJsonObject{};
  obj["roles"] = QJsonArray{};
  obj["channels"] = QJsonArray{};

  auto guild = parse_guild(obj);
  ASSERT_TRUE(guild.has_value());
  // Empty properties object IS an object, so the parser enters the properties
  // branch and reads empty strings for name/icon
  EXPECT_EQ(guild->name, "");
  EXPECT_EQ(guild->icon_hash, "");
  EXPECT_EQ(guild->owner_id, 0ULL);
}
