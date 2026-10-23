#pragma once

#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4201)
#pragma warning(disable: 4100)
#pragma warning(disable: 4244)
#endif
#include "clay.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace padv::ui {

class FrameStringArena {
 public:
  static auto instance() -> FrameStringArena& {
    static FrameStringArena s_instance;
    return s_instance;
  }

  auto clear() -> void {
    strings_.clear();
  }

  auto intern(std::string_view sv) -> Clay_String {
    strings_.emplace_back(sv);
    const auto& s = strings_.back();
    return Clay_String{false, static_cast<int32_t>(s.size()), s.data()};
  }

  template <typename... Args>
  auto format(std::format_string<Args...> fmt, Args&&... args) -> Clay_String {
    strings_.push_back(std::format(fmt, std::forward<Args>(args)...));
    const auto& s = strings_.back();
    return Clay_String{false, static_cast<int32_t>(s.size()), s.data()};
  }

 private:
  std::vector<std::string> strings_;
};

inline auto intern_str(std::string_view sv) -> Clay_String {
  return FrameStringArena::instance().intern(sv);
}

template <typename... Args>
inline auto format_str(std::format_string<Args...> fmt, Args&&... args) -> Clay_String {
  return FrameStringArena::instance().format(fmt, std::forward<Args>(args)...);
}

}
