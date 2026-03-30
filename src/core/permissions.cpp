#include "permissions.hpp"

#include <unordered_set>

namespace kind {

uint64_t compute_permissions(
    Snowflake user_id,
    Snowflake guild_id,
    Snowflake guild_owner_id,
    const std::vector<Role>& guild_roles,
    const std::vector<Snowflake>& member_role_ids,
    const std::vector<PermissionOverwrite>& channel_overwrites) {

  if (user_id == guild_owner_id) {
    return permission_bits::all;
  }

  uint64_t base = 0;
  for (const auto& role : guild_roles) {
    if (role.id == guild_id) {
      base = role.permissions;
      break;
    }
  }

  std::unordered_set<Snowflake> role_set(member_role_ids.begin(), member_role_ids.end());
  for (const auto& role : guild_roles) {
    if (role_set.count(role.id)) {
      base |= role.permissions;
    }
  }

  if (base & permission_bits::administrator) {
    return permission_bits::all;
  }

  for (const auto& ow : channel_overwrites) {
    if (ow.id == guild_id && ow.type == 0) {
      base = (base & ~ow.deny) | ow.allow;
      break;
    }
  }

  uint64_t role_allow = 0;
  uint64_t role_deny = 0;
  for (const auto& ow : channel_overwrites) {
    if (ow.type == 0 && ow.id != guild_id && role_set.count(ow.id)) {
      role_allow |= ow.allow;
      role_deny |= ow.deny;
    }
  }
  base = (base & ~role_deny) | role_allow;

  for (const auto& ow : channel_overwrites) {
    if (ow.id == user_id && ow.type == 1) {
      base = (base & ~ow.deny) | ow.allow;
      break;
    }
  }

  return base;
}

bool can_view_channel(uint64_t perms) {
  return (perms & permission_bits::view_channel) != 0;
}

bool can_send_messages(uint64_t perms) {
  return (perms & permission_bits::send_messages) != 0;
}

} // namespace kind
