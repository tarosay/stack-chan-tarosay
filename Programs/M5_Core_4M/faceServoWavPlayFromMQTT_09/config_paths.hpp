#ifndef CONFIG_PATHS_HPP
#define CONFIG_PATHS_HPP

#include <stdint.h>

namespace config_paths {

// SD上の設定JSON（将来増える前提でここに集約）
inline constexpr const char* kBasicConfig = "/json/SC_BasicConfig.json";
inline constexpr const char* kSecConfig = "/json/SC_SecConfig.json";
}

#endif  //CONFIG_PATHS_HPP