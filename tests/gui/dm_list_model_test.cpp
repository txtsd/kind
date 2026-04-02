#include "json/parsers.hpp"
#include "models/dm_list_model.hpp"
#include "store/data_store.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

// =============================================================================
// Helpers
// =============================================================================

static kind::Channel make_dm_channel(kind::Snowflake id, const std::string& username,
                                     kind::Snowflake last_msg_id = 0) {
  kind::Channel ch;
  ch.id = id;
  ch.guild_id = 0;
  ch.type = 1;
  ch.last_message_id = last_msg_id;
  kind::User user;
  user.id = id + 1000;
  user.username = username;
  ch.recipients.push_back(user);
  return ch;
}

static kind::Channel make_dm_channel_with_avatar(kind::Snowflake id, const std::string& username,
                                                 const std::string& avatar_hash,
                                                 kind::Snowflake last_msg_id = 0) {
  auto ch = make_dm_channel(id, username, last_msg_id);
  ch.recipients[0].avatar_hash = avatar_hash;
  return ch;
}

// =============================================================================
// Tier 1: Normal tests - DmListModel
// =============================================================================

TEST(DmListModelTest, SortsByLastMessageIdDescending) {
  kind::gui::DmListModel model;
  std::vector<kind::Channel> channels = {
      make_dm_channel(1, "alice", 100),
      make_dm_channel(2, "bob", 300),
      make_dm_channel(3, "carol", 200),
  };
  model.set_channels(channels);

  ASSERT_EQ(model.rowCount(), 3);
  // Most recent first: bob(300), carol(200), alice(100)
  auto idx0 = model.index(0, 0);
  auto idx1 = model.index(1, 0);
  auto idx2 = model.index(2, 0);
  EXPECT_EQ(idx0.data(kind::gui::DmListModel::RecipientNameRole).toString().toStdString(), "bob");
  EXPECT_EQ(idx1.data(kind::gui::DmListModel::RecipientNameRole).toString().toStdString(), "carol");
  EXPECT_EQ(idx2.data(kind::gui::DmListModel::RecipientNameRole).toString().toStdString(), "alice");
}

TEST(DmListModelTest, ProvidesRecipientName) {
  kind::gui::DmListModel model;
  model.set_channels({make_dm_channel(1, "testuser", 10)});

  auto idx = model.index(0, 0);
  EXPECT_EQ(idx.data(kind::gui::DmListModel::RecipientNameRole).toString().toStdString(), "testuser");
}

TEST(DmListModelTest, ProvidesRecipientAvatarUrl) {
  kind::gui::DmListModel model;
  model.set_channels({make_dm_channel_with_avatar(1, "testuser", "abc123hash", 10)});

  auto idx = model.index(0, 0);
  auto url = idx.data(kind::gui::DmListModel::RecipientAvatarUrlRole).toString();
  EXPECT_TRUE(url.contains("cdn.discordapp.com"));
  EXPECT_TRUE(url.contains("abc123hash"));
  EXPECT_TRUE(url.contains(QString::number(1001))); // user id = channel id + 1000
}

TEST(DmListModelTest, ReturnsEmptyAvatarUrlForNoAvatar) {
  kind::gui::DmListModel model;
  model.set_channels({make_dm_channel(1, "noavatar", 10)});

  auto idx = model.index(0, 0);
  auto url = idx.data(kind::gui::DmListModel::RecipientAvatarUrlRole).toString();
  EXPECT_TRUE(url.isEmpty());
}

TEST(DmListModelTest, ReturnsUnknownUserForNoRecipients) {
  kind::Channel ch;
  ch.id = 1;
  ch.type = 1;
  ch.last_message_id = 10;
  // No recipients

  kind::gui::DmListModel model;
  model.set_channels({ch});

  auto idx = model.index(0, 0);
  EXPECT_EQ(idx.data(Qt::DisplayRole).toString(), "Unknown User");
  EXPECT_EQ(idx.data(kind::gui::DmListModel::RecipientNameRole).toString(), "Unknown User");
}

TEST(DmListModelTest, ChannelIdAtReturnsCorrectId) {
  kind::gui::DmListModel model;
  std::vector<kind::Channel> channels = {
      make_dm_channel(10, "alice", 100),
      make_dm_channel(20, "bob", 300),
      make_dm_channel(30, "carol", 200),
  };
  model.set_channels(channels);

  // After sorting: bob(300)=id 20, carol(200)=id 30, alice(100)=id 10
  EXPECT_EQ(model.channel_id_at(0), 20u);
  EXPECT_EQ(model.channel_id_at(1), 30u);
  EXPECT_EQ(model.channel_id_at(2), 10u);
}

// =============================================================================
// Tier 1: Normal tests - Channel parser (recipients)
// =============================================================================

TEST(ChannelParseTest, ParsesRecipients) {
  QJsonObject obj;
  obj["id"] = "12345";
  obj["type"] = 1;
  obj["name"] = "";
  obj["position"] = 0;

  QJsonObject userObj;
  userObj["id"] = "99999";
  userObj["username"] = "dmfriend";
  userObj["discriminator"] = "0001";
  userObj["avatar"] = "somehash";
  userObj["bot"] = false;

  QJsonArray recipients;
  recipients.append(userObj);
  obj["recipients"] = recipients;

  auto channel = kind::json_parse::parse_channel(obj);
  ASSERT_TRUE(channel.has_value());
  ASSERT_EQ(channel->recipients.size(), 1u);
  EXPECT_EQ(channel->recipients[0].id, 99999u);
  EXPECT_EQ(channel->recipients[0].username, "dmfriend");
  EXPECT_EQ(channel->recipients[0].avatar_hash, "somehash");
}

TEST(ChannelParseTest, ParsesLastMessageId) {
  QJsonObject obj;
  obj["id"] = "12345";
  obj["type"] = 1;
  obj["name"] = "";
  obj["position"] = 0;
  obj["last_message_id"] = "777888999";

  auto channel = kind::json_parse::parse_channel(obj);
  ASSERT_TRUE(channel.has_value());
  EXPECT_EQ(channel->last_message_id, 777888999u);
}

// =============================================================================
// Tier 1: Normal tests - Store private channels
// =============================================================================

TEST(StorePrivateChannelTest, UpsertPrivateChannel) {
  kind::DataStore store;
  store.upsert_private_channel(make_dm_channel(1, "alice", 100));

  auto pcs = store.private_channels();
  ASSERT_EQ(pcs.size(), 1u);
  EXPECT_EQ(pcs[0].id, 1u);
  EXPECT_EQ(pcs[0].recipients[0].username, "alice");
}

TEST(StorePrivateChannelTest, RemovePrivateChannel) {
  kind::DataStore store;
  store.upsert_private_channel(make_dm_channel(1, "alice", 100));
  ASSERT_EQ(store.private_channels().size(), 1u);

  store.remove_private_channel(1);
  EXPECT_TRUE(store.private_channels().empty());
}

TEST(StorePrivateChannelTest, BulkUpsertPrivateChannels) {
  kind::DataStore store;
  std::vector<kind::Channel> channels = {
      make_dm_channel(1, "alice", 100),
      make_dm_channel(2, "bob", 200),
      make_dm_channel(3, "carol", 300),
  };
  store.bulk_upsert_private_channels(std::move(channels));

  auto pcs = store.private_channels();
  ASSERT_EQ(pcs.size(), 3u);
}

TEST(StorePrivateChannelTest, PrivateChannelsSortedByRecency) {
  kind::DataStore store;
  store.upsert_private_channel(make_dm_channel(1, "alice", 100));
  store.upsert_private_channel(make_dm_channel(2, "bob", 300));
  store.upsert_private_channel(make_dm_channel(3, "carol", 200));

  auto pcs = store.private_channels();
  ASSERT_EQ(pcs.size(), 3u);
  EXPECT_EQ(pcs[0].id, 2u); // bob, last_message_id=300
  EXPECT_EQ(pcs[1].id, 3u); // carol, last_message_id=200
  EXPECT_EQ(pcs[2].id, 1u); // alice, last_message_id=100
}

TEST(StorePrivateChannelTest, UpdatePrivateChannelLastMessage) {
  kind::DataStore store;
  store.upsert_private_channel(make_dm_channel(1, "alice", 100));

  store.update_private_channel_last_message(1, 500);

  auto pcs = store.private_channels();
  ASSERT_EQ(pcs.size(), 1u);
  EXPECT_EQ(pcs[0].last_message_id, 500u);
}

// =============================================================================
// Tier 2: Extensive edge cases - DmListModel
// =============================================================================

TEST(DmListModelTest, EmptyChannelsList) {
  kind::gui::DmListModel model;
  model.set_channels({});
  EXPECT_EQ(model.rowCount(), 0);
}

TEST(DmListModelTest, ChannelWithMultipleRecipients) {
  kind::Channel ch;
  ch.id = 1;
  ch.type = 1;
  ch.last_message_id = 10;

  kind::User user1;
  user1.id = 1001;
  user1.username = "first_recipient";
  ch.recipients.push_back(user1);

  kind::User user2;
  user2.id = 1002;
  user2.username = "second_recipient";
  ch.recipients.push_back(user2);

  kind::gui::DmListModel model;
  model.set_channels({ch});

  auto idx = model.index(0, 0);
  // Should use the first recipient
  EXPECT_EQ(idx.data(kind::gui::DmListModel::RecipientNameRole).toString().toStdString(), "first_recipient");
}

TEST(DmListModelTest, ChannelIdAtOutOfBoundsReturnsZero) {
  kind::gui::DmListModel model;
  model.set_channels({make_dm_channel(1, "alice", 10)});

  EXPECT_EQ(model.channel_id_at(-1), 0u);
  EXPECT_EQ(model.channel_id_at(1), 0u);
  EXPECT_EQ(model.channel_id_at(999), 0u);
}

TEST(DmListModelTest, InvalidIndexReturnsEmptyVariant) {
  kind::gui::DmListModel model;
  model.set_channels({make_dm_channel(1, "alice", 10)});

  auto bad_idx = model.index(5, 0);
  EXPECT_FALSE(bad_idx.data(Qt::DisplayRole).isValid());
}

// =============================================================================
// Tier 2: Extensive edge cases - Store private channels
// =============================================================================

TEST(StorePrivateChannelTest, UpdatePrivateChannelLastMessageDoesNotGoBackward) {
  kind::DataStore store;
  store.upsert_private_channel(make_dm_channel(1, "alice", 100));

  // Try to set a lower message id
  store.update_private_channel_last_message(1, 50);

  auto pcs = store.private_channels();
  ASSERT_EQ(pcs.size(), 1u);
  EXPECT_EQ(pcs[0].last_message_id, 100u); // stays at 100
}

TEST(StorePrivateChannelTest, UpsertPrivateChannelUpdatesExisting) {
  kind::DataStore store;
  store.upsert_private_channel(make_dm_channel(1, "alice", 100));

  // Upsert same channel id with different name
  store.upsert_private_channel(make_dm_channel(1, "alice_updated", 200));

  auto pcs = store.private_channels();
  ASSERT_EQ(pcs.size(), 1u); // not duplicated
  EXPECT_EQ(pcs[0].recipients[0].username, "alice_updated");
  EXPECT_EQ(pcs[0].last_message_id, 200u);
}

TEST(StorePrivateChannelTest, RemoveNonexistentChannelNoCrash) {
  kind::DataStore store;
  EXPECT_NO_THROW(store.remove_private_channel(999));
  EXPECT_TRUE(store.private_channels().empty());
}

TEST(StorePrivateChannelTest, UpdateLastMessageNonexistentChannelNoCrash) {
  kind::DataStore store;
  EXPECT_NO_THROW(store.update_private_channel_last_message(999, 100));
}

// =============================================================================
// Tier 2: Extensive edge cases - Channel parser
// =============================================================================

TEST(ChannelParseTest, ParsesLastMessageIdNull) {
  QJsonObject obj;
  obj["id"] = "12345";
  obj["type"] = 1;
  obj["name"] = "";
  obj["position"] = 0;
  obj["last_message_id"] = QJsonValue::Null;

  auto channel = kind::json_parse::parse_channel(obj);
  ASSERT_TRUE(channel.has_value());
  EXPECT_EQ(channel->last_message_id, 0u);
}

TEST(ChannelParseTest, ParsesEmptyRecipientsArray) {
  QJsonObject obj;
  obj["id"] = "12345";
  obj["type"] = 1;
  obj["name"] = "";
  obj["position"] = 0;
  obj["recipients"] = QJsonArray();

  auto channel = kind::json_parse::parse_channel(obj);
  ASSERT_TRUE(channel.has_value());
  EXPECT_TRUE(channel->recipients.empty());
}

TEST(ChannelParseTest, ParsesNoRecipientsField) {
  QJsonObject obj;
  obj["id"] = "12345";
  obj["type"] = 0;
  obj["name"] = "general";
  obj["position"] = 0;
  // No recipients field at all

  auto channel = kind::json_parse::parse_channel(obj);
  ASSERT_TRUE(channel.has_value());
  EXPECT_TRUE(channel->recipients.empty());
}

// =============================================================================
// Tier 3: Unhinged
// =============================================================================

TEST(DmListModelTest, ThousandDmChannels) {
  kind::gui::DmListModel model;
  std::vector<kind::Channel> channels;
  channels.reserve(1000);
  for (int i = 0; i < 1000; ++i) {
    channels.push_back(make_dm_channel(
        static_cast<kind::Snowflake>(i + 1),
        "user" + std::to_string(i),
        static_cast<kind::Snowflake>(i + 1)));
  }
  model.set_channels(channels);

  ASSERT_EQ(model.rowCount(), 1000);

  // Most recent should be first (last_message_id = 1000)
  auto first = model.index(0, 0);
  EXPECT_EQ(first.data(kind::gui::DmListModel::RecipientNameRole).toString().toStdString(), "user999");

  // Least recent should be last (last_message_id = 1)
  auto last = model.index(999, 0);
  EXPECT_EQ(last.data(kind::gui::DmListModel::RecipientNameRole).toString().toStdString(), "user0");
}

TEST(StorePrivateChannelTest, RapidPrivateChannelUpsertRemove) {
  kind::DataStore store;
  for (int i = 0; i < 1000; ++i) {
    store.upsert_private_channel(make_dm_channel(42, "ephemeral", static_cast<kind::Snowflake>(i)));
    store.remove_private_channel(42);
  }
  EXPECT_TRUE(store.private_channels().empty());
}

TEST(StorePrivateChannelTest, BulkUpsertReplacesAll) {
  kind::DataStore store;
  // First bulk upsert
  store.bulk_upsert_private_channels({
      make_dm_channel(1, "alice", 100),
      make_dm_channel(2, "bob", 200),
  });
  ASSERT_EQ(store.private_channels().size(), 2u);

  // Second bulk upsert completely replaces
  store.bulk_upsert_private_channels({
      make_dm_channel(3, "carol", 300),
  });
  auto pcs = store.private_channels();
  ASSERT_EQ(pcs.size(), 1u);
  EXPECT_EQ(pcs[0].id, 3u);
}

TEST(DmListModelTest, SetChannelsTriggersModelReset) {
  kind::gui::DmListModel model;
  model.set_channels({make_dm_channel(1, "alice", 100)});
  ASSERT_EQ(model.rowCount(), 1);

  // Replace with completely different set
  model.set_channels({
      make_dm_channel(2, "bob", 200),
      make_dm_channel(3, "carol", 300),
  });
  ASSERT_EQ(model.rowCount(), 2);
  EXPECT_EQ(model.channel_id_at(0), 3u); // carol has higher last_message_id
  EXPECT_EQ(model.channel_id_at(1), 2u);
}

TEST(DmListModelTest, ChannelIdRoleReturnsQulonglong) {
  kind::gui::DmListModel model;
  model.set_channels({make_dm_channel(42, "alice", 100)});

  auto idx = model.index(0, 0);
  auto val = idx.data(kind::gui::DmListModel::ChannelIdRole);
  EXPECT_TRUE(val.isValid());
  EXPECT_EQ(val.toULongLong(), 42u);
}
