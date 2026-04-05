#include "permissions.hpp"

#include <gtest/gtest.h>

#include <numeric>

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

// ---------------------------------------------------------------------------
// Tier 2: Extensive edge cases
// ---------------------------------------------------------------------------

TEST(PermissionsExtended, MultipleRoleOverwritesDenyThenAllow) {
  // Role A denies send, Role B allows send. Allow should win (OR of allows).
  std::vector<Role> roles = {
      make_role(guild_id, permission_bits::view_channel | permission_bits::send_messages),
      make_role(10, 0, 1),
      make_role(11, 0, 2),
  };
  std::vector<PermissionOverwrite> overwrites = {
      make_overwrite(10, 0, 0, permission_bits::send_messages),
      make_overwrite(11, 0, permission_bits::send_messages, 0),
  };
  auto perms = compute_permissions(user_id, guild_id, owner_id, roles, {10, 11}, overwrites);
  EXPECT_TRUE(can_send_messages(perms));
}

TEST(PermissionsExtended, ZeroBasePermissionsNoRoles) {
  // Everyone role has zero permissions, user has no additional roles
  std::vector<Role> roles = {make_role(guild_id, 0)};
  auto perms = compute_permissions(user_id, guild_id, owner_id, roles, {}, {});
  EXPECT_EQ(perms, 0ULL);
  EXPECT_FALSE(can_view_channel(perms));
  EXPECT_FALSE(can_send_messages(perms));
}

TEST(PermissionsExtended, AllPermissionsBitsSet) {
  std::vector<Role> roles = {make_role(guild_id, permission_bits::all)};
  auto perms = compute_permissions(user_id, guild_id, owner_id, roles, {}, {});
  EXPECT_EQ(perms, permission_bits::all);
}

TEST(PermissionsExtended, UserOverwriteDenyOverridesRoleAllow) {
  // Role overwrite allows send, but user overwrite denies it
  std::vector<Role> roles = {
      make_role(guild_id, permission_bits::view_channel),
      make_role(10, 0),
  };
  std::vector<PermissionOverwrite> overwrites = {
      make_overwrite(10, 0, permission_bits::send_messages, 0),   // role allow
      make_overwrite(user_id, 1, 0, permission_bits::send_messages),  // user deny
  };
  auto perms = compute_permissions(user_id, guild_id, owner_id, roles, {10}, overwrites);
  EXPECT_FALSE(can_send_messages(perms));
}

TEST(PermissionsExtended, UserOverwriteAllowOverridesRoleDeny) {
  std::vector<Role> roles = {
      make_role(guild_id, permission_bits::view_channel | permission_bits::send_messages),
      make_role(10, 0),
  };
  std::vector<PermissionOverwrite> overwrites = {
      make_overwrite(10, 0, 0, permission_bits::send_messages),   // role deny
      make_overwrite(user_id, 1, permission_bits::send_messages, 0),  // user allow
  };
  auto perms = compute_permissions(user_id, guild_id, owner_id, roles, {10}, overwrites);
  EXPECT_TRUE(can_send_messages(perms));
}

TEST(PermissionsExtended, EveryoneOverwriteDeny) {
  // @everyone channel overwrite denies view, user has no role overwrites
  std::vector<PermissionOverwrite> overwrites = {
      make_overwrite(guild_id, 0, 0, permission_bits::view_channel),
  };
  std::vector<Role> roles = {
      make_role(guild_id, permission_bits::view_channel | permission_bits::send_messages),
  };
  auto perms = compute_permissions(user_id, guild_id, owner_id, roles, {}, overwrites);
  EXPECT_FALSE(can_view_channel(perms));
}

TEST(PermissionsExtended, AdminIgnoresChannelOverwrites) {
  // Admin should bypass all channel overwrites
  std::vector<Role> roles = {
      make_role(guild_id, 0),
      make_role(10, permission_bits::administrator),
  };
  std::vector<PermissionOverwrite> overwrites = {
      make_overwrite(guild_id, 0, 0, permission_bits::view_channel | permission_bits::send_messages),
  };
  auto perms = compute_permissions(user_id, guild_id, owner_id, roles, {10}, overwrites);
  EXPECT_EQ(perms, permission_bits::all);
}

TEST(PermissionsExtended, OwnerIgnoresChannelOverwrites) {
  std::vector<PermissionOverwrite> overwrites = {
      make_overwrite(owner_id, 1, 0, permission_bits::all),
  };
  std::vector<Role> roles = {make_role(guild_id, 0)};
  auto perms = compute_permissions(owner_id, guild_id, owner_id, roles, {}, overwrites);
  EXPECT_EQ(perms, permission_bits::all);
}

TEST(PermissionsExtended, MemberWithNoMatchingRoles) {
  // Member has role IDs that don't exist in guild_roles
  std::vector<Role> roles = {make_role(guild_id, permission_bits::view_channel)};
  auto perms = compute_permissions(user_id, guild_id, owner_id, roles, {999, 998, 997}, {});
  // Should still get @everyone base permissions
  EXPECT_TRUE(can_view_channel(perms));
}

TEST(PermissionsExtended, OverwriteForUnheldRoleIgnored) {
  // Channel has a role overwrite for role 10, but user doesn't have role 10
  std::vector<Role> roles = {
      make_role(guild_id, permission_bits::view_channel | permission_bits::send_messages),
      make_role(10, 0),
  };
  std::vector<PermissionOverwrite> overwrites = {
      make_overwrite(10, 0, 0, permission_bits::send_messages),
  };
  // User does NOT have role 10
  auto perms = compute_permissions(user_id, guild_id, owner_id, roles, {}, overwrites);
  EXPECT_TRUE(can_send_messages(perms));
}

// ---------------------------------------------------------------------------
// Tier 3: Absolutely unhinged scenarios
// ---------------------------------------------------------------------------

TEST(PermissionsExtended, FiftyRolesAllORed) {
  // User holds 50 roles, each granting different permission bits.
  // Skip bit 3 (administrator) since that grants all permissions.
  std::vector<Role> roles;
  roles.push_back(make_role(guild_id, 0));
  std::vector<Snowflake> member_roles;

  uint64_t expected = 0;
  int bit_index = 0;
  for (int i = 0; i < 50; ++i) {
    if (bit_index == 3) ++bit_index;  // skip administrator
    Snowflake rid = 200 + i;
    uint64_t bit = 1ULL << bit_index;
    roles.push_back(make_role(rid, bit, i + 1));
    member_roles.push_back(rid);
    expected |= bit;
    ++bit_index;
  }

  auto perms = compute_permissions(user_id, guild_id, owner_id, roles, member_roles, {});
  EXPECT_EQ(perms, expected);
}

TEST(PermissionsExtended, HundredOverwritesAccumulate) {
  std::vector<Role> roles = {make_role(guild_id, permission_bits::all)};
  std::vector<Snowflake> member_roles;
  std::vector<PermissionOverwrite> overwrites;

  // 100 role overwrites, alternating deny/allow on view_channel
  for (int i = 0; i < 100; ++i) {
    Snowflake rid = 300 + i;
    roles.push_back(make_role(rid, 0, i + 1));
    member_roles.push_back(rid);
    if (i % 2 == 0) {
      overwrites.push_back(make_overwrite(rid, 0, 0, permission_bits::view_channel));
    } else {
      overwrites.push_back(make_overwrite(rid, 0, permission_bits::view_channel, 0));
    }
  }

  auto perms = compute_permissions(user_id, guild_id, owner_id, roles, member_roles, overwrites);
  // At least one allow exists, so view_channel should be allowed (OR of allows)
  EXPECT_TRUE(can_view_channel(perms));
}

TEST(PermissionsExtended, OwnerWithZeroGuildId) {
  // Edge case: guild_id is 0 (shouldn't happen but shouldn't crash)
  std::vector<Role> roles = {make_role(0, 0)};
  auto perms = compute_permissions(owner_id, 0, owner_id, roles, {}, {});
  EXPECT_EQ(perms, permission_bits::all);
}

TEST(PermissionsExtended, SameIdForRoleAndUser) {
  // User ID matches a role ID in overwrites (type field distinguishes them)
  Snowflake shared_id = 42;
  std::vector<Role> roles = {
      make_role(guild_id, permission_bits::view_channel),
      make_role(shared_id, 0),
  };
  std::vector<PermissionOverwrite> overwrites = {
      make_overwrite(shared_id, 0, 0, permission_bits::view_channel),  // role overwrite: deny
      make_overwrite(shared_id, 1, permission_bits::view_channel, 0),  // user overwrite: allow
  };
  auto perms = compute_permissions(shared_id, guild_id, owner_id, roles, {shared_id}, overwrites);
  EXPECT_TRUE(can_view_channel(perms));
}

TEST(PermissionsExtended, EmptyEverything) {
  std::vector<Role> roles;
  auto perms = compute_permissions(user_id, guild_id, owner_id, roles, {}, {});
  EXPECT_EQ(perms, 0ULL);
}

TEST(PermissionsExtended, DenyAllThenUserAllowAll) {
  // @everyone overwrite denies everything, user overwrite allows everything
  std::vector<Role> roles = {make_role(guild_id, permission_bits::all)};
  std::vector<PermissionOverwrite> overwrites = {
      make_overwrite(guild_id, 0, 0, permission_bits::all),
      make_overwrite(user_id, 1, permission_bits::all, 0),
  };
  auto perms = compute_permissions(user_id, guild_id, owner_id, roles, {}, overwrites);
  EXPECT_EQ(perms, permission_bits::all);
}
