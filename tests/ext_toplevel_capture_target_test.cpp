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
    const auto editor = uniqueExtHandleForTitle(windows, "Editor");
    const auto caseMismatch = uniqueExtHandleForTitle(windows, "terminal");
    const auto missing = uniqueExtHandleForTitle(windows, "Missing");
    return expect(editor.handle == handleA && editor.matchCount == 1, "unique title should resolve to its handle")
        && expect(caseMismatch.handle == nullptr && caseMismatch.matchCount == 0, "title match should be exact")
        && expect(missing.handle == nullptr && missing.matchCount == 0, "unknown title should not resolve");
  }

  bool rejectsAmbiguity() {
    const std::vector<ToplevelInfo> windows{
        window(handleA, "Editor"),
        window(handleB, "Editor"),
    };
    const auto ambiguous = uniqueExtHandleForTitle(windows, "Editor");
    const auto empty = uniqueExtHandleForTitle(windows, "");
    return expect(
               ambiguous.handle == nullptr && ambiguous.matchCount == 2, "two same-title windows should be ambiguous"
           )
        && expect(empty.handle == nullptr && empty.matchCount == 2, "empty title should not resolve among many");
  }

  bool preferredRankResolveAmbiguity() {
    const std::vector<ToplevelInfo> windows{
        window(handleA, "Editor"),
        window(handleB, "Editor"),
    };
    const std::vector<ToplevelInfo> solo{window(handleA, "Terminal")};
    const auto first = uniqueExtHandleForTitle(windows, "Editor", 0);
    const auto second = uniqueExtHandleForTitle(windows, "Editor", 1);
    const auto skew = uniqueExtHandleForTitle(windows, "Editor", 2);
    const auto soloMatch = uniqueExtHandleForTitle(solo, "Terminal", 7);
    const auto emptyTitled = uniqueExtHandleForTitle(windows, "", 1);
    return expect(first.handle == handleA && first.matchCount == 2, "rank 0 should pick the first sibling")
        && expect(second.handle == handleB && second.matchCount == 2, "rank 1 should pick the second sibling")
        && expect(skew.handle == nullptr && skew.matchCount == 2, "out-of-range rank should keep the ambiguity error")
        && expect(soloMatch.handle == handleA && soloMatch.matchCount == 1, "unique match should ignore the rank")
        && expect(emptyTitled.handle == handleB && emptyTitled.matchCount == 2, "empty title should honor the rank");
  }

  bool ignoresEntriesWithoutExtHandle() {
    const std::vector<ToplevelInfo> windows{
        window(nullptr, "Editor"),
        window(handleA, "Editor"),
    };
    const auto match = uniqueExtHandleForTitle(windows, "Editor");
    return expect(match.handle == handleA && match.matchCount == 1, "wlr-only entries should be ignored");
  }

  bool orphanFallbackResolveWhenAppBucketMisses() {
    const std::vector<ToplevelInfo> appBucketed{};
    const std::vector<ToplevelInfo> orphans{window(handleA, "Steam")};
    const auto match = focusedExtHandleForTitle(appBucketed, orphans, "Steam", 0);
    return expect(
        match.handle == handleA && match.matchCount == 1, "app-id-less bucket should resolve an app bucket miss"
    );
  }

  bool orphanFallbackPreserveAmbiguity() {
    const std::vector<ToplevelInfo> appBucketed{};
    const std::vector<ToplevelInfo> orphans{window(handleA, "Steam"), window(handleB, "Steam")};
    const auto noRank = focusedExtHandleForTitle(appBucketed, orphans, "Steam", kNoPreferredRank);
    const auto inRange = focusedExtHandleForTitle(appBucketed, orphans, "Steam", 1);
    return expect(noRank.handle == nullptr && noRank.matchCount == 2, "ambiguous orphans without a rank should refuse")
        && expect(inRange.handle == handleB && inRange.matchCount == 2, "rank should still select among ambiguous");
  }

  bool appBucketWinsOverOrphans() {
    const std::vector<ToplevelInfo> appBucketed{window(handleA, "Steam")};
    const std::vector<ToplevelInfo> orphans{window(handleB, "Steam")};
    const auto match = focusedExtHandleForTitle(appBucketed, orphans, "Steam", 0);
    return expect(match.handle == handleA && match.matchCount == 1, "app bucket should win over the orphan fallback");
  }

  bool emptySelection() {
    const std::vector<ToplevelInfo> windows{window(handleA, "Editor")};
    const auto only = uniqueExtHandleForTitle(windows, "");
    const auto none = uniqueExtHandleForTitle({}, "Editor");
    return expect(only.handle == handleA && only.matchCount == 1, "empty title accepts the only candidate")
        && expect(none.handle == nullptr && none.matchCount == 0, "no candidates should not resolve");
  }
} // namespace

int main() {
  bool ok = true;
  ok = selectsUniqueTitleMatch() && ok;
  ok = rejectsAmbiguity() && ok;
  ok = preferredRankResolveAmbiguity() && ok;
  ok = ignoresEntriesWithoutExtHandle() && ok;
  ok = emptySelection() && ok;
  ok = orphanFallbackResolveWhenAppBucketMisses() && ok;
  ok = orphanFallbackPreserveAmbiguity() && ok;
  ok = appBucketWinsOverOrphans() && ok;
  return ok ? 0 : 1;
}
