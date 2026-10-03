//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

// trace ignore
namespace bot {

enum TraceIgnore : int32_t {
  None = 0,
  Glass = ystl::bit (0),
  Monsters = ystl::bit (1),
  Everything = Glass | Monsters
};
YSTL_ENABLE_ENUM_FLAGS (TraceIgnore);

// global trace wrapper
class Trace final : public ystl::Singleton<Trace> {
public:
  Trace () {}
  ~Trace () = default;

public:
  // binary-compatible with engine traceresult
  struct Result {
    int all_solid {};
    int start_solid {};
    int in_open {};
    int in_water {};
    float fraction {};
    vec3_t end_pos {};
    float plane_dist {};
    vec3_t plane_normal {};
    edict_t *hit {};
    int hit_group {};
  };

private:
  // cache constants
  static constexpr int kCacheSize = 128;
  static constexpr float kCacheTTL = 0.1f;

  struct CacheKey {
    float start_x, start_y, start_z;
    float end_x, end_y, end_z;
    int ignore_flags;
    int16_t hull_number;
    edict_t *ignore_entity;

    bool Matches (const CacheKey &other) const {
      return start_x == other.start_x && start_y == other.start_y && start_z == other.start_z && end_x == other.end_x && end_y == other.end_y &&
             end_z == other.end_z && ignore_flags == other.ignore_flags && hull_number == other.hull_number &&
             ignore_entity == other.ignore_entity;
    }
  };

  // single cache entry
  struct CacheEntry {
    CacheKey key {};
    Result result {};
    float timestamp {};
    uint32_t last_used {};
    bool valid {};
  };

  ystl::FixedArray<CacheEntry, kCacheSize> cache_ {};
  uint32_t lru_counter_ {};

  int hits_ {};
  int misses_ {};

  CacheEntry *FindInCache (const CacheKey &key, float now);
  void StoreInCache (CacheEntry *entry, const CacheKey &key, const Result &result, float now);

  static bool IsCacheable (const Result &result);

public:
  // trace line
  void Line (const ystl::Vector &start, const ystl::Vector &end, int ignore_flags, edict_t *ignore_entity, Result *ptr);

  // trace model
  void Model (const ystl::Vector &start, const ystl::Vector &end, int hull_number, edict_t *ent_to_hit, Result *ptr);

  // trace hull
  void Hull (const ystl::Vector &start, const ystl::Vector &end, int ignore_flags, int hull_number, edict_t *ignore_entity, Result *ptr);

  // check if trace endpoint is in open space (not inside non-solid brushes)
  bool IsEndpointClear (const Result &result) const;

  // cache statistics
  int GetHits () const {
    return hits_;
  }

  int GetMisses () const {
    return misses_;
  }

  float GetHitRate () const {
    auto total = hits_ + misses_;
    return total > 0 ? static_cast<float> (hits_) / total * 100.0f : 0.0f;
  }
};

// static assert binary compatibility
static_assert (sizeof (Trace::Result) == sizeof (TraceResult), "Trace::Result must be binary-compatible with TraceResult");

// expose globals
YSTL_EXPOSE_GLOBAL_SINGLETON (Trace, trace);

} // namespace bot
