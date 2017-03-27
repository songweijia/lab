#ifndef UTIL_HPP
#define UTIL_HPP

#ifdef _DEBUG
#include <spdlog/spdlog.h>
#endif//_DEBUG

#ifdef _DEBUG
  inline auto dbgConsole() {
    static auto console = spdlog::stdout_color_mt("console");
    return console;
  }
  #define dbg_info(...) dbgConsole()->info(__VA_ARGS__)
  #define dbg_warn(...) dbgConsole()->warn(__VA_ARGS__)
  #define dbg_crit(...) dbgConsole()->critical(__VA_ARGS__)
#else
  #define dbg_info(...)
  #define dbg_warn(...)
  #define dbg_crit(...)
#endif//_DEBUG


#endif//UTIL_HPP
