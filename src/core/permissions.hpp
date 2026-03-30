#pragma once

#include "models/permission_overwrite.hpp"
#include "models/role.hpp"
#include "models/snowflake.hpp"

#include <cstdint>
#include <vector>

namespace kind {

namespace permission_bits {
  constexpr uint64_t administrator = 1ULL << 3;
  constexpr uint64_t view_channel = 1ULL << 10;
  constexpr uint64_t send_messages = 1ULL << 11;
  constexpr uint64_t all = ~0ULL;
} // namespace permission_bits

uint64_t compute_permissions(
    Snowflake user_id,
    Snowflake guild_id,
    Snowflake guild_owner_id,
    const std::vector<Role>& guild_roles,
    const std::vector<Snowflake>& member_role_ids,
    const std::vector<PermissionOverwrite>& channel_overwrites);

bool can_view_channel(uint64_t perms);
bool can_send_messages(uint64_t perms);

} // namespace kind
