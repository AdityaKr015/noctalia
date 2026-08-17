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
        .value =
            playerName.empty() ? std::to_string(percent) + "%" : playerName + " - " + std::to_string(percent) + "%",
        .progress = static_cast<float>(std::clamp(volume, 0.0, 1.0)),
        .overLimit = percent > 100,
    };
  }

} // namespace

void MediaOsd::bindOverlay(OsdOverlay& overlay) { m_overlay = &overlay; }

void MediaOsd::onMprisChanged(const MprisService& service) {
  const auto activePlayerOpt = service.activePlayer();
  if (activePlayerOpt.has_value()) {
    const auto& activePlayer = activePlayerOpt.value();
    const MediaOsdData osdData = {.title = activePlayer.title, .artist = joinedArtists(activePlayer.artists)};

    // Show an OSD when the active player changes its track while playing, or when the OSD is first initialized.
    if (!m_hasData) {
      m_lastData = osdData;
      m_hasData = true;
    } else if (activePlayer.playbackStatus == "Playing" && osdData != m_lastData) {
      m_lastData = osdData;
      if (m_overlay != nullptr) {
        m_overlay->show(makeMprisContent(osdData));
      }
    }
  }

  // Show an OSD when any player's volume changes, even if it's not the active player.
  for (const auto& [busName, player] : service.players()) {
    const auto it = m_lastVolumes.find(busName);
    if (it == m_lastVolumes.end()) {
      m_lastVolumes.emplace(busName, player.volume);
      continue;
    }
    if (std::abs(player.volume - it->second) <= kVolumeChangeEpsilon) {
      continue;
    }
    it->second = player.volume;
    if (m_overlay != nullptr) {
      m_overlay->show(makeVolumeContent(player.identity, player.volume));
    }
  }

  // Remove any cached volume entries for players that have disappeared.
  if (m_lastVolumes.size() != service.players().size()) {
    std::erase_if(m_lastVolumes, [&](const auto& entry) { return !service.players().contains(entry.first); });
  }
}
