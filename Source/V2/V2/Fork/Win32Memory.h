#pragma once

#include "AddressProfile.h"

namespace fork_boundary {

// Bounded current-process memory operations only. Does not authenticate the
// supplied image base/extent, executable identity, dependencies or safe phase.
// Caller must establish those gates and exclusive access independently. No
// production caller is installed. Callback state captures values, not an owner
// pointer; the caller must keep the entire mapped allocation alive throughout.
// Mutation spans must fit one system page; unsupported spans fail unchanged.
MemoryAccess MakeWin32ImageMemory(std::uint32_t base, std::uint32_t imageSize);

} // namespace fork_boundary
