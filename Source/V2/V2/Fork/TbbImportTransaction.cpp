#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "TbbImportTransaction.h"
#include "StartupIdentity.h"
#include "Win32Memory.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

#if !defined(_M_IX86)
#error The TBB import transaction supports Win32 only.
#endif

namespace fork_boundary {
namespace {

constexpr std::uint32_t kPinnedTbbPreferredBase = 0x10000000;
constexpr std::uint32_t kPinnedTbbTextRva = 0x1000;
constexpr std::uint32_t kPinnedTbbTextSize = 0x1689A;
constexpr std::size_t kMaxDescriptors = 64;
constexpr std::size_t kMaxThunks = 512;
constexpr std::size_t kMaxName = 64;
constexpr std::size_t kMaxSections = 32;
constexpr std::size_t kPathChars = 32768;

// Derived from pinned TBB SHA256 2280E7AC...E12E32 relocation directory.
// The entry at each cumulative RVA is a HIGHLOW DWORD in virtual .text.
// No proprietary TBB code bytes are embedded. Root's private derivation
// hashes these 1,809 little-endian uint16 deltas to 314F1578...6C6841.
static constexpr std::uint16_t kPinnedTextRelocationDeltas[] = {
    0x0164, 0x0098, 0x00A9, 0x0012, 0x0012, 0x001A, 0x006E, 0x000A, 0x0248, 0x0013, 0x0153, 0x00FF,
    0x0050, 0x00B5, 0x0278, 0x009E, 0x0013, 0x0085, 0x0019, 0x000C, 0x0019, 0x000C, 0x0095, 0x0004,
    0x001F, 0x000A, 0x0008, 0x002C, 0x00E7, 0x01CF, 0x0035, 0x0012, 0x00B4, 0x006A, 0x0025, 0x008B,
    0x0012, 0x019E, 0x0012, 0x004F, 0x0030, 0x0043, 0x0049, 0x000A, 0x0066, 0x0063, 0x0012, 0x002F,
    0x0056, 0x006F, 0x007D, 0x000A, 0x04A5, 0x0309, 0x0056, 0x0017, 0x006E, 0x0016, 0x002E, 0x0016,
    0x0039, 0x003B, 0x0049, 0x0155, 0x00D8, 0x002E, 0x0014, 0x00A4, 0x0012, 0x0077, 0x006C, 0x001C,
    0x001A, 0x001F, 0x005B, 0x0098, 0x0012, 0x00C1, 0x0012, 0x0385, 0x001A, 0x005F, 0x0012, 0x04EE,
    0x0012, 0x00E4, 0x006E, 0x0034, 0x000B, 0x007B, 0x001F, 0x0013, 0x02D9, 0x002E, 0x0012, 0x00F7,
    0x0008, 0x000A, 0x001A, 0x000A, 0x008B, 0x0058, 0x000C, 0x0070, 0x00BC, 0x002A, 0x0046, 0x002D,
    0x0039, 0x000F, 0x000C, 0x0024, 0x003F, 0x0011, 0x000F, 0x0040, 0x0018, 0x001C, 0x0017, 0x0009,
    0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009,
    0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009,
    0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009,
    0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009,
    0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009,
    0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0027, 0x0009,
    0x0027, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009,
    0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009,
    0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009,
    0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009,
    0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009,
    0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009,
    0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009,
    0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009, 0x0017, 0x0009,
    0x0017, 0x0009, 0x0112, 0x0025, 0x0005, 0x000B, 0x0021, 0x0009, 0x000A, 0x0009, 0x001F, 0x000F,
    0x0016, 0x0012, 0x0020, 0x001B, 0x0007, 0x002F, 0x0007, 0x0016, 0x0008, 0x000B, 0x0013, 0x0019,
    0x001B, 0x0026, 0x0007, 0x0013, 0x0008, 0x0022, 0x002B, 0x0007, 0x0015, 0x0015, 0x000E, 0x000E,
    0x0005, 0x001B, 0x000F, 0x0010, 0x0013, 0x000F, 0x0015, 0x0005, 0x0010, 0x0005, 0x0017, 0x0007,
    0x0015, 0x000A, 0x000A, 0x0008, 0x001F, 0x0014, 0x0007, 0x0015, 0x0006, 0x0005, 0x0006, 0x0006,
    0x0006, 0x0005, 0x0006, 0x000A, 0x000F, 0x0010, 0x0008, 0x0015, 0x0013, 0x0013, 0x0017, 0x000A,
    0x001F, 0x0006, 0x001C, 0x0010, 0x0010, 0x0010, 0x000A, 0x0015, 0x0030, 0x002C, 0x001F, 0x0030,
    0x0017, 0x0036, 0x0013, 0x0005, 0x000D, 0x0005, 0x000B, 0x0014, 0x0005, 0x0011, 0x0006, 0x0005,
    0x0005, 0x0006, 0x0006, 0x0006, 0x0004, 0x0006, 0x0004, 0x000E, 0x0006, 0x00F1, 0x0048, 0x00F0,
    0x0012, 0x006C, 0x00DD, 0x0023, 0x0022, 0x001B, 0x005F, 0x0080, 0x0029, 0x0018, 0x0013, 0x0021,
    0x001D, 0x0022, 0x00D8, 0x0023, 0x000E, 0x0019, 0x0036, 0x0023, 0x000E, 0x0019, 0x003B, 0x0033,
    0x0023, 0x0045, 0x001C, 0x0040, 0x0086, 0x0028, 0x0071, 0x0076, 0x003A, 0x0019, 0x001F, 0x0013,
    0x00A7, 0x0045, 0x0031, 0x0013, 0x0041, 0x00B4, 0x0019, 0x002F, 0x0010, 0x021F, 0x0019, 0x003C,
    0x006C, 0x0013, 0x0058, 0x008C, 0x003C, 0x004D, 0x0018, 0x0058, 0x0010, 0x0033, 0x00CD, 0x0012,
    0x000F, 0x020C, 0x00A7, 0x0099, 0x0132, 0x0021, 0x0082, 0x009B, 0x0072, 0x006C, 0x009D, 0x00F5,
    0x0015, 0x00BC, 0x000B, 0x0005, 0x0057, 0x0050, 0x0146, 0x001F, 0x0024, 0x00CB, 0x00E5, 0x000B,
    0x0005, 0x0087, 0x0050, 0x0050, 0x0050, 0x0050, 0x0402, 0x0081, 0x0055, 0x0026, 0x0085, 0x0029,
    0x014E, 0x012F, 0x0096, 0x00B1, 0x0080, 0x004F, 0x004E, 0x00E3, 0x0031, 0x0044, 0x004B, 0x0032,
    0x0021, 0x018D, 0x0210, 0x0050, 0x0050, 0x0066, 0x0060, 0x0071, 0x0022, 0x01BE, 0x012F, 0x0070,
    0x0073, 0x008B, 0x0061, 0x00FE, 0x003C, 0x00CC, 0x0010, 0x0082, 0x01ED, 0x0022, 0x0017, 0x0055,
    0x0021, 0x0030, 0x0052, 0x003E, 0x000B, 0x0005, 0x0046, 0x0074, 0x0007, 0x0062, 0x0023, 0x0020,
    0x004D, 0x0051, 0x006C, 0x0022, 0x0051, 0x001D, 0x000B, 0x0005, 0x0010, 0x0030, 0x000B, 0x0005,
    0x00A0, 0x0030, 0x0039, 0x003B, 0x011C, 0x0007, 0x0019, 0x0007, 0x0029, 0x0009, 0x0018, 0x002F,
    0x0007, 0x0049, 0x000C, 0x0048, 0x002C, 0x0007, 0x0049, 0x000A, 0x0042, 0x0007, 0x003D, 0x000A,
    0x0036, 0x0007, 0x006B, 0x0007, 0x0037, 0x0038, 0x0010, 0x0007, 0x0051, 0x0007, 0x0036, 0x0007,
    0x007C, 0x0007, 0x0041, 0x0031, 0x0010, 0x0007, 0x0050, 0x000D, 0x0060, 0x0007, 0x002E, 0x0070,
    0x000E, 0x00C2, 0x000E, 0x0052, 0x000E, 0x0052, 0x000E, 0x0052, 0x000E, 0x0052, 0x0010, 0x0024,
    0x008B, 0x0013, 0x0006, 0x0006, 0x000C, 0x0016, 0x0009, 0x0006, 0x0009, 0x0017, 0x0017, 0x000B,
    0x0013, 0x003A, 0x000C, 0x0006, 0x000A, 0x0032, 0x0006, 0x0006, 0x0017, 0x0006, 0x000D, 0x0006,
    0x002F, 0x0006, 0x0012, 0x0022, 0x0006, 0x0018, 0x0022, 0x0006, 0x0018, 0x0022, 0x0006, 0x0018,
    0x0018, 0x0006, 0x0012, 0x0022, 0x0006, 0x0018, 0x003F, 0x0010, 0x0010, 0x0010, 0x0016, 0x0023,
    0x0007, 0x000D, 0x0016, 0x000B, 0x0022, 0x0005, 0x000D, 0x0017, 0x0006, 0x0017, 0x002C, 0x0016,
    0x0005, 0x000D, 0x0024, 0x0006, 0x0006, 0x0017, 0x0006, 0x000D, 0x0006, 0x0017, 0x0013, 0x0006,
    0x0017, 0x0013, 0x0006, 0x0017, 0x0013, 0x0006, 0x002A, 0x0020, 0x0020, 0x0022, 0x000C, 0x003F,
    0x0007, 0x0009, 0x0006, 0x000A, 0x0012, 0x0006, 0x000A, 0x000E, 0x0012, 0x0006, 0x000A, 0x0069,
    0x0038, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0032, 0x0011, 0x001B, 0x000F, 0x000E, 0x000F,
    0x0011, 0x0012, 0x0011, 0x0012, 0x000E, 0x0014, 0x0005, 0x000D, 0x000F, 0x0005, 0x000D, 0x000F,
    0x0005, 0x000D, 0x0015, 0x0006, 0x0005, 0x000D, 0x0015, 0x0006, 0x0005, 0x000D, 0x0015, 0x0006,
    0x0005, 0x000D, 0x000A, 0x000E, 0x000F, 0x0011, 0x0012, 0x0011, 0x0012, 0x0011, 0x0012, 0x0011,
    0x0019, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004,
    0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x000E, 0x0006, 0x001A, 0x0006, 0x001A, 0x0006, 0x001A,
    0x0006, 0x001A, 0x0006, 0x001A, 0x0006, 0x001A, 0x0006, 0x001A, 0x0006, 0x001A, 0x0006, 0x001A,
    0x0006, 0x001C, 0x0056, 0x0014, 0x001F, 0x0006, 0x0059, 0x0011, 0x0005, 0x0006, 0x0014, 0x0015,
    0x0007, 0x0026, 0x000C, 0x0008, 0x000B, 0x001A, 0x0007, 0x000F, 0x000B, 0x0017, 0x0006, 0x0028,
    0x001E, 0x0006, 0x0018, 0x0010, 0x0011, 0x001F, 0x0009, 0x0017, 0x0006, 0x000A, 0x0006, 0x004D,
    0x000C, 0x0017, 0x0081, 0x0130, 0x003D, 0x0009, 0x000A, 0x000A, 0x002A, 0x002E, 0x0021, 0x002B,
    0x000C, 0x000B, 0x0005, 0x001D, 0x0025, 0x001D, 0x001A, 0x001B, 0x000C, 0x000B, 0x0005, 0x001D,
    0x007E, 0x001E, 0x0010, 0x0033, 0x0009, 0x001F, 0x0009, 0x0024, 0x0009, 0x0019, 0x000B, 0x0023,
    0x000B, 0x001E, 0x0011, 0x0012, 0x001D, 0x000F, 0x0010, 0x002A, 0x0005, 0x0006, 0x0013, 0x0004,
    0x0010, 0x0010, 0x0010, 0x001C, 0x0020, 0x002D, 0x0058, 0x001B, 0x0019, 0x0034, 0x0015, 0x0009,
    0x0013, 0x007B, 0x0065, 0x000E, 0x003C, 0x0006, 0x0019, 0x002E, 0x000A, 0x003B, 0x000D, 0x0007,
    0x0020, 0x000E, 0x0013, 0x001C, 0x000A, 0x003A, 0x001C, 0x0015, 0x001F, 0x0015, 0x003B, 0x003B,
    0x00CD, 0x00E7, 0x00AC, 0x00F4, 0x000E, 0x0006, 0x000A, 0x000B, 0x001E, 0x0007, 0x0023, 0x000C,
    0x0005, 0x000A, 0x0006, 0x000C, 0x000D, 0x0006, 0x002F, 0x001E, 0x000A, 0x0005, 0x001C, 0x0011,
    0x0007, 0x0017, 0x001A, 0x0014, 0x0019, 0x0036, 0x000B, 0x001E, 0x000A, 0x0005, 0x0016, 0x000D,
    0x001E, 0x000A, 0x0005, 0x00A6, 0x000D, 0x001E, 0x000A, 0x0005, 0x00D6, 0x0017, 0x0019, 0x0020,
    0x000A, 0x0005, 0x003F, 0x001F, 0x000E, 0x02FA, 0x0012, 0x0007, 0x0021, 0x0013, 0x001D, 0x0150,
    0x000F, 0x0140, 0x0069, 0x008C, 0x009D, 0x0018, 0x0012, 0x0007, 0x0111, 0x0024, 0x0012, 0x0007,
    0x0061, 0x001A, 0x0005, 0x0006, 0x0027, 0x0008, 0x0008, 0x0008, 0x000C, 0x0098, 0x0005, 0x0006,
    0x0035, 0x0005, 0x0006, 0x00DF, 0x0020, 0x001D, 0x0018, 0x0018, 0x000E, 0x0028, 0x0017, 0x0023,
    0x0010, 0x0020, 0x0023, 0x0018, 0x0023, 0x0010, 0x0018, 0x0051, 0x0006, 0x0022, 0x0013, 0x0006,
    0x001A, 0x0026, 0x0050, 0x0043, 0x0032, 0x0063, 0x0040, 0x0010, 0x0039, 0x005C, 0x0103, 0x0006,
    0x0024, 0x0012, 0x0026, 0x0014, 0x000F, 0x0015, 0x0006, 0x0016, 0x0007, 0x0021, 0x001A, 0x0025,
    0x0006, 0x0041, 0x0019, 0x001B, 0x0010, 0x0035, 0x0012, 0x0029, 0x0011, 0x0046, 0x0016, 0x0023,
    0x0011, 0x0046, 0x0016, 0x0026, 0x002B, 0x0032, 0x000E, 0x0020, 0x0020, 0x001C, 0x009C, 0x0019,
    0x0036, 0x0006, 0x0006, 0x004D, 0x000D, 0x0096, 0x00F3, 0x0015, 0x0110, 0x0008, 0x0016, 0x0039,
    0x0012, 0x0043, 0x0006, 0x0006, 0x00E6, 0x0076, 0x0023, 0x0015, 0x00AC, 0x005D, 0x0007, 0x003E,
    0x0012, 0x0060, 0x0046, 0x0011, 0x001A, 0x0039, 0x0005, 0x0019, 0x0097, 0x0005, 0x0017, 0x001B,
    0x000F, 0x0006, 0x003F, 0x0008, 0x0005, 0x000F, 0x0005, 0x000D, 0x0005, 0x0005, 0x000B, 0x0008,
    0x0005, 0x005F, 0x000E, 0x0016, 0x0023, 0x0022, 0x0028, 0x000A, 0x0007, 0x000F, 0x000F, 0x0010,
    0x000A, 0x0007, 0x0006, 0x0024, 0x0009, 0x0040, 0x0007, 0x001E, 0x0007, 0x0010, 0x004E, 0x0008,
    0x0009, 0x0009, 0x0018, 0x0019, 0x0008, 0x0006, 0x0005, 0x00BB, 0x003A, 0x000E, 0x0007, 0x0011,
    0x0009, 0x0017, 0x000E, 0x0230, 0x0008, 0x0013, 0x0007, 0x001C, 0x001E, 0x0007, 0x000C, 0x0007,
    0x0019, 0x004F, 0x000F, 0x0019, 0x0009, 0x00B8, 0x0096, 0x0013, 0x0012, 0x0014, 0x0006, 0x001F,
    0x0012, 0x000E, 0x0051, 0x0037, 0x000A, 0x0222, 0x000A, 0x0464, 0x000F, 0x0181, 0x000F, 0x0061,
    0x000F, 0x0071, 0x000F, 0x0071, 0x000F, 0x0071, 0x0012, 0x0084, 0x0005, 0x0061, 0x0070, 0x0070,
    0x0070, 0x0030, 0x0024, 0x0011, 0x007E, 0x0010, 0x0005, 0x0020, 0x000A, 0x0009, 0x000A, 0x0021,
    0x000A, 0x0006, 0x003E, 0x0011, 0x00BE, 0x00AD, 0x012C, 0x0437, 0x0010, 0x0012, 0x000E, 0x008B,
    0x00A7, 0x00D9, 0x0028, 0x0007, 0x006E, 0x002F, 0x0012, 0x001D, 0x006D, 0x005C, 0x0048, 0x000A,
    0x0006, 0x01E8, 0x0006, 0x011A, 0x0057, 0x001A, 0x0018, 0x0019, 0x0011, 0x0040, 0x00A2, 0x000A,
    0x0006, 0x0019, 0x000A, 0x0006, 0x0012, 0x000D, 0x0006, 0x0036, 0x0019, 0x0011, 0x001A, 0x00D0,
    0x0058, 0x001D, 0x0010, 0x000C, 0x0020, 0x0013, 0x0061, 0x000F, 0x0019, 0x0013, 0x0013, 0x0111,
    0x00F5, 0x007F, 0x000C, 0x0018, 0x0020, 0x0166, 0x0052, 0x0051, 0x00A5, 0x00FA, 0x001F, 0x00C3,
    0x0128, 0x0029, 0x0102, 0x0048, 0x00A4, 0x0010, 0x004B, 0x0005, 0x0012, 0x0006, 0x0028, 0x0010,
    0x004B, 0x0005, 0x0012, 0x0006, 0x00BC, 0x0004, 0x0008, 0x0004, 0x0098, 0x03D0, 0x0029, 0x003B,
    0x0016, 0x0054, 0x0124, 0x0082, 0x0013, 0x0032, 0x00B7, 0x0170, 0x0082, 0x002D, 0x0037, 0x0012,
    0x0013, 0x0019, 0x003F, 0x0029, 0x00CB, 0x008C, 0x005A, 0x00A6, 0x0039, 0x0096, 0x0007, 0x0029,
    0x0007, 0x0040, 0x0006, 0x0128, 0x0012, 0x0019, 0x000A, 0x0006, 0x0072, 0x001D, 0x002F, 0x01B2,
    0x0017, 0x0031, 0x004A, 0x0011, 0x0024, 0x0030, 0x0005, 0x0023, 0x0010, 0x0004, 0x0004, 0x0004,
    0x0004, 0x0004, 0x0012, 0x0012, 0x0098, 0x01A6, 0x0072, 0x0011, 0x001D, 0x0030, 0x0005, 0x001C,
    0x0005, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0091, 0x0007, 0x0029, 0x0007, 0x002B, 0x0013,
    0x00DB, 0x0040, 0x000A, 0x0023, 0x0135, 0x0017, 0x00A8, 0x0010, 0x0012, 0x0006, 0x0008, 0x005F,
    0x0038, 0x0017, 0x0008, 0x002E, 0x0012, 0x0029, 0x000B, 0x0022, 0x005D, 0x0066, 0x0018, 0x0050,
    0x002A, 0x000F, 0x0025, 0x0036, 0x000F, 0x002C, 0x0013, 0x0020, 0x0086, 0x0019, 0x0008, 0x0012,
    0x0005, 0x0006, 0x000B, 0x0006, 0x0009, 0x0023, 0x0005, 0x0014, 0x0014, 0x0007, 0x0014, 0x0004,
    0x0004, 0x0004, 0x0021, 0x0010, 0x0007, 0x001A, 0x000F, 0x000E, 0x0012, 0x0005, 0x0006, 0x000B,
    0x0006, 0x0009, 0x000B, 0x0010, 0x0038, 0x001E, 0x0030, 0x002D, 0x000E, 0x000F, 0x0023, 0x0017,
    0x000F, 0x0007, 0x0006, 0x000B, 0x0006, 0x0009, 0x000C, 0x001C, 0x000D, 0x000D, 0x0007, 0x0006,
    0x000E, 0x0007, 0x0024, 0x0007, 0x0019, 0x0030, 0x0020, 0x000C, 0x0251, 0x00D8, 0x0006, 0x0005,
    0x000D, 0x038A, 0x0016, 0x0017, 0x00C6, 0x001C, 0x0039, 0x000E, 0x0384, 0x00BC, 0x024D, 0x0013,
    0x00DD, 0x0013, 0x0176, 0x00F6, 0x00B1, 0x00EE, 0x0013, 0x0075, 0x0007, 0x0021, 0x0033, 0x0066,
    0x0007, 0x0020, 0x0031, 0x0065, 0x0065, 0x0064, 0x0023, 0x001B, 0x0042, 0x018E, 0x0012, 0x0008,
    0x0017, 0x0043, 0x0006, 0x0006, 0x0006, 0x0010, 0x0006, 0x0006, 0x003E, 0x0006, 0x0006, 0x0006,
    0x0019, 0x005E, 0x0063, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006,
    0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x000E, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006,
    0x0006, 0x0006, 0x0006, 0x0006, 0x0005, 0x000E, 0x0065, 0x0006, 0x0007, 0x000B, 0x0006, 0x0013,
    0x0015, 0x000B, 0x0016, 0x0018, 0x000A, 0x0043, 0x000A, 0x0005, 0x0005, 0x0016, 0x000C, 0x0021,
    0x000E, 0x000D, 0x0005, 0x0026, 0x0009, 0x0011, 0x0018, 0x0015, 0x0005, 0x0006, 0x0018, 0x0005,
    0x000C, 0x0010, 0x0006, 0x0007, 0x0017, 0x0006, 0x0014, 0x0018, 0x0018, 0x0006, 0x0013, 0x0020,
    0x0011, 0x000B, 0x0008, 0x0028, 0x0007, 0x0005, 0x0005, 0x0007, 0x000D, 0x0011, 0x001D, 0x000D,
    0x0018, 0x0053, 0x002D, 0x0048, 0x0038, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0007, 0x0007,
    0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0008, 0x0008, 0x0008, 0x000C, 0x0009, 0x0005, 0x0006,
    0x000A, 0x0009, 0x000B, 0x000C, 0x0005, 0x0010, 0x0005, 0x0006, 0x0006, 0x0016, 0x0007, 0x003B,
    0x0013, 0x001D, 0x004D, 0x0005, 0x0010, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0008, 0x0005,
    0x0021, 0x0005, 0x001F, 0x0006, 0x00A2, 0x0005, 0x0012, 0x001E, 0x0014, 0x0006, 0x0067, 0x0006,
    0x0006, 0x0006, 0x000D, 0x0023, 0x000D, 0x000C, 0x0008, 0x0008, 0x000C, 0x0029, 0x0008, 0x000C,
    0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006,
    0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006,
    0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006,
    0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006,
    0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x002A, 0x0028, 0x0020, 0x0028, 0x0030, 0x0028, 0x0028,
    0x0030, 0x0028, 0x0028, 0x0038, 0x0028, 0x0030, 0x0030, 0x0030, 0x0054, 0x002C, 0x0030, 0x0046,
    0x002A, 0x001B, 0x0016, 0x0022, 0x0016, 0x001A, 0x0016, 0x001A, 0x0016, 0x001A, 0x0016, 0x001A,
    0x0016, 0x0039, 0x0037, 0x0038, 0x0048, 0x0030, 0x0030, 0x0038, 0x0040, 0x002F, 0x0039, 0x001F,
    0x0039, 0x001A, 0x0016, 0x001A, 0x0016, 0x001A, 0x0016, 0x001A, 0x0016, 0x001A, 0x0016, 0x0032,
    0x002D, 0x0039, 0x0027, 0x0039, 0x0026, 0x0031, 0x0039, 0x0030, 0x0030, 0x0027, 0x0028, 0x0020,
    0x0028, 0x0028, 0x002B, 0x002D, 0x0030, 0x0018, 0x000F, 0x000A, 0x0016,
};

struct GateExit {
    volatile long* gate;
    long next = 0;
    ~GateExit() noexcept {
        InterlockedExchange(reinterpret_cast<volatile LONG*>(gate), static_cast<LONG>(next));
    }
};

bool InImage(std::uint32_t rva, std::size_t count, std::uint32_t size) noexcept {
    return count && rva < size && count <= size-rva;
}

template <class T>
bool ReadObject(const MemoryAccess& memory, std::uint32_t rva,
                std::uint32_t size, T& output) {
    return InImage(rva,sizeof(T),size) &&
        memory.read(rva,reinterpret_cast<unsigned char*>(&output),sizeof(T));
}

template <class T>
bool ReadProviderObject(const TbbImportBackend& backend, std::uint32_t base,
                        std::uint32_t rva, std::uint32_t size, T& output) {
    return InImage(rva,sizeof(T),size) && backend.readProvider(base,rva,
        reinterpret_cast<unsigned char*>(&output),sizeof(T));
}

bool ReadName(const MemoryAccess& memory, std::uint32_t rva,
              std::uint32_t size, char (&output)[kMaxName]) {
    for (std::size_t i=0;i<kMaxName;++i) {
        if (rva>std::numeric_limits<std::uint32_t>::max()-i ||
            !InImage(rva+static_cast<std::uint32_t>(i),1,size) ||
            !memory.read(rva+static_cast<std::uint32_t>(i),
                         reinterpret_cast<unsigned char*>(&output[i]),1)) return false;
        if (!output[i]) return true;
    }
    return false;
}

bool ProviderNameEquals(const TbbImportBackend& backend,std::uint32_t base,
                        std::uint32_t rva,std::uint32_t size,
                        const char* expected,bool& match) {
    match=false;
    const auto length=std::strlen(expected);
    for (std::size_t i=0;i<=length;++i) {
        char value=0;
        if (rva>std::numeric_limits<std::uint32_t>::max()-i ||
            !InImage(rva+static_cast<std::uint32_t>(i),1,size) ||
            !backend.readProvider(base,rva+static_cast<std::uint32_t>(i),
                                  reinterpret_cast<unsigned char*>(&value),1)) return false;
        if (value!=expected[i]) return true;
    }
    match=true;
    return true;
}

char LowerAscii(char value) noexcept {
    return value>='A' && value<='Z' ? static_cast<char>(value-'A'+'a') : value;
}

bool SameAsciiNoCase(const char* left,const char* right) noexcept {
    for (;*left && *right;++left,++right)
        if (LowerAscii(*left)!=LowerAscii(*right)) return false;
    return *left==*right;
}

bool ReadDiskHash(const TbbImportBackend& backend,std::uint32_t base,
                  std::uint64_t size,const std::array<unsigned char,32>& expected) {
    // Keep the 32,768-character Win32 path bound off the callback stack.
    // This allocation occurs before any IAT protection or pointer mutation.
    std::vector<wchar_t> path(kPathChars,L'\0');
    if (!backend.modulePath(base,path.data(),path.size())) return false;
    fork_startup::FileIdentity identity{};
    if (fork_startup::ReadFileIdentity(path.data(),size,identity)!=
        fork_startup::IdentityReadStatus::Ok) return false;
    return identity.size==size &&
        std::memcmp(identity.sha256,expected.data(),expected.size())==0;
}

bool ReadPeHeaders(const MemoryAccess& memory,std::uint32_t imageSize,
                   IMAGE_NT_HEADERS32& nt,std::uint32_t& ntRva) {
    IMAGE_DOS_HEADER dos{};
    if (!ReadObject(memory,0,imageSize,dos) || dos.e_magic!=IMAGE_DOS_SIGNATURE ||
        dos.e_lfanew<0) return false;
    ntRva=static_cast<std::uint32_t>(dos.e_lfanew);
    return ReadObject(memory,ntRva,imageSize,nt) &&
        nt.Signature==IMAGE_NT_SIGNATURE &&
        nt.FileHeader.Machine==IMAGE_FILE_MACHINE_I386 &&
        nt.FileHeader.SizeOfOptionalHeader>=sizeof(IMAGE_OPTIONAL_HEADER32) &&
        nt.FileHeader.NumberOfSections>0 &&
        nt.FileHeader.NumberOfSections<=kMaxSections &&
        nt.OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR32_MAGIC &&
        nt.OptionalHeader.SizeOfImage==imageSize &&
        nt.OptionalHeader.NumberOfRvaAndSizes>IMAGE_DIRECTORY_ENTRY_IMPORT;
}

bool AuthenticateText(const MemoryAccess& memory,std::uint32_t base,
                      const TbbImportExpectation& expected,
                      const IMAGE_NT_HEADERS32& nt,std::uint32_t ntRva) {
    const auto sectionOffset=static_cast<std::uint64_t>(ntRva)+sizeof(DWORD)+
        sizeof(IMAGE_FILE_HEADER)+nt.FileHeader.SizeOfOptionalHeader;
    if (sectionOffset>std::numeric_limits<std::uint32_t>::max()) return false;
    std::size_t textMatches=0;
    for (std::size_t i=0;i<nt.FileHeader.NumberOfSections;++i) {
        const auto offset=sectionOffset+i*sizeof(IMAGE_SECTION_HEADER);
        if (offset>std::numeric_limits<std::uint32_t>::max()) return false;
        IMAGE_SECTION_HEADER section{};
        if (!ReadObject(memory,static_cast<std::uint32_t>(offset),
                        expected.imageSize,section)) return false;
        if (std::memcmp(section.Name,".text",5)!=0) continue;
        if (++textMatches!=1 || section.VirtualAddress!=expected.textRva ||
            section.Misc.VirtualSize!=expected.textVirtualSize ||
            section.SizeOfRawData!=expected.textRawSize) return false;
    }
    if (textMatches!=1 || !InImage(expected.textRva,expected.textVirtualSize,
                                   expected.imageSize)) return false;
    std::vector<unsigned char> text(expected.textVirtualSize);
    if (!memory.read(expected.textRva,text.data(),text.size())) return false;
    std::uint32_t cursor=expected.textRva;
    std::uint32_t previous=0;
    const auto delta=base-expected.preferredBase; // modulo 2^32
    for (std::size_t i=0;i<expected.textRelocationCount;++i) {
        const auto step=expected.textRelocationDeltas[i];
        if (!step || cursor>std::numeric_limits<std::uint32_t>::max()-step) return false;
        cursor+=step;
        if (i && cursor<previous+4) return false;
        if (cursor<expected.textRva ||
            !InImage(cursor-expected.textRva,4,expected.textVirtualSize)) return false;
        const auto offset=cursor-expected.textRva;
        std::uint32_t value=0;
        std::memcpy(&value,text.data()+offset,4);
        value-=delta;
        std::memcpy(text.data()+offset,&value,4);
        previous=cursor;
    }
    unsigned char digest[32]{};
    fork_startup::HashBytes(text.data(),text.size(),digest);
    return std::memcmp(digest,expected.normalizedTextSha256.data(),32)==0;
}

bool AuthenticateImports(const MemoryAccess& memory,const TbbImportExpectation& expected,
                         const IMAGE_NT_HEADERS32& nt,bool& createThreadAbsent) {
    const auto& directory=nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!InImage(directory.VirtualAddress,directory.Size,expected.imageSize) ||
        directory.Size<sizeof(IMAGE_IMPORT_DESCRIPTOR) ||
        directory.Size>kMaxDescriptors*sizeof(IMAGE_IMPORT_DESCRIPTOR)) return false;
    bool found=false,ended=false,createThreadFound=false;
    for (std::size_t d=0;d<directory.Size/sizeof(IMAGE_IMPORT_DESCRIPTOR);++d) {
        IMAGE_IMPORT_DESCRIPTOR desc{};
        const auto rva=directory.VirtualAddress+static_cast<std::uint32_t>(d*sizeof(desc));
        if (!ReadObject(memory,rva,expected.imageSize,desc)) return false;
        if (!desc.Name && !desc.FirstThunk && !desc.OriginalFirstThunk) {ended=true;break;}
        if (!desc.Name || !desc.FirstThunk || !desc.OriginalFirstThunk) return false;
        char dll[kMaxName]{};
        if (!ReadName(memory,desc.Name,expected.imageSize,dll)) return false;
        bool thunkEnded=false;
        for (std::size_t t=0;t<kMaxThunks;++t) {
            const auto offset=static_cast<std::uint64_t>(t)*4;
            if (static_cast<std::uint64_t>(desc.FirstThunk)+offset>0xffffffffULL ||
                static_cast<std::uint64_t>(desc.OriginalFirstThunk)+offset>0xffffffffULL)
                return false;
            const auto slot=desc.FirstThunk+static_cast<std::uint32_t>(offset);
            const auto lookupRva=desc.OriginalFirstThunk+static_cast<std::uint32_t>(offset);
            std::uint32_t lookup=0,value=0;
            if (!ReadObject(memory,lookupRva,expected.imageSize,lookup) ||
                !ReadObject(memory,slot,expected.imageSize,value)) return false;
            if (!lookup) {thunkEnded=true;break;}
            if (lookup&IMAGE_ORDINAL_FLAG32) continue;
            if (lookup>expected.imageSize-2) return false;
            char name[kMaxName]{};
            if (!ReadName(memory,lookup+2,expected.imageSize,name)) return false;
            if (SameAsciiNoCase(dll,"MSVCR100.dll") &&
                std::strcmp(name,"_beginthreadex")==0) {
                if (found || slot!=expected.beginThreadExIatRva || (slot&3) || !value)
                    return false;
                found=true;
            }
            if (std::strcmp(name,"CreateThread")==0) createThreadFound=true;
        }
        if (!thunkEnded) return false;
    }
    createThreadAbsent=!createThreadFound;
    return found && ended && createThreadAbsent;
}

bool AuthenticateProvider(const TbbImportBackend& backend,std::uint32_t base,
                          std::uint32_t provider,const TbbImportExpectation& expected) {
    if (!base || !provider || !backend.isMappedImage(base,expected.crtImageSize) ||
        base>std::numeric_limits<std::uint32_t>::max()-expected.crtBeginThreadExExportRva ||
        provider!=base+expected.crtBeginThreadExExportRva ||
        !ReadDiskHash(backend,base,expected.crtDiskSize,expected.crtDiskSha256)) return false;
    IMAGE_DOS_HEADER dos{};
    if (!ReadProviderObject(backend,base,0,expected.crtImageSize,dos) ||
        dos.e_magic!=IMAGE_DOS_SIGNATURE || dos.e_lfanew<0) return false;
    IMAGE_NT_HEADERS32 nt{};
    if (!ReadProviderObject(backend,base,static_cast<std::uint32_t>(dos.e_lfanew),
                            expected.crtImageSize,nt)) return false;
    if (!(nt.Signature==IMAGE_NT_SIGNATURE &&
        nt.FileHeader.Machine==IMAGE_FILE_MACHINE_I386 &&
        nt.FileHeader.TimeDateStamp==expected.crtTimeDateStamp &&
        nt.OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR32_MAGIC &&
        nt.OptionalHeader.SizeOfImage==expected.crtImageSize &&
        nt.FileHeader.SizeOfOptionalHeader>=sizeof(IMAGE_OPTIONAL_HEADER32) &&
        nt.OptionalHeader.NumberOfRvaAndSizes>IMAGE_DIRECTORY_ENTRY_EXPORT)) return false;
    const auto& directory=nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!InImage(directory.VirtualAddress,directory.Size,expected.crtImageSize) ||
        directory.Size<sizeof(IMAGE_EXPORT_DIRECTORY)) return false;
    IMAGE_EXPORT_DIRECTORY exports{};
    if (!ReadProviderObject(backend,base,directory.VirtualAddress,
                            expected.crtImageSize,exports) ||
        !exports.NumberOfNames || exports.NumberOfNames>4096 ||
        !exports.NumberOfFunctions || exports.NumberOfFunctions>4096 ||
        !exports.AddressOfNames || !exports.AddressOfNameOrdinals ||
        !exports.AddressOfFunctions) return false;
    bool found=false;
    for (std::uint32_t i=0;i<exports.NumberOfNames;++i) {
        const auto nameOffset=static_cast<std::uint64_t>(exports.AddressOfNames)+4ull*i;
        const auto ordinalOffset=static_cast<std::uint64_t>(exports.AddressOfNameOrdinals)+2ull*i;
        if (nameOffset>0xffffffffULL || ordinalOffset>0xffffffffULL) return false;
        std::uint32_t nameRva=0;
        std::uint16_t ordinalIndex=0;
        if (!ReadProviderObject(backend,base,static_cast<std::uint32_t>(nameOffset),
                                expected.crtImageSize,nameRva) ||
            !ReadProviderObject(backend,base,static_cast<std::uint32_t>(ordinalOffset),
                                expected.crtImageSize,ordinalIndex)) return false;
        bool nameMatches=false;
        if (!ProviderNameEquals(backend,base,nameRva,expected.crtImageSize,
                                "_beginthreadex",nameMatches)) return false;
        if (!nameMatches) continue;
        if (found || ordinalIndex>=exports.NumberOfFunctions ||
            exports.Base>std::numeric_limits<std::uint32_t>::max()-ordinalIndex ||
            exports.Base+ordinalIndex!=expected.crtBeginThreadExOrdinal) return false;
        const auto functionOffset=static_cast<std::uint64_t>(exports.AddressOfFunctions)+4ull*ordinalIndex;
        if (functionOffset>0xffffffffULL) return false;
        std::uint32_t functionRva=0;
        if (!ReadProviderObject(backend,base,static_cast<std::uint32_t>(functionOffset),
                                expected.crtImageSize,functionRva) ||
            functionRva!=expected.crtBeginThreadExExportRva ||
            (functionRva>=directory.VirtualAddress &&
             functionRva-directory.VirtualAddress<directory.Size)) return false;
        found=true;
    }
    return found;
}

bool Win32ImageMapping(std::uint32_t base,std::uint32_t size) noexcept {
    if (!base || !size || base>std::numeric_limits<std::uint32_t>::max()-size) return false;
    // A loader may discard relocation pages after mapping. Authenticate the
    // image allocation at its header; actual headers/.text/IAT reads then
    // validate their own committed spans against this AllocationBase.
    MEMORY_BASIC_INFORMATION info{};
    return VirtualQuery(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(base)),
                        &info,sizeof(info))==sizeof(info) &&
        reinterpret_cast<std::uintptr_t>(info.AllocationBase)==base &&
        info.Type==MEM_IMAGE && info.State==MEM_COMMIT;
}

TbbIatCasResult Win32Cas(std::uint32_t base,std::uint32_t size,
                        std::uint32_t rva,std::uint32_t expected,
                        std::uint32_t desired,std::uint32_t& observed) noexcept {
    if (!InImage(rva,4,size) || (rva&3) ||
        base>std::numeric_limits<std::uint32_t>::max()-rva) return TbbIatCasResult::Failed;
    LONG old=0;
    __try {
        old=InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(
            static_cast<std::uintptr_t>(base)+rva),static_cast<LONG>(desired),
            static_cast<LONG>(expected));
    } __except(EXCEPTION_EXECUTE_HANDLER) {return TbbIatCasResult::Failed;}
    observed=static_cast<std::uint32_t>(old);
    return observed==expected ? TbbIatCasResult::Swapped : TbbIatCasResult::Mismatch;
}

} // namespace

const TbbImportExpectation& PinnedTbbImportExpectation() noexcept {
    static const TbbImportExpectation value{
        kPinnedTbbImageSize,0x4DB7F7BD,kPinnedTbbPreferredBase,
        kPinnedTbbTextRva,kPinnedTbbTextSize,0x16A00,
        kPinnedTbbBeginThreadExIatRva,142552,
        {0x22,0x80,0xE7,0xAC,0xDE,0xF5,0xE2,0xCC,0xF2,0x04,0x54,0xF1,0x17,0x56,0xA4,0x4E,
         0x2C,0xB3,0xE2,0xAF,0xD0,0x5D,0x6D,0x2D,0x0E,0xF5,0x83,0x6C,0x04,0xE1,0x2E,0x32},
        {0x8F,0x1D,0x1A,0x0D,0x2A,0x6E,0xF7,0xAB,0x06,0xDC,0x72,0x51,0x7F,0x7B,0xC3,0x07,
         0x42,0x5B,0xB3,0x54,0xB0,0x88,0x5C,0x77,0x6D,0xC6,0xAB,0x7A,0xFF,0xBC,0x31,0xD9},
        0xBF000,0x4DF2BE1E,0x5C601,469,773968,
        {0x87,0x93,0x35,0x34,0x61,0x82,0x6F,0xBD,0x48,0xF2,0x5E,0xA8,0xB8,0x35,0xBE,0x20,
         0x4B,0x75,0x8C,0xE7,0x51,0x0D,0xB2,0xAF,0x63,0x1B,0x28,0x85,0x03,0x55,0xBD,0x18},
        kPinnedTextRelocationDeltas,
        sizeof(kPinnedTextRelocationDeltas)/sizeof(kPinnedTextRelocationDeltas[0])
    };
    return value;
}

TbbImportBackend MakeWin32TbbImportBackend(
    std::uint32_t candidateTbbBase,std::uint32_t imageSize,
    std::function<void(std::uint32_t)> publish) {
    TbbImportBackend backend;
    backend.memory=MakeWin32ImageMemory(candidateTbbBase,imageSize);
    backend.retainFromAddress=[](std::uint32_t address,std::uint32_t& base) noexcept {
        HMODULE module=nullptr;
        if (!address || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            reinterpret_cast<LPCWSTR>(static_cast<std::uintptr_t>(address)),&module)) return false;
        base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(module));
        return true;
    };
    backend.modulePath=[](std::uint32_t base,wchar_t* path,std::size_t capacity) noexcept {
        if (!base || !path || capacity<2 || capacity>std::numeric_limits<DWORD>::max()) return false;
        const auto length=GetModuleFileNameW(reinterpret_cast<HMODULE>(
            static_cast<std::uintptr_t>(base)),path,static_cast<DWORD>(capacity));
        return length>0 && length<capacity && path[length]==L'\0';
    };
    backend.isMappedImage=[](std::uint32_t base,std::uint32_t size) noexcept {
        return Win32ImageMapping(base,size);
    };
    backend.resolveBeginThreadEx=[](std::uint32_t authenticatedCrtBase) noexcept -> std::uint32_t {
        return authenticatedCrtBase ? static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(
            GetProcAddress(reinterpret_cast<HMODULE>(
                static_cast<std::uintptr_t>(authenticatedCrtBase)),"_beginthreadex"))) : 0;
    };
    backend.readProvider=[](std::uint32_t base,std::uint32_t rva,
                            unsigned char* output,std::size_t count) {
        const auto memory=MakeWin32ImageMemory(base,PinnedTbbImportExpectation().crtImageSize);
        return memory.read(rva,output,count);
    };
    backend.publishOriginal=std::move(publish);
    backend.compareExchange=[candidateTbbBase,imageSize](std::uint32_t rva,
        std::uint32_t expected,std::uint32_t desired,std::uint32_t& observed) noexcept {
        return Win32Cas(candidateTbbBase,imageSize,rva,expected,desired,observed);
    };
    return backend;
}

TbbIatReport TbbImportTransaction::Run(std::uint32_t candidateTbbBase,
                                       std::uint32_t hook,
                                       const TbbImportBackend& backend,
                                       const TbbImportExpectation& expected) {
    TbbIatReport report;
    const auto prior=InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(&gate_),1,0);
    if (prior==1) {report.state=TbbIatState::Busy;report.reason="busy";return report;}
    if (prior==3) {
        report.state=TbbIatState::Poisoned;report.reason="poisoned";
        report.tbbReferenceRetained=retainedTbbBase_!=0;
        report.providerReferenceRetained=retainedProviderBase_!=0;
        report.originalPublished=originalPublished_;
        report.hookPossiblyExposed=hookPossiblyExposed_;
        return report;
    }
    if (prior==2) {
        if (InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(&gate_),1,2)!=2) {
            report.state=TbbIatState::Busy;report.reason="busy";return report;
        }
    }
    if (prior!=0 && prior!=2) {
        report.state=TbbIatState::Busy;report.reason="invalid or changing gate";return report;
    }
    GateExit gate{&gate_,prior==2 ? 2L : 0L};
    report.reason.reserve(512);
    report.tbbReferenceRetained=retainedTbbBase_!=0;
    report.providerReferenceRetained=retainedProviderBase_!=0;
    report.originalPublished=originalPublished_;
    report.hookPossiblyExposed=hookPossiblyExposed_;
    auto reject=[&](const char* reason,bool terminal=false) {
        report.reason=reason;
        report.state=(terminal || prior==2) ? TbbIatState::Poisoned : TbbIatState::Rejected;
        if (report.state==TbbIatState::Poisoned) gate.next=3;
        return report;
    };
    if (!candidateTbbBase && prior==0 && !retainedTbbBase_) {
        report.state=TbbIatState::Skipped;report.reason="TBB not yet loaded";return report;
    }
    if (!candidateTbbBase || !hook || expected.imageSize==0 ||
        expected.imageSize>64u*1024u*1024u ||
        !expected.diskSize || !expected.crtDiskSize ||
        !expected.textVirtualSize || expected.textVirtualSize>16u*1024u*1024u ||
        expected.textRelocationCount>4096 ||
        (expected.textRelocationCount && !expected.textRelocationDeltas) ||
        !InImage(expected.beginThreadExIatRva,4,expected.imageSize) ||
        (expected.beginThreadExIatRva&3) ||
        !backend.memory.read || !backend.memory.beginWrite ||
        !backend.memory.restoreProtection || !backend.retainFromAddress ||
        !backend.modulePath || !backend.isMappedImage ||
        !backend.resolveBeginThreadEx || !backend.readProvider ||
        !backend.publishOriginal || !backend.compareExchange)
        return reject("incomplete TBB identity or transaction backend",retainedTbbBase_!=0);
    if (retainedTbbBase_ && retainedTbbBase_!=candidateTbbBase)
        return reject("TBB module changed after reference retention",true);
    if (!retainedTbbBase_) {
        std::uint32_t retained=0;
        if (!backend.retainFromAddress(candidateTbbBase,retained))
            return reject("TBB module could not be retained");
        retainedTbbBase_=retained;
        report.tbbReferenceRetained=true;
        if (retained!=candidateTbbBase)
            return reject("retained TBB base differs from candidate",true);
    }
    if (prior==2) {
        // The retained reference pins the already-authenticated image. Do
        // not rehash the disk or reparse PE metadata on every main-loop retry.
        if (!originalPublished_ || !hookPossiblyExposed_ || hook!=installedHook_)
            return reject("installed TBB hook identity changed",true);
        if (!retainedProviderBase_) return reject("installed provider reference missing",true);
        const auto provider=backend.resolveBeginThreadEx(retainedProviderBase_);
        std::uint32_t slot=0;
        if (!provider || provider!=original_ || !backend.memory.read(
            expected.beginThreadExIatRva,reinterpret_cast<unsigned char*>(&slot),4) ||
            slot!=hook) return reject("installed TBB provider or slot changed",true);
        report.state=TbbIatState::AlreadyInstalled;
        report.reason="TBB import and provider verified";
        report.createThreadImportSkipped=true;
        report.hookObservedBeforeRecovery=true;
        return report;
    }
    if (!backend.isMappedImage(candidateTbbBase,expected.imageSize) ||
        !ReadDiskHash(backend,candidateTbbBase,expected.diskSize,expected.diskSha256))
        return reject("TBB mapping or pinned disk identity mismatch",true);
    IMAGE_NT_HEADERS32 nt{};
    std::uint32_t ntRva=0;
    bool createThreadAbsent=false;
    if (!ReadPeHeaders(backend.memory,expected.imageSize,nt,ntRva) ||
        nt.FileHeader.TimeDateStamp!=expected.timeDateStamp ||
        nt.OptionalHeader.ImageBase!=expected.preferredBase)
        return reject("mapped TBB PE header identity mismatch",true);
    if (!AuthenticateText(backend.memory,candidateTbbBase,expected,nt,ntRva))
        return reject("mapped TBB normalized text mismatch",true);
    if (!AuthenticateImports(backend.memory,expected,nt,createThreadAbsent))
        return reject("mapped TBB named import mismatch",true);
    report.createThreadImportSkipped=createThreadAbsent;
    // The active IAT DWORD is the only safe provider-origin starting point.
    // Retain and authenticate its owner before any GetProcAddress call, so an
    // untrusted named CRT export cannot trigger a forwarder load first.
    std::uint32_t current=0;
    if (!backend.memory.read(expected.beginThreadExIatRva,
        reinterpret_cast<unsigned char*>(&current),4) || !current || current==hook)
        return reject("foreign or unreadable TBB import before provider authentication",true);
    if (!retainedProviderBase_) {
        std::uint32_t retained=0;
        if (!backend.retainFromAddress(current,retained))
            return reject("active provider module could not be retained",true);
        retainedProviderBase_=retained;
        report.providerReferenceRetained=true;
    }
    if (!AuthenticateProvider(backend,retainedProviderBase_,current,expected))
        return reject("active MSVCR100 provider identity mismatch",true);
    const auto provider=backend.resolveBeginThreadEx(retainedProviderBase_);
    if (!provider || provider!=current || provider==hook)
        return reject("authenticated CRT export differs from active IAT",true);
    if (hookPossiblyExposed_ && provider!=original_)
        return reject("provider changed after possible hook exposure",true);
    if (!originalPublished_ || !hookPossiblyExposed_) {
        original_=provider;
        backend.publishOriginal(provider);
        originalPublished_=true;
    }
    report.originalPublished=true;
    std::uintptr_t originalToken=0;
    bool tokenOpen=false;
    bool attempted=false;
    bool byteRecovered=true;
    bool protectionRecovered=true;
    const char* failure=nullptr;
    if (!backend.memory.beginWrite(expected.beginThreadExIatRva,4,originalToken))
        failure="TBB IAT protection acquisition failed";
    else tokenOpen=true;
    if (!failure) {
        attempted=true;report.attempted=true;
        std::uint32_t observed=0;
        const auto outcome=backend.compareExchange(expected.beginThreadExIatRva,
            provider,hook,observed);
        if (outcome==TbbIatCasResult::Swapped || outcome==TbbIatCasResult::Failed ||
            (outcome==TbbIatCasResult::Mismatch && observed==hook)) {
            hookPossiblyExposed_=true;report.hookPossiblyExposed=true;
        }
        std::uint32_t after=0;
        if (!backend.memory.read(expected.beginThreadExIatRva,
                                 reinterpret_cast<unsigned char*>(&after),4))
            failure="TBB IAT readback failed";
        else {
            if (after==hook) {
                hookPossiblyExposed_=true;report.hookPossiblyExposed=true;
                report.hookObservedBeforeRecovery=true;
            }
            if (outcome!=TbbIatCasResult::Swapped || after!=hook)
                failure=outcome==TbbIatCasResult::Mismatch ?
                    "TBB IAT CAS foreign mismatch" : "TBB IAT CAS/readback mismatch";
        }
    }
    if (!failure) {
        if (!backend.memory.restoreProtection(expected.beginThreadExIatRva,4,originalToken))
            failure="TBB IAT final protection restoration failed";
        else tokenOpen=false;
    }
    if (!failure) {
        installedHook_=hook;gate.next=2;
        report.state=TbbIatState::Applied;
        report.reason="active TBB _beginthreadex import installed";
        return report;
    }
    if (attempted) {
        std::uintptr_t rollbackToken=0;
        if (!backend.memory.beginWrite(expected.beginThreadExIatRva,4,rollbackToken)) {
            byteRecovered=false;protectionRecovered=false;
        } else {
            std::uint32_t at=0;
            if (!backend.memory.read(expected.beginThreadExIatRva,
                                     reinterpret_cast<unsigned char*>(&at),4))
                byteRecovered=false;
            else if (at==hook) {
                std::uint32_t observed=0;
                if (backend.compareExchange(expected.beginThreadExIatRva,
                    hook,provider,observed)!=TbbIatCasResult::Swapped) byteRecovered=false;
            } else if (at!=provider) byteRecovered=false;
            std::uint32_t check=0;
            if (!backend.memory.read(expected.beginThreadExIatRva,
                 reinterpret_cast<unsigned char*>(&check),4) || check!=provider)
                byteRecovered=false;
            else if (byteRecovered) report.originalVerifiedAfterRecovery=true;
            if (!backend.memory.restoreProtection(expected.beginThreadExIatRva,4,rollbackToken))
                protectionRecovered=false;
        }
    }
    if (tokenOpen && !backend.memory.restoreProtection(
        expected.beginThreadExIatRva,4,originalToken)) protectionRecovered=false;
    std::uint32_t finalValue=0;
    if (!backend.memory.read(expected.beginThreadExIatRva,
         reinterpret_cast<unsigned char*>(&finalValue),4) || finalValue!=provider)
        byteRecovered=false;
    report.reason=failure;
    report.reason.append(byteRecovered && protectionRecovered ?
        "; original and protection restored" : "; recovery incomplete");
    if (byteRecovered && protectionRecovered) {
        report.state=TbbIatState::RolledBack;gate.next=0;
    } else {
        report.state=TbbIatState::RollbackFailed;gate.next=3;
    }
    return report;
}

} // namespace fork_boundary
