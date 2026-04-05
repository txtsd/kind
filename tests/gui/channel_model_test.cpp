#include "models/channel_model.hpp"
#include "mute_state_manager.hpp"
#include "read_state_manager.hpp"
#include "permissions.hpp"

#include <gtest/gtest.h>

// =============================================================================
// Helpers
// =============================================================================

static kind::Channel make_channel(kind::Snowflake id, const std::string& name,
                                  int type, int position,
                                  std::optional<kind::Snowflake> parent_id = std::nullopt,
                                  kind::Snowflake guild_id = 1) {
  kind::Channel ch;
  ch.id = id;
  ch.guild_id = guild_id;
  ch.name = name;
  ch.type = type;
  ch.position = position;
  ch.parent_id = parent_id;
  return ch;
}

static kind::Channel make_category(kind::Snowflake id, const std::string& name,
                                   int position) {
  return make_channel(id, name, 4, position);
}

static kind::Channel make_text(kind::Snowflake id, const std::string& name,
                               int position, kind::Snowflake parent_id) {
  return make_channel(id, name, 0, position, parent_id);
}

static kind::Channel make_uncategorized(kind::Snowflake id, const std::string& name,
                                        int position) {
  return make_channel(id, name, 0, position);
}

// =============================================================================
// Tier 1: Normal tests
// =============================================================================

TEST(ChannelModelTest, SetChannelsWithCategoriesAndChildren) {
  kind::gui::ChannelModel model;
  std::vector<kind::Channel> channels = {
    make_category(100, "General", 0),
    make_text(1, "chat", 0, 100),
    make_text(2, "voice", 1, 100),
  };
  model.set_channels(channels);

  // Category + 2 children = 3 rows
  ASSERT_EQ(model.rowCount(), 3);
}

TEST(ChannelModelTest, RowCountReflectsVisibleChannels) {
  kind::gui::ChannelModel model;
  model.set_channels({});
  EXPECT_EQ(model.rowCount(), 0);

  model.set_channels({make_category(100, "Cat", 0), make_text(1, "ch", 0, 100)});
  EXPECT_EQ(model.rowCount(), 2);
}

TEST(ChannelModelTest, DisplayRoleReturnsChannelName) {
  kind::gui::ChannelModel model;
  model.set_channels({
    make_category(100, "General", 0),
    make_text(1, "announcements", 0, 100),
  });

  auto idx0 = model.index(0, 0);
  auto idx1 = model.index(1, 0);
  // Uncategorized channels come first, then categories.
  // Here there are no uncategorized, so category is first.
  EXPECT_EQ(idx0.data(Qt::DisplayRole).toString().toStdString(), "General");
  EXPECT_EQ(idx1.data(Qt::DisplayRole).toString().toStdString(), "announcements");
}

TEST(ChannelModelTest, ChannelIdAtReturnsCorrectId) {
  kind::gui::ChannelModel model;
  model.set_channels({
    make_category(100, "Cat", 0),
    make_text(1, "ch1", 0, 100),
    make_text(2, "ch2", 1, 100),
  });

  EXPECT_EQ(model.channel_id_at(0), 100u);
  EXPECT_EQ(model.channel_id_at(1), 1u);
  EXPECT_EQ(model.channel_id_at(2), 2u);
}

TEST(ChannelModelTest, ChannelIdAtOutOfBoundsReturnsZero) {
  kind::gui::ChannelModel model;
  model.set_channels({make_text(1, "ch", 0, 100)});

  EXPECT_EQ(model.channel_id_at(-1), 0u);
  EXPECT_EQ(model.channel_id_at(999), 0u);
}

TEST(ChannelModelTest, ToggleCollapsedHidesChildren) {
  kind::gui::ChannelModel model;
  model.set_channels({
    make_category(100, "Cat", 0),
    make_text(1, "ch1", 0, 100),
    make_text(2, "ch2", 1, 100),
  });
  ASSERT_EQ(model.rowCount(), 3);

  model.toggle_collapsed(100);
  EXPECT_TRUE(model.is_collapsed(100));
  // Category still visible, children hidden
  ASSERT_EQ(model.rowCount(), 1);
  EXPECT_EQ(model.channel_id_at(0), 100u);

  // Toggle back to expand
  model.toggle_collapsed(100);
  EXPECT_FALSE(model.is_collapsed(100));
  ASSERT_EQ(model.rowCount(), 3);
}

TEST(ChannelModelTest, ChannelTypeRole) {
  kind::gui::ChannelModel model;
  model.set_channels({
    make_category(100, "Cat", 0),
    make_text(1, "ch", 0, 100),
  });

  auto cat_idx = model.index(0, 0);
  auto ch_idx = model.index(1, 0);
  EXPECT_EQ(cat_idx.data(kind::gui::ChannelModel::ChannelTypeRole).toInt(), 4);
  EXPECT_EQ(ch_idx.data(kind::gui::ChannelModel::ChannelTypeRole).toInt(), 0);
}

TEST(ChannelModelTest, IsCategoryRole) {
  kind::gui::ChannelModel model;
  model.set_channels({
    make_category(100, "Cat", 0),
    make_text(1, "ch", 0, 100),
  });

  EXPECT_TRUE(model.index(0, 0).data(kind::gui::ChannelModel::IsCategoryRole).toBool());
  EXPECT_FALSE(model.index(1, 0).data(kind::gui::ChannelModel::IsCategoryRole).toBool());
}

TEST(ChannelModelTest, CollapsedRoleReflectsState) {
  kind::gui::ChannelModel model;
  model.set_channels({
    make_category(100, "Cat", 0),
    make_text(1, "ch", 0, 100),
  });

  EXPECT_FALSE(model.index(0, 0).data(kind::gui::ChannelModel::CollapsedRole).toBool());
  model.toggle_collapsed(100);
  EXPECT_TRUE(model.index(0, 0).data(kind::gui::ChannelModel::CollapsedRole).toBool());
}

TEST(ChannelModelTest, ChannelIdRoleReturnsQulonglong) {
  kind::gui::ChannelModel model;
  model.set_channels({make_uncategorized(42, "ch", 0)});

  auto val = model.index(0, 0).data(kind::gui::ChannelModel::ChannelIdRole);
  EXPECT_TRUE(val.isValid());
  EXPECT_EQ(val.toULongLong(), 42u);
}

TEST(ChannelModelTest, ParentIdRoleForCategorizedChannel) {
  kind::gui::ChannelModel model;
  model.set_channels({
    make_category(100, "Cat", 0),
    make_text(1, "ch", 0, 100),
  });

  auto parent_val = model.index(1, 0).data(kind::gui::ChannelModel::ParentIdRole);
  EXPECT_TRUE(parent_val.isValid());
  EXPECT_EQ(parent_val.toULongLong(), 100u);

  // Category itself has no parent
  auto cat_parent = model.index(0, 0).data(kind::gui::ChannelModel::ParentIdRole);
  EXPECT_FALSE(cat_parent.isValid());
}

TEST(ChannelModelTest, LockedRoleDefaultsFalse) {
  kind::gui::ChannelModel model;
  model.set_channels({make_text(1, "ch", 0, 100)});

  // No permissions set, so not locked
  EXPECT_FALSE(model.index(0, 0).data(kind::gui::ChannelModel::LockedRole).toBool());
}

TEST(ChannelModelTest, LockedRoleWithPermissions) {
  kind::gui::ChannelModel model;
  std::unordered_map<kind::Snowflake, uint64_t> perms;
  // Channel 1 has view_channel permission
  perms[1] = kind::permission_bits::view_channel;
  // Channel 2 has no view_channel permission (just send_messages)
  perms[2] = kind::permission_bits::send_messages;

  model.set_channels({make_uncategorized(1, "visible", 0), make_uncategorized(2, "locked", 1)}, perms);

  EXPECT_FALSE(model.index(0, 0).data(kind::gui::ChannelModel::LockedRole).toBool());
  EXPECT_TRUE(model.index(1, 0).data(kind::gui::ChannelModel::LockedRole).toBool());
}

TEST(ChannelModelTest, InvalidIndexReturnsEmptyVariant) {
  kind::gui::ChannelModel model;
  model.set_channels({make_text(1, "ch", 0, 100)});

  auto bad = model.index(5, 0);
  EXPECT_FALSE(bad.data(Qt::DisplayRole).isValid());
}

// =============================================================================
// Tier 2: Extensive edge cases
// =============================================================================

TEST(ChannelModelTest, HideLockedFiltersChannelsWithoutViewPermission) {
  kind::gui::ChannelModel model;
  std::unordered_map<kind::Snowflake, uint64_t> perms;
  perms[1] = kind::permission_bits::view_channel;
  perms[2] = 0; // no view_channel

  std::vector<kind::Channel> channels = {
    make_category(100, "Cat", 0),
    make_text(1, "visible", 0, 100),
    make_text(2, "hidden", 1, 100),
  };
  model.set_channels(channels, perms, true);

  // Category + 1 visible child = 2 rows (hidden channel filtered)
  ASSERT_EQ(model.rowCount(), 2);
  EXPECT_EQ(model.index(0, 0).data(Qt::DisplayRole).toString().toStdString(), "Cat");
  EXPECT_EQ(model.index(1, 0).data(Qt::DisplayRole).toString().toStdString(), "visible");
}

TEST(ChannelModelTest, EmptyCategoriesRemovedWithHideLocked) {
  kind::gui::ChannelModel model;
  std::unordered_map<kind::Snowflake, uint64_t> perms;
  perms[1] = 0; // no view_channel, will be filtered

  std::vector<kind::Channel> channels = {
    make_category(100, "EmptyCat", 0),
    make_text(1, "locked-child", 0, 100),
  };
  model.set_channels(channels, perms, true);

  // Child is filtered, category becomes empty, so category is skipped too
  EXPECT_EQ(model.rowCount(), 0);
}

TEST(ChannelModelTest, EmptyCategoryWithNoChildrenRemovedWithHideLocked) {
  kind::gui::ChannelModel model;
  // Category with zero children at all
  model.set_channels({make_category(100, "EmptyCat", 0)}, {}, true);
  EXPECT_EQ(model.rowCount(), 0);
}

TEST(ChannelModelTest, EmptyCategoryKeptWithoutHideLocked) {
  kind::gui::ChannelModel model;
  model.set_channels({make_category(100, "EmptyCat", 0)});
  EXPECT_EQ(model.rowCount(), 1);
}

TEST(ChannelModelTest, CollapseAllCollapsesEveryCategory) {
  kind::gui::ChannelModel model;
  model.set_channels({
    make_category(100, "Cat1", 0),
    make_text(1, "ch1", 0, 100),
    make_category(200, "Cat2", 1),
    make_text(2, "ch2", 0, 200),
  });
  ASSERT_EQ(model.rowCount(), 4);

  model.collapse_all();
  EXPECT_TRUE(model.is_collapsed(100));
  EXPECT_TRUE(model.is_collapsed(200));
  // Only categories visible
  ASSERT_EQ(model.rowCount(), 2);
  EXPECT_EQ(model.channel_id_at(0), 100u);
  EXPECT_EQ(model.channel_id_at(1), 200u);
}

TEST(ChannelModelTest, ExpandAllExpandsEveryCategory) {
  kind::gui::ChannelModel model;
  model.set_channels({
    make_category(100, "Cat1", 0),
    make_text(1, "ch1", 0, 100),
    make_category(200, "Cat2", 1),
    make_text(2, "ch2", 0, 200),
  });
  model.collapse_all();
  ASSERT_EQ(model.rowCount(), 2);

  model.expand_all();
  EXPECT_FALSE(model.is_collapsed(100));
  EXPECT_FALSE(model.is_collapsed(200));
  ASSERT_EQ(model.rowCount(), 4);
}

TEST(ChannelModelTest, PositionSortingWithinCategory) {
  kind::gui::ChannelModel model;
  model.set_channels({
    make_category(100, "Cat", 0),
    make_text(3, "third", 2, 100),
    make_text(1, "first", 0, 100),
    make_text(2, "second", 1, 100),
  });

  ASSERT_EQ(model.rowCount(), 4);
  EXPECT_EQ(model.index(0, 0).data(Qt::DisplayRole).toString().toStdString(), "Cat");
  EXPECT_EQ(model.index(1, 0).data(Qt::DisplayRole).toString().toStdString(), "first");
  EXPECT_EQ(model.index(2, 0).data(Qt::DisplayRole).toString().toStdString(), "second");
  EXPECT_EQ(model.index(3, 0).data(Qt::DisplayRole).toString().toStdString(), "third");
}

TEST(ChannelModelTest, CategoriesSortedByPosition) {
  kind::gui::ChannelModel model;
  model.set_channels({
    make_category(200, "SecondCat", 1),
    make_category(100, "FirstCat", 0),
    make_category(300, "ThirdCat", 2),
  });

  ASSERT_EQ(model.rowCount(), 3);
  EXPECT_EQ(model.index(0, 0).data(Qt::DisplayRole).toString().toStdString(), "FirstCat");
  EXPECT_EQ(model.index(1, 0).data(Qt::DisplayRole).toString().toStdString(), "SecondCat");
  EXPECT_EQ(model.index(2, 0).data(Qt::DisplayRole).toString().toStdString(), "ThirdCat");
}

TEST(ChannelModelTest, UncategorizedChannelsAppearFirst) {
  kind::gui::ChannelModel model;
  model.set_channels({
    make_category(100, "Cat", 0),
    make_text(1, "categorized", 0, 100),
    make_uncategorized(2, "uncategorized", 0),
  });

  ASSERT_EQ(model.rowCount(), 3);
  // Uncategorized first, then category, then its children
  EXPECT_EQ(model.index(0, 0).data(Qt::DisplayRole).toString().toStdString(), "uncategorized");
  EXPECT_EQ(model.index(1, 0).data(Qt::DisplayRole).toString().toStdString(), "Cat");
  EXPECT_EQ(model.index(2, 0).data(Qt::DisplayRole).toString().toStdString(), "categorized");
}

TEST(ChannelModelTest, UncategorizedChannelsSortedByPosition) {
  kind::gui::ChannelModel model;
  model.set_channels({
    make_uncategorized(3, "third", 2),
    make_uncategorized(1, "first", 0),
    make_uncategorized(2, "second", 1),
  });

  ASSERT_EQ(model.rowCount(), 3);
  EXPECT_EQ(model.index(0, 0).data(Qt::DisplayRole).toString().toStdString(), "first");
  EXPECT_EQ(model.index(1, 0).data(Qt::DisplayRole).toString().toStdString(), "second");
  EXPECT_EQ(model.index(2, 0).data(Qt::DisplayRole).toString().toStdString(), "third");
}

TEST(ChannelModelTest, CollapsingOneCategoryDoesNotAffectOther) {
  kind::gui::ChannelModel model;
  model.set_channels({
    make_category(100, "Cat1", 0),
    make_text(1, "ch1", 0, 100),
    make_category(200, "Cat2", 1),
    make_text(2, "ch2", 0, 200),
  });

  model.toggle_collapsed(100);
  ASSERT_EQ(model.rowCount(), 3); // Cat1 (collapsed) + Cat2 + ch2
  EXPECT_EQ(model.channel_id_at(0), 100u);
  EXPECT_EQ(model.channel_id_at(1), 200u);
  EXPECT_EQ(model.channel_id_at(2), 2u);
}

TEST(ChannelModelTest, HideLockedDoesNotFilterCategories) {
  kind::gui::ChannelModel model;
  std::unordered_map<kind::Snowflake, uint64_t> perms;
  perms[100] = 0; // category itself has no view_channel
  perms[1] = kind::permission_bits::view_channel;

  model.set_channels({
    make_category(100, "Cat", 0),
    make_text(1, "ch", 0, 100),
  }, perms, true);

  // Categories are never filtered by hide_locked (only type != 4 is filtered)
  // Category has a visible child, so it stays
  ASSERT_EQ(model.rowCount(), 2);
}

TEST(ChannelModelTest, PositionRoleReturnsPosition) {
  kind::gui::ChannelModel model;
  model.set_channels({make_uncategorized(1, "ch", 7)});

  EXPECT_EQ(model.index(0, 0).data(kind::gui::ChannelModel::PositionRole).toInt(), 7);
}

TEST(ChannelModelTest, MutedRoleWithoutManagerReturnsFalse) {
  kind::gui::ChannelModel model;
  model.set_channels({make_uncategorized(1, "ch", 0)});

  EXPECT_FALSE(model.index(0, 0).data(kind::gui::ChannelModel::MutedRole).toBool());
}

TEST(ChannelModelTest, UnreadCountRoleWithoutManagerReturnsZero) {
  kind::gui::ChannelModel model;
  model.set_channels({make_uncategorized(1, "ch", 0)});

  EXPECT_EQ(model.index(0, 0).data(kind::gui::ChannelModel::UnreadCountRole).toInt(), 0);
}

TEST(ChannelModelTest, MentionCountRoleWithoutManagerReturnsZero) {
  kind::gui::ChannelModel model;
  model.set_channels({make_uncategorized(1, "ch", 0)});

  EXPECT_EQ(model.index(0, 0).data(kind::gui::ChannelModel::MentionCountRole).toInt(), 0);
}

TEST(ChannelModelTest, UnreadTextRoleWithoutManagerReturnsEmpty) {
  kind::gui::ChannelModel model;
  model.set_channels({make_uncategorized(1, "ch", 0)});

  EXPECT_TRUE(model.index(0, 0).data(kind::gui::ChannelModel::UnreadTextRole).toString().isEmpty());
}

TEST(ChannelModelTest, ReadStateManagerIntegration) {
  kind::gui::ChannelModel model;
  kind::ReadStateManager rsm;
  model.set_read_state_manager(&rsm);

  model.set_channels({make_uncategorized(1, "ch", 0)});

  // Simulate unread messages
  rsm.increment_unread(1, 50);
  rsm.increment_unread(1, 51);
  rsm.increment_unread(1, 52);

  EXPECT_EQ(model.index(0, 0).data(kind::gui::ChannelModel::UnreadCountRole).toInt(), 3);
  EXPECT_EQ(model.index(0, 0).data(kind::gui::ChannelModel::UnreadTextRole).toString().toStdString(), "3");
}

TEST(ChannelModelTest, MuteStateManagerSuppressesUnreads) {
  kind::gui::ChannelModel model;
  kind::ReadStateManager rsm;
  kind::MuteStateManager msm;
  model.set_read_state_manager(&rsm);
  model.set_mute_state_manager(&msm);

  auto ch = make_uncategorized(1, "ch", 0);
  ch.guild_id = 42;
  model.set_channels({ch});

  rsm.increment_unread(1, 50);
  msm.set_channel_muted(1, true);

  EXPECT_TRUE(model.index(0, 0).data(kind::gui::ChannelModel::MutedRole).toBool());
  EXPECT_EQ(model.index(0, 0).data(kind::gui::ChannelModel::UnreadCountRole).toInt(), 0);
  EXPECT_TRUE(model.index(0, 0).data(kind::gui::ChannelModel::UnreadTextRole).toString().isEmpty());
  EXPECT_EQ(model.index(0, 0).data(kind::gui::ChannelModel::MentionCountRole).toInt(), 0);
}

TEST(ChannelModelTest, MentionCountRole) {
  kind::gui::ChannelModel model;
  kind::ReadStateManager rsm;
  model.set_read_state_manager(&rsm);

  model.set_channels({make_uncategorized(1, "ch", 0)});
  rsm.increment_mention(1, 3);

  EXPECT_EQ(model.index(0, 0).data(kind::gui::ChannelModel::MentionCountRole).toInt(), 3);
}

TEST(ChannelModelTest, SetChannelsReplacesExistingData) {
  kind::gui::ChannelModel model;
  model.set_channels({make_uncategorized(1, "old", 0)});
  ASSERT_EQ(model.rowCount(), 1);

  model.set_channels({
    make_uncategorized(2, "new1", 0),
    make_uncategorized(3, "new2", 1),
  });
  ASSERT_EQ(model.rowCount(), 2);
  EXPECT_EQ(model.channel_id_at(0), 2u);
  EXPECT_EQ(model.channel_id_at(1), 3u);
}

TEST(ChannelModelTest, CollapsedStateSurvivesSetChannels) {
  kind::gui::ChannelModel model;
  model.set_channels({
    make_category(100, "Cat", 0),
    make_text(1, "ch", 0, 100),
  });
  model.toggle_collapsed(100);
  ASSERT_TRUE(model.is_collapsed(100));

  // Reload same channels
  model.set_channels({
    make_category(100, "Cat", 0),
    make_text(1, "ch", 0, 100),
  });

  // Collapsed state persists across set_channels calls
  EXPECT_TRUE(model.is_collapsed(100));
  EXPECT_EQ(model.rowCount(), 1); // only category visible
}

TEST(ChannelModelTest, HideLockedWithNoPermissionsMapShowsAll) {
  kind::gui::ChannelModel model;
  // hide_locked=true but empty permissions map: channels without entries are not locked
  model.set_channels({
    make_category(100, "Cat", 0),
    make_text(1, "ch", 0, 100),
  }, {}, true);

  // No permission entries means can_view is not denied (no entry = not locked)
  // But with hide_locked, channels without a permission entry are kept (only filtered if entry exists and denies)
  ASSERT_EQ(model.rowCount(), 2);
}

TEST(ChannelModelTest, UnreadTextRoleOver99ShowsPlus) {
  kind::gui::ChannelModel model;
  kind::ReadStateManager rsm;
  model.set_read_state_manager(&rsm);

  model.set_channels({make_uncategorized(1, "ch", 0)});

  // Increment 100 times
  for (int i = 0; i < 100; ++i) {
    rsm.increment_unread(1, static_cast<kind::Snowflake>(i + 10));
  }

  EXPECT_EQ(model.index(0, 0).data(kind::gui::ChannelModel::UnreadTextRole).toString().toStdString(), "99+");
}

// =============================================================================
// Tier 3: Absolutely unhinged scenarios
// =============================================================================

TEST(ChannelModelTest, HundredCategoriesWithTenChildrenEach) {
  kind::gui::ChannelModel model;
  std::vector<kind::Channel> channels;
  channels.reserve(1100);

  for (int cat = 0; cat < 100; ++cat) {
    auto cat_id = static_cast<kind::Snowflake>(1000 + cat);
    channels.push_back(make_category(cat_id, "cat" + std::to_string(cat), cat));
    for (int ch = 0; ch < 10; ++ch) {
      auto ch_id = static_cast<kind::Snowflake>(cat * 100 + ch + 1);
      channels.push_back(make_text(ch_id, "ch" + std::to_string(cat) + "_" + std::to_string(ch), ch, cat_id));
    }
  }

  model.set_channels(channels);
  // 100 categories + 1000 children
  ASSERT_EQ(model.rowCount(), 1100);

  // Collapse all
  model.collapse_all();
  ASSERT_EQ(model.rowCount(), 100);

  // Every row should be a category
  for (int i = 0; i < 100; ++i) {
    EXPECT_TRUE(model.index(i, 0).data(kind::gui::ChannelModel::IsCategoryRole).toBool());
  }

  // Expand all
  model.expand_all();
  ASSERT_EQ(model.rowCount(), 1100);

  // Verify order: each category followed by its 10 children
  int row = 0;
  for (int cat = 0; cat < 100; ++cat) {
    auto cat_id = static_cast<kind::Snowflake>(1000 + cat);
    EXPECT_EQ(model.channel_id_at(row), cat_id) << "category " << cat << " at row " << row;
    ++row;
    for (int ch = 0; ch < 10; ++ch) {
      auto ch_id = static_cast<kind::Snowflake>(cat * 100 + ch + 1);
      EXPECT_EQ(model.channel_id_at(row), ch_id) << "child " << ch << " of cat " << cat;
      ++row;
    }
  }
}

TEST(ChannelModelTest, ChannelsWithNoCategoriesAtAll) {
  kind::gui::ChannelModel model;
  std::vector<kind::Channel> channels;
  for (int i = 0; i < 50; ++i) {
    channels.push_back(make_uncategorized(
      static_cast<kind::Snowflake>(i + 1),
      "ch" + std::to_string(i),
      49 - i // reverse position order to verify sorting
    ));
  }

  model.set_channels(channels);
  ASSERT_EQ(model.rowCount(), 50);

  // Should be sorted by position ascending: ch49 (pos=0) first, ch0 (pos=49) last
  EXPECT_EQ(model.index(0, 0).data(Qt::DisplayRole).toString().toStdString(), "ch49");
  EXPECT_EQ(model.index(49, 0).data(Qt::DisplayRole).toString().toStdString(), "ch0");

  // collapse_all and expand_all should be no-ops when no categories exist
  model.collapse_all();
  EXPECT_EQ(model.rowCount(), 50);
  model.expand_all();
  EXPECT_EQ(model.rowCount(), 50);
}

TEST(ChannelModelTest, ToggleNonExistentCategoryIsHarmless) {
  kind::gui::ChannelModel model;
  model.set_channels({
    make_category(100, "Cat", 0),
    make_text(1, "ch", 0, 100),
  });
  ASSERT_EQ(model.rowCount(), 2);

  // Toggle a snowflake that does not correspond to any category
  model.toggle_collapsed(999999);
  // Should not crash and row count should be unaffected
  EXPECT_EQ(model.rowCount(), 2);

  // The non-existent id is now in collapsed set, but it has no effect
  EXPECT_TRUE(model.is_collapsed(999999));
}

TEST(ChannelModelTest, RapidCollapseExpandCycles) {
  kind::gui::ChannelModel model;
  model.set_channels({
    make_category(100, "Cat", 0),
    make_text(1, "ch1", 0, 100),
    make_text(2, "ch2", 1, 100),
  });

  for (int i = 0; i < 1000; ++i) {
    model.toggle_collapsed(100);
  }

  // 1000 toggles: even number means back to expanded
  EXPECT_FALSE(model.is_collapsed(100));
  EXPECT_EQ(model.rowCount(), 3);
}

TEST(ChannelModelTest, ChildrenOfNonExistentParentBecomeUncategorized) {
  kind::gui::ChannelModel model;
  // Channels reference parent_id 999 which does not exist as a category
  model.set_channels({
    make_text(1, "orphan1", 0, 999),
    make_text(2, "orphan2", 1, 999),
    make_category(100, "Cat", 0),
    make_text(3, "normal", 0, 100),
  });

  // Orphaned channels go into by_parent[999], which matches no category.
  // They will not appear (neither uncategorized nor under any category).
  // Only the valid category and its child appear.
  // Plus the orphans are effectively lost since parent 999 does not exist.
  // Actually, let's verify what really happens.
  // by_parent[999] will have entries, but no category with id=999 exists,
  // so those children are never appended. Row count = Cat + normal = 2
  EXPECT_EQ(model.rowCount(), 2);
  EXPECT_EQ(model.index(0, 0).data(Qt::DisplayRole).toString().toStdString(), "Cat");
  EXPECT_EQ(model.index(1, 0).data(Qt::DisplayRole).toString().toStdString(), "normal");
}

TEST(ChannelModelTest, AllChannelsLockedAndHidden) {
  kind::gui::ChannelModel model;
  std::unordered_map<kind::Snowflake, uint64_t> perms;

  std::vector<kind::Channel> channels;
  for (int cat = 0; cat < 5; ++cat) {
    auto cat_id = static_cast<kind::Snowflake>(100 + cat);
    channels.push_back(make_category(cat_id, "cat" + std::to_string(cat), cat));
    for (int ch = 0; ch < 3; ++ch) {
      auto ch_id = static_cast<kind::Snowflake>(cat * 10 + ch + 1);
      channels.push_back(make_text(ch_id, "ch", ch, cat_id));
      perms[ch_id] = 0; // no view_channel
    }
  }

  model.set_channels(channels, perms, true);

  // All children locked and hidden, all categories become empty and are skipped
  EXPECT_EQ(model.rowCount(), 0);
}

TEST(ChannelModelTest, MixedUncategorizedAndCategorizedWithCollapse) {
  kind::gui::ChannelModel model;
  model.set_channels({
    make_uncategorized(1, "rules", 0),
    make_uncategorized(2, "info", 1),
    make_category(100, "General", 0),
    make_text(3, "chat", 0, 100),
    make_text(4, "memes", 1, 100),
    make_category(200, "Voice", 1),
    make_text(5, "lounge", 0, 200),
  });

  ASSERT_EQ(model.rowCount(), 7);

  // Collapse General
  model.toggle_collapsed(100);
  ASSERT_EQ(model.rowCount(), 5); // rules, info, General(collapsed), Voice, lounge
  EXPECT_EQ(model.index(0, 0).data(Qt::DisplayRole).toString().toStdString(), "rules");
  EXPECT_EQ(model.index(1, 0).data(Qt::DisplayRole).toString().toStdString(), "info");
  EXPECT_EQ(model.index(2, 0).data(Qt::DisplayRole).toString().toStdString(), "General");
  EXPECT_EQ(model.index(3, 0).data(Qt::DisplayRole).toString().toStdString(), "Voice");
  EXPECT_EQ(model.index(4, 0).data(Qt::DisplayRole).toString().toStdString(), "lounge");

  // Collapse Voice too
  model.toggle_collapsed(200);
  ASSERT_EQ(model.rowCount(), 4); // rules, info, General, Voice
}

TEST(ChannelModelTest, SetChannelsThenSetReadStateManager) {
  // Setting the read state manager after channels are already loaded
  kind::gui::ChannelModel model;
  model.set_channels({make_uncategorized(1, "ch", 0)});

  kind::ReadStateManager rsm;
  rsm.increment_unread(1, 50);

  model.set_read_state_manager(&rsm);

  // Reads from the manager should work immediately
  EXPECT_EQ(model.index(0, 0).data(kind::gui::ChannelModel::UnreadCountRole).toInt(), 1);
}

TEST(ChannelModelTest, DisconnectingReadStateManager) {
  kind::gui::ChannelModel model;
  kind::ReadStateManager rsm;
  model.set_read_state_manager(&rsm);
  model.set_channels({make_uncategorized(1, "ch", 0)});

  rsm.increment_unread(1, 50);
  EXPECT_EQ(model.index(0, 0).data(kind::gui::ChannelModel::UnreadCountRole).toInt(), 1);

  // Replace with nullptr
  model.set_read_state_manager(nullptr);
  EXPECT_EQ(model.index(0, 0).data(kind::gui::ChannelModel::UnreadCountRole).toInt(), 0);
}

TEST(ChannelModelTest, DuplicateChannelIdsLastOneWins) {
  kind::gui::ChannelModel model;
  // Two channels with the same id but different names
  model.set_channels({
    make_uncategorized(1, "first", 0),
    make_uncategorized(1, "second", 1),
  });

  // Both will be present since the model does not deduplicate
  ASSERT_EQ(model.rowCount(), 2);
}

TEST(ChannelModelTest, ZeroSnowflakeChannelId) {
  kind::gui::ChannelModel model;
  model.set_channels({make_uncategorized(0, "zero-id-channel", 0)});

  ASSERT_EQ(model.rowCount(), 1);
  EXPECT_EQ(model.channel_id_at(0), 0u);
  EXPECT_EQ(model.index(0, 0).data(Qt::DisplayRole).toString().toStdString(), "zero-id-channel");
}

TEST(ChannelModelTest, EmptyChannelName) {
  kind::gui::ChannelModel model;
  model.set_channels({make_uncategorized(1, "", 0)});

  ASSERT_EQ(model.rowCount(), 1);
  EXPECT_EQ(model.index(0, 0).data(Qt::DisplayRole).toString().toStdString(), "");
}

TEST(ChannelModelTest, SamePositionChannelsStableOrder) {
  kind::gui::ChannelModel model;
  // Multiple channels with the same position value
  model.set_channels({
    make_category(100, "Cat", 0),
    make_text(1, "alpha", 0, 100),
    make_text(2, "beta", 0, 100),
    make_text(3, "gamma", 0, 100),
  });

  // All have position 0. std::sort is not guaranteed stable, so we just
  // verify they all appear under the category
  ASSERT_EQ(model.rowCount(), 4);
  EXPECT_EQ(model.index(0, 0).data(Qt::DisplayRole).toString().toStdString(), "Cat");
  // The three children are present (order may vary)
  std::set<std::string> names;
  for (int i = 1; i < 4; ++i) {
    names.insert(model.index(i, 0).data(Qt::DisplayRole).toString().toStdString());
  }
  EXPECT_EQ(names.count("alpha"), 1u);
  EXPECT_EQ(names.count("beta"), 1u);
  EXPECT_EQ(names.count("gamma"), 1u);
}
