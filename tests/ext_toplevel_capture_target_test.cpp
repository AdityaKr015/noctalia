#include "wayland/ext_foreign_toplevels.h"

#include <iostream>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
  bool expect(bool condition, const char* message) {
    if (!condition) {
      std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
  }

  int handleTokenA = 0;
  int handleTokenB = 0;

  ext_foreign_toplevel_handle_v1* const handleA = reinterpret_cast<ext_foreign_toplevel_handle_v1*>(&handleTokenA);
  ext_foreign_toplevel_handle_v1* const handleB = reinterpret_cast<ext_foreign_toplevel_handle_v1*>(&handleTokenB);

  [[nodiscard]] ToplevelInfo window(ext_foreign_toplevel_handle_v1* extHandle, std::string title) {
    return ToplevelInfo{
        .title = std::move(title),
        .appId = "app",
        .extHandle = extHandle,
    };
  }

  bool selectsUniqueTitleMatch() {
    const std::vector<ToplevelInfo> windows{
        window(handleA, "Editor"),
        window(handleB, "Terminal"),
    };
    return expect(uniqueExtHandleForTitle(windows, "Editor") == handleA, "unique title should resolve to its handle")
        && expect(uniqueExtHandleForTitle(windows, "terminal") == nullptr, "title match should be exact")
        && expect(uniqueExtHandleForTitle(windows, "Missing") == nullptr, "unknown title should not resolve");
  }

  bool rejectsAmbiguity() {
    const std::vector<ToplevelInfo> windows{
        window(handleA, "Editor"),
        window(handleB, "Editor"),
    };
    return expect(uniqueExtHandleForTitle(windows, "Editor") == nullptr, "two same-title windows should be ambiguous")
        && expect(uniqueExtHandleForTitle(windows, "") == nullptr, "empty title should not resolve among many");
  }

  bool ignoresEntriesWithoutExtHandle() {
    const std::vector<ToplevelInfo> windows{
        window(nullptr, "Editor"),
        window(handleA, "Editor"),
    };
    return expect(uniqueExtHandleForTitle(windows, "Editor") == handleA, "wlr-only entries should be ignored");
  }

  bool emptySelection() {
    const std::vector<ToplevelInfo> windows{window(handleA, "Editor")};
    return expect(uniqueExtHandleForTitle(windows, "") == handleA, "empty title accepts the only candidate")
        && expect(uniqueExtHandleForTitle({}, "Editor") == nullptr, "no candidates should not resolve");
  }
} // namespace

int main() {
  bool ok = true;
  ok = selectsUniqueTitleMatch() && ok;
  ok = rejectsAmbiguity() && ok;
  ok = ignoresEntriesWithoutExtHandle() && ok;
  ok = emptySelection() && ok;
  return ok ? 0 : 1;
}
