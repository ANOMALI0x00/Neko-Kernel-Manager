#ifndef CONFIG_OPTIONS_HPP
#define CONFIG_OPTIONS_HPP

#include <string>

extern "C" {
    int apply_config_options(const char* json, const char* config_path);
}

#endif // CONFIG_OPTIONS_HPP