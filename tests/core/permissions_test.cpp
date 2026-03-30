#include "permissions.hpp"

#include <gtest/gtest.h>

using namespace kind;

static Role make_role(Snowflake id, uint64_t perms, int position = 0) {
  return {id, "role", perms, position};
}

static PermissionOverwrite make_overwrite(Snowflake id, int type, uint64_t allow, uint64_t deny) {
  return {id, type, allow, deny};
}

static constexpr Snowflake guild_id = 100;
static constexpr Snowflake owner_id = 1;
static constexpr Snowflake user_id = 2;

static const std::vector<Role> base_roles = {
    make_role(guild_id, permission_bits::view_channel | permission_bits::send_messages),
};

TEST(Permissions, OwnerBypassesAll) {
  auto perms = compute_permissions(owner_id, guild_id, owner_id, base_roles, {}, {});
  EXPECT_EQ(perms, permission_bits::all);
}

TEST(Permissions, AdministratorBypassesAll) {
  std::vector<Role> roles = {
      make_role(guild_id, 0),
      make_role(10, permission_bits::administrator),
  };
  auto perms = compute_permissions(user_id, guild_id, owner_id, roles, {10}, {});
  EXPECT_EQ(perms, permission_bits::all);
}

TEST(Permissions, EveryoneBasePermissions) {
  auto perms = compute_permissions(user_id, guild_id, owner_id, base_roles, {}, {});
  EXPECT_TRUE(can_view_channel(perms));
  EXPECT_TRUE(can_send_messages(perms));
}

TEST(Permissions, RolePermissionsORed) {
  std::vector<Role> roles = {
      make_role(guild_id, permission_bits::view_channel),
      make_role(10, permission_bits::send_messages),
  };
  auto perms = compute_permissions(user_id, guild_id, owner_id, roles, {10}, {});
  EXPECT_TRUE(can_view_channel(perms));
  EXPECT_TRUE(can_send_messages(perms));
}

TEST(Permissions, ChannelOverwriteDenyRemovesBits) {
  std::vector<PermissionOverwrite> overwrites = {
      make_overwrite(guild_id, 0, 0, permission_bits::send_messages),
  };
  auto perms = compute_permissions(user_id, guild_id, owner_id, base_roles, {}, overwrites);
  EXPECT_TRUE(can_view_channel(perms));
  EXPECT_FALSE(can_send_messages(perms));
}

TEST(Permissions, ChannelOverwriteAllowAddsBits) {
  std::vector<Role> roles = {make_role(guild_id, 0)};
  std::vector<PermissionOverwrite> overwrites = {
      make_overwrite(guild_id, 0, permission_bits::view_channel, 0),
  };
  auto perms = compute_permissions(user_id, guild_id, owner_id, roles, {}, overwrites);
  EXPECT_TRUE(can_view_channel(perms));
}

TEST(Permissions, RoleOverwritesAccumulate) {
  std::vector<Role> roles = {
      make_role(guild_id, permission_bits::view_channel | permission_bits::send_messages),
      make_role(10, 0),
      make_role(11, 0),
  };
  std::vector<PermissionOverwrite> overwrites = {
      make_overwrite(10, 0, 0, permission_bits::send_messages),
      make_overwrite(11, 0, permission_bits::send_messages, 0),
  };
  auto perms = compute_permissions(user_id, guild_id, owner_id, roles, {10, 11}, overwrites);
  EXPECT_TRUE(can_send_messages(perms));
}

TEST(Permissions, UserOverwriteTakesPrecedence) {
  std::vector<PermissionOverwrite> overwrites = {
      make_overwrite(guild_id, 0, 0, permission_bits::view_channel),
      make_overwrite(user_id, 1, permission_bits::view_channel, 0),
  };
  auto perms = compute_permissions(user_id, guild_id, owner_id, base_roles, {}, overwrites);
  EXPECT_TRUE(can_view_channel(perms));
}

TEST(Permissions, ViewChannelFalseWhenDenied) {
  std::vector<Role> roles = {make_role(guild_id, 0)};
  auto perms = compute_permissions(user_id, guild_id, owner_id, roles, {}, {});
  EXPECT_FALSE(can_view_channel(perms));
}

TEST(Permissions, SendMessagesFalseWhenDenied) {
  std::vector<Role> roles = {make_role(guild_id, permission_bits::view_channel)};
  auto perms = compute_permissions(user_id, guild_id, owner_id, roles, {}, {});
  EXPECT_FALSE(can_send_messages(perms));
}
