//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

// debug panel channels, laid out to match the classic racc/parabot ai console
namespace bot {

// full definitions live in behavior.h, kept out to avoid include cycles
enum class AimFlags : uint32_t;
enum class Sense : uint32_t;

enum class DebugChannel : int32_t {
  Eyes = 0, // what the bot sees
  Ears, // what the bot hears
  Body, // obstacle / stuck state
  Legs, // movement keys and speeds
  Hand, // view angles and weapon
  Chat, // chat / radio intents
  Cognition, // personality / task / goals
  Navigation, // node, goal, path
  Count
};

// on-screen column the channel is rendered into
enum class DebugColumn : int32_t {
  Left = 0, // eyes, ears, body
  Center, // cognition, navigation
  Right // legs, hand, chat
};

// multi-panel on-screen debug console (single server-wide instance)
class DebugPanel final : public ystl::Singleton<DebugPanel> {
public:
  enum : size_t {
    Rows = 6, // rendered rows per channel
    Columns = 3 // left / center / right screen columns
  };

  static constexpr float kRefreshInterval = 0.5f; // overlay resend period
  static constexpr float kHoldTime = kRefreshInterval * 2.0f; // hud lifetime, overlaps resends

private:
  // captured text lines, one row buffer per channel and row
  ystl::String rows_[ystl::to_underlying (DebugChannel::Count)][Rows];

  // composed and reused panel buffers, sent as three hud messages
  ystl::String panels_[Columns];

  // column assignment and title for each channel
  struct ChannelInfo {
    DebugChannel channel {};
    DebugColumn column {};
    ystl::StringRef title {};
  };

  static constexpr ChannelInfo kChannels[] {
    { DebugChannel::Eyes,       DebugColumn::Left,   "[EYES]"       },
    { DebugChannel::Ears,       DebugColumn::Left,   "[EARS]"       },
    { DebugChannel::Body,       DebugColumn::Left,   "[BODY]"       },
    { DebugChannel::Legs,       DebugColumn::Right,  "[LEGS]"       },
    { DebugChannel::Hand,       DebugColumn::Right,  "[HAND]"       },
    { DebugChannel::Chat,       DebugColumn::Right,  "[CHAT]"       },
    { DebugChannel::Cognition,  DebugColumn::Center, "[COGNITION]"  },
    { DebugChannel::Navigation, DebugColumn::Center, "[NAVIGATION]" },
  };

public:
  DebugPanel () = default;
  ~DebugPanel () = default;

public:
  // write a formatted line into a channel row (mirrors aiconsole_printf)
  template <typename... Args> void Set (DebugChannel channel, int row, const char *fmt, Args &&...args) {
    if (row < 0 || static_cast<size_t> (row) >= Rows) {
      return;
    }
    rows_[ystl::to_underlying (channel)][row].assignf (fmt, ystl::forward<Args> (args)...);
  }

  // push a plain string line into a channel row
  void SetString (DebugChannel channel, int row, ystl::StringRef text) {
    if (row < 0 || static_cast<size_t> (row) >= Rows) {
      return;
    }
    rows_[ystl::to_underlying (channel)][row].assign (text.chars (), text.size ());
  }

  // compose the panels from the captured rows and send them as hud text
  void Render (edict_t *overlay_entity, ystl::StringRef spectating_name);

  // clear every captured row (called after render)
  void Reset ();

  // print the whole panel set for a given bot's live state
  void Update (Bot *bot);

private:
  // human-readable name for a noise flag combination
  static ystl::StringRef NoiseName (Noise noise);

  // human-readable name for a personality
  static ystl::StringRef PersonalityName (Personality personality);

  // compact list of active movement/action buttons
  static ystl::String KeysName (int buttons);

  // compact list of active aim flags
  static ystl::String AimFlagsName (AimFlags flags);

  // compact list of active sense flags
  static ystl::String SenseName (Sense states);
};

// expose the global debug panel
YSTL_EXPOSE_GLOBAL_SINGLETON (DebugPanel, botdebug);

} // namespace bot
