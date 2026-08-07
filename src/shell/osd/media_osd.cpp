#include "shell/osd/media_osd.h"

#include "dbus/mpris/mpris_service.h"
#include "shell/osd/osd_overlay.h"

#include <algorithm>
#include <cmath>

namespace {

  constexpr double kVolumeChangeEpsilon = 0.003;

  OsdContent makeMprisContent(const MediaOsdData& data) {
    return OsdContent{
        .kind = OsdKind::Media,
        .icon = "disc-filled",
        .value = data.artist.empty() ? data.title : data.title + " — " + data.artist,
        .showProgress = false,
    };
  }

  OsdContent makeVolumeContent(std::string playerName, double volume) {
    const int percent = static_cast<int>(std::round(std::max(0.0, volume) * 100.0));
    return OsdContent{
        .kind = OsdKind::Media,
        .icon = "disc-filled",
        .value = playerName.empty() ? std::to_string(percent) + "%" : playerName + " — " + std::to_string(percent) + "%",
        .progress = static_cast<float>(std::clamp(volume, 0.0, 1.0)),
        .overLimit = percent > 100,
    };
  }

} // namespace

void MediaOsd::bindOverlay(OsdOverlay& overlay) { m_overlay = &overlay; }

void MediaOsd::onMprisChanged(const MprisService& service) {
  const auto activePlayerOpt = service.activePlayer();
  if (!activePlayerOpt.has_value()) {
    return;
  }
  const auto& activePlayer = activePlayerOpt.value();
  const MediaOsdData osdData = {.title = activePlayer.title, .artist = joinedArtists(activePlayer.artists)};
  const double volume = activePlayer.volume;

  // First snapshot seeds the baseline; it is not a user-visible transition.
  if (!m_hasData) {
    m_lastData = osdData;
    m_lastVolume = volume;
    m_hasData = true;
    return;
  }

  const bool trackChanged = activePlayer.playbackStatus == "Playing" && osdData != m_lastData;
  const bool volumeChanged = std::abs(volume - m_lastVolume) > kVolumeChangeEpsilon;

  if (trackChanged) {
    m_lastData = osdData;
  }
  if (volumeChanged) {
    m_lastVolume = volume;
  }

  if (m_overlay == nullptr) {
    return;
  }

  // Track-change OSD keeps priority when both change in the same event.
  if (trackChanged) {
    m_overlay->show(makeMprisContent(osdData));
  } else if (volumeChanged) {
    m_overlay->show(makeVolumeContent(activePlayer.identity, volume));
  }
}
