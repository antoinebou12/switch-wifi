#pragma once

#ifndef SWITCH_WIFI_VERSION
#define SWITCH_WIFI_VERSION "0.3.0-dev"
#endif

#ifndef SWITCH_WIFI_ATMOSPHERE_TARGET
#define SWITCH_WIFI_ATMOSPHERE_TARGET "1.11.2"
#endif

#ifndef SWITCH_WIFI_HOS_TARGET
#define SWITCH_WIFI_HOS_TARGET "22.5.0"
#endif

#ifndef SWITCH_WIFI_LIBNX_BASELINE
#define SWITCH_WIFI_LIBNX_BASELINE "4.10.0+"
#endif

namespace swifi::build {
inline constexpr const char* version = SWITCH_WIFI_VERSION;
inline constexpr const char* atmosphereTarget = SWITCH_WIFI_ATMOSPHERE_TARGET;
inline constexpr const char* hosTarget = SWITCH_WIFI_HOS_TARGET;
inline constexpr const char* libnxBaseline = SWITCH_WIFI_LIBNX_BASELINE;
} // namespace swifi::build
