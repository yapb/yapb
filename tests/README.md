# YaPB test host

Fake engine executable + fake cs gamedll. Tests only, native gcc/clang/msvc,
x64. No std::* in harness code (ystl only).

## Layout

- `host/fake_engine.{h,cpp}` — fake GoldSource engine: `enginefuncs_t` +
  `globalvars_t`, edict pool, string table, cvars, scriptable traces,
  `initialise()` resets everything (foolsgoldsource-inspired).
- `host/fake_cs/fake_cs.cpp` — fake cs gamedll (`fake_cs.so` / `fake_cs.dll`):
  baseline `gamefuncs_t` that yapb intercepts via `GetEntityAPI`. Logs every
  call, observable through `host/fake_cs_api.h` via the platform loader.
- `host/host_main.cpp` — ystl framework runner (`--list-tests`, filter).
- `host/test_host.h` — shared helpers: `FakeCSApi` resolution,
  `HOST_REQUIRE`, entry-point declarations, `testhost::pumpBots`,
  `testhost::findBot`, `testhost::csCalls`, `installEngineTables`.
- `host/scenarios/` — one file per scenario (`TEST_CASE`):
  - `boot/full_cycle` — full standalone lifecycle smoke.
  - `forward/hooks` — gamedll call forwarding + engine order.
  - `gamedll/matrix_{legacy,modern,regame}` — gamedll detection matrix:
    version banner + runtime flags + conditional hook presence,
    each against its own staged fake gamedll (`gamedir[-variant]`).
  - `tier0/weapon_data` — pure data invariants without a server lifecycle
    (weapon table, spot prices, id roundtrips, gun bitmasks).
  - `unit/analyze_cleaner` — `ConnectionCleaner` on synthetic graphs:
    bridge veto, triplet detour removal (reachability preserved),
    jump-link protection. Lightest prefix (engine tables only).
- `unit/engine_{tables,entities,round}` — per-file coverage for
  engine.cpp: string interop, trace cache + TTL, visibility sets
  (tables); entity classifiers, origins, breakable matrix, bomb/vip,
  teams, entity search (entities); round/bomb timing (round).
  - `unit/engine_light` — `LightMeasure` on a synthetic single-node BSP
    (modern staging): style init/animation, lightmap sampling with a
    proportional gray probe, sky color averaging. No server lifecycle.
  - `unit/engine_{game,cvars,state}` — the rest of engine.cpp without
    skips: flags, spawns, fake clients, bot commands, engine commands,
    sounds, prints, hud/beam messages, fixed-point helpers, cvar reads,
    developer mode, healthy-environment revert (game); typed ConVar
    accessors, ConVarRef, bounds checker, cvar descriptions (cvars);
    round flags, grenade/interesting tracking with flag gating,
    detonation tracker, hitbox enumeration with caching (state).
- `unit/navigate_{path,goals,move,gaps,cover,bomb}` — per-file coverage for navigate.cpp
  on synthetic graphs with a live Bot (`BotNavigateHook` friend, tests only):
    shortest/sync paths, node queries, button lookup, goal posts/tactics,
    bomb/hostage branches (path, goals); blockage/strafe/jump/duck/wall
    predicates, fall safety, stuck detection, collision-weight math,
    strafe commits (move).
  - `scenario/navigate_walk` — hook-driven path draining: sync path,
    node-by-node `advanceMovement` with harness physics, input
    translation, search-state reset. `HOST_REQUIRE` evaluates once
    (actions must never run twice).
  - `unit/manager_{create,kicks,teams}` — per-file coverage for
    manager.cpp through the public API only (no production changes):
    creation queue incl. GraphError/TeamStacked, lookups, forEach,
    dead-first/lowest-frags kicks, team-scoped kicks, quota math,
    census/KPD, stacking, priorities, economics, difficulties, weapon
    modes, say/radio capture, death handling, frame() smoke.
    Quota stays above the live count so the auto-balancer never fires
    mid-setup; manual adds increment it,     dead-first reselects stale
    bots (disconnect first). `unit/manager_{maintain,leaders_*}` covers
    idle-quota kicks, serverFill queueing with limit drops, leader
    election incl. VIP/C4 map branches, and inline worker tasks.
    Needs `BotManagerHook` (BotBehaviorData). Map branches need
    pre-activation map entities (levelInitialize scans them).
    Regression guards for fixed prod bugs:
    sparse in-place decay (Twin aliasing), rebuild-window write
    protection, sub-20 full no-op.
  - `unit/chatlib_{text,bot}` — per-file coverage for chatlib.cpp: tag
    stripping table, reply-use tracking, error bounds, keyword matching
    with cycle reset on an injected factory, full marker expansion
    (`%f %s %v %t %e %m %r %g`, unknown/trailing/cap), reply flow with
    queue effects, send guards and legacy broadcast. Needs `BotChatHook`
    (BotChatData, BotBehaviorData). Regression guards for fixed prod
    bugs: `%v` with no victim renders "unknown" (was a null-deref
    through indexOfPlayer into ent->free). Damage dispatch pins the
    (armor, health, bits) argument order into takeDamage.
    `m_blindNodeIndex` defaults out of range (was 0, a valid node:
    Normal blinds navigated to node 0 since `blind_` accepts
    `>= Normal` but `takeBlind` assigns cover only above it).
  - `unit/vision_{angles,sees,aim,nav}` — per-file coverage for
    vision.cpp: FOV math with wrap, view cone, body angles with frustum
    refresh, frustum planes directly, item/C4/entity visibility incl.
    blocked classname and tolerance bands (traces cache 0.1s: advance
    between scripted phases), darkness guards with flashlight/NVG
    commands, dead-predict clearing, live predict landing, danger
    glances and walk lookahead on a synced walk. Needs `BotVisionHook`
    (Bot, BotVisionData, BotNavigateData, BotWeaponsData, BotCombatData).
  - `unit/funmode_modes` — per-file coverage for funmode.cpp through
    the singleton only (no hook): keyword tables, setters, transitions
    with gravity save/restore, tron/haunted/dark/newyear/stoned/mars
    application (ghosts iterate Bots, tron iterates clients).
  - `unit/radio_{orders,chatter}` — per-file coverage for radio.cpp:
    busy guards, follow recruit with overfull pruning, hold/round/
    fire/spotted/go/blow/regroup/storm/fallback/report/sector/position
    orders with exact state effects, chatter guards, kill tuning,
    enemy-count calls, frame bomb warning, head-toward, push guards.
    Percent rolls pin at 100 via the hook; literal-chance branches
    (MoveTo/Attack/chatter-change) stay out. Needs `BotRadioHook`
    (Bot, BotRadioData, BotBehaviorData). Notes: overfull prune stops
    at the headcount (first follower drops). Regression guards for
    fixed prod bugs: null victim in `handleChatterOnPlayerKill` falls
    back to the headcount switch (was a SEGV on the 72% path,
    verified by reverting).
    `unit/behavior_map` boots map entities pre-activation for
    Demolition/Hostage/breakable branches (hostage rules, breakable
    touch task, bomb timer, escape override); `unit/behavior_frame`
    runs a live bot through update/logic. Fixture rule: stale live
    edicts with default Spectator teams get tracked as enemies, so
    census/lookup tests pin teams and kill or isolate extras.
  - `unit/behavior_logic` — frame core: one `logic()` pass pins the
    movement contract, reaction/view clamps and fire state; parachute
    folded, null drops, double-jump naming, blind clearing, creature
    paint via infected flag, debug overlay through a watching editor,
    C4 donate guard and full path, defuse presence checks, camp
    direction fallback and level-4 debug chatter. Notes: pumped bots
    join as spectators, assign teams; fake_cs kill is record-only;
    `SetClientKeyValue` is a no-op so model masks stay clear.
  - `unit/behavior_{queue,senses,damage}` — per-file coverage for
    behavior.cpp: Say fan-out with alive filtering, dry-Buy dispatch,
    reaction timers (3 modes), ignore list, camp direction (clear +
    practice index), camp buttons, shift speed legs, heavy timer,
    hearing with preference scoring and sound memory, hostage/bomb
    guards, C4 donate, follow-user, full takeDamage matrix (stamps,
    creature, aggression/fear, camp clear, practice landing, teamkill).
    Needs `BotBehaviorHook` (Bot, BotBehaviorData, BotNavigateData).
    Newborn timers (buy/heavy) always outlive pump: advance first.
  - `unit/combat_{senses,fire}` — per-file coverage for combat.cpp:
    friend/foe census, rendering/invincibility/notarget matrices,
    darkness rules incl. threat override, groups, knife/grenade modes,
    zeroed     thru-wall, exact throw ballistics with clamps, surface
    scale, body-part visibility, sight/remember/friend/threat/react
    chains. Needs `BotCombatHook` (Bot, BotCombatData, BotBehaviorData,
    BotNavigateData). Fresh clients start zeroed, bring up solid/takedamage;
    `isEnemyInSight` is retained API covered via the hook. `unit/combat_watch`
    covers enemy lookup/tracking, team radio orders, grenade-velocity
    miss, custom heights, and focused fire (frustum needs
    updateBodyAngles first; fresh frustums pass everything).
    Regression guards for fixed prod bugs: open-sky toss arcs over the
    chord (raw midpoint failed validation, everything was null).
- `unit/tasks_{core,basic,throw,break,duty,drills}` — per-file coverage
  for tasks.cpp: desire arbiter wins (blind/pickup/hunt/attack),
  priority order with pop restore, empty-complete safety, pause/spray/
  blind/hide/camp/escape/follow/doublejump/pickup null-legs, plant
  entries, defuse thin-air and on-bar paths, grenade dispatch (self-blow
  guard, flag legs, owned select), breakable shooting with and
  without ammo (map boot registers the breakable), hunt drops; drills
  cover empty-state legs per task body (seek/attack/follow/doublejump/
  escape/pickup/camp/hide/blind/spray/hunt/clear/defuse), debug-goal
  adoption, move-target adoption, follow parking and camp watches.
  Needs `BotTasksHook` (Bot, BotBehaviorData, BotCombatData,
  BotWeaponsData, BotNavigateData). Notes: blind index defaults out
  of range post-fix; clip lookup keys off conf ids; gamedef
  classnames are empty in-harness; duty needs a Demolition map boot
  or origins never land. Regression guards for fixed prod bugs:
  open-sky toss arcs (in combat), blind cover index default.
   - `unit/weapons_{fire,arms,pickup}` — per-file coverage for
     weapons.cpp: wall penetration (cached/uncached), firing pauses,
     zoom/burst selectors, carry math (best primary/secondary/owned,
     low-ammo, clip/reserve), best/second/index/id selection, knife
     jump draw/restore, reload refusal vs refill, pickup block matrix,
     item end-to-end acquire, shield/ammo validation, finalize
     (keep/conflict/height), stuck clear, smoke blocking. Needs
     `BotWeaponsHook` (Bot, BotWeaponsData, BotBehaviorData,
     BotCombatData, BotNavigateData). Notes: engine weapon metadata
     arrives via WeaponList — publish it or getWeaponProp defaults
     (id Invalid) silently break reloads; `weapon_shield` never
     enters the interesting list so shield pickup is only reachable
     via direct classify; smoke needs FL_ONGROUND plus sgtrack (via
      the sg_explode sample) plus model match; interesting refresh
      (0.5s) and active-grenade refresh (0.25s) need time advances.
      Regression guards for fixed prod bugs: wallEntry was a
      reference into the trace result that the second trace overwrote;
      weapon_shield is collected into the interesting list (was
      classify-only, unreachable end-to-end).
    - `unit/analyze_{cleaner,finish}` — per-file coverage for
      analyze.cpp: ConnectionCleaner collect/sort/bearings, ladder/jump
      protection, alternate-route vetoes, triplet/pair removals with a
      dead seam pass, and the GraphAnalyze optimizer passes (collinear,
      duplicates, unconnected), importance/collinearity math, link
      helpers, cleanup cascade, connectivity repair, finish-flag
      parsing, goal/team marking, camp sweep, plus a full
      start-to-analyzed pipeline. Needs `BotAnalyzeHook`
      (ConnectionCleaner, GraphAnalyze). Notes: sorts run ascending
      (distance shortest-first, bearings low-to-high) so the angular
      gates are live and the tight test cluster lets distance decide;
      repair heals reachability, not exact links; frames
      must advance under the lag cutoff; never copy engine tables over
      hooked ones (self-recursing spawn). No bots are spawned.
   - `unit/navigate_gaps` — closes the leftover navigate.cpp legs: rush
     timing across personalities/health/aggression/round clock, lost-bot
     refind through the nav frame, async find with throttle, epoch
     publish/apply (fresh vs stale), terrain probe-to-duck chain,
     collision execute/reset/ignore, fall track/trigger/evaluate,
     occupancy/reachability/ladder predicates, path-origin jitter,
     alternative selection negatives, avoidance with priority tracking,
     jump ballistics, nav-frame jump/door/arrival legs, lift
     enter/timeout/travel extension. Needs manager bots (createFakeClient
     joins as spectator, pin teams by hand; analyzer autostart blocks
     creation, suspend first). Notes: trace cache (0.1s) serves stale
     unhooked results — advance past it after installing hooks; path
     epochs move under you (newRound/rechoice bump too), read before
     publishing; avoidance prediction scales with maxspeed and frame
      interval; lift entry with no lift underneath is a no-op returning
      true (= continue navigation, only false aborts/repaths).
       Alternatives need a rebuilt vistable (startRebuild + pump, the empty
       map sees everything).
     - `unit/navigate_cover` — valid-node reselection (fresh pick, expired
       re-rate, rechoice overflow through findBestGoal), empty-walk advance,
       ladder distances (plain/empty/descending via prev-node heights),
       null lift, path-origin ladder-miss/radius/narrow legs, moveToGoal
       ladder/crouch-miss/plain/water legs. Notes: PathWalk accessors are
       cursor-relative (next() is at(1)); a lingering MOVETYPE_FLY silently
       routes setPathOrigin into the ladder branch — restore WALK before
       the radius legs; the narrow leg uses body-angle forward jitter
       (not the circular spread), still bounded by the radius.
    - `unit/navigate_bomb` — bomb nodes on a demolition map: fallback to
      the empty origin, close-range and audible planted resolution,
      planted-grenade entity lookup (models/w_c4.mdl matches the
      "c4.mdl" conf substring).
    - `unit/planner_search` — per-file coverage for planner.cpp:
      heuristic g/h functions across modes and flags (hostage bans,
      crouch inflation, random bands), skip predicates, A* with guards
      (reversed build, src==dst, OOB, island, missing callbacks, tiny
      graphs), Dijkstra with exact distances, Floyd rebuild/routing,
      and the facade with the memory-limit fallback via the real cvar
      path (zero limit trips init() even on tiny graphs, then restore).
      Unloaded practice still prices every node at 1. Islands
      fail graph sanity; the planner records it and carries on.
    - `unit/clients_track` — per-file coverage for clients.cpp: census
      flags (used/alive/bot/human), origin tracking, sticky teams,
      death/dormant transitions, const access. Fresh clients start
      zeroed, bring up liveness.
   - `unit/control_{cmds,graph,debug}` — per-file coverage for control.cpp
     through the real entries: dispatcher legs (prefix/help/unknown/
     admin gate/BadFormat), bot commands (add/kick/kill/fill/vote/modes/
     cvars), graph editing (check/stats/editor rights/display modes/
     radius/teleport/save/load/erase/export/import/clean/menus), debug
     commands (resolve/slay/slap/god/notarget/exec/memory/translate),
     menus end-to-end, secureCompare and admin rights. Notes: console
     prints go direct, client prints to fake edicts are dropped (assert
     state or issuer instead); dedicated harness gates graph edits
      without an editor; `graph` resolves by exact alias (never by
      substring, so `graphmenu` cannot shadow it; both `graph` and `g`
      are exercised); unlink wipes the live graph;
      graph/node commands need an admin issuer; fake_cs kill/disconnect
      are record-only, kills run async through damage; funmode applies on
      frames (assert the cvar); save/export write FS paths, loads read
      VFS (harness-only mirror, prod engine FS bridges them);
      `upload`/`refresh iamsure` stay out (network).
     Skipped: menuKick pagination internals (reached via control menu).
   - `unit/config_{lookups,files}` — per-file coverage for config.cpp:
     weapon id/type/team/noise lookups with default passthrough, table
     accessors, legacy price normalization, def application (weapons and
     sounds, garbage ignored), snapshot Ready/Unchanged/Missing cycles,
     version gating, single-name roster determinism, chat bank cycles,
     custom defaults and overrides, legacy language early-out, plus
     missing-file smokes for the rest. Needs `BotConfigHook`
     (BotConfig, BotSounds) for the private readers. Notes: unloaded
     practice still prices nodes at 1; fake_cs kill is record-only;
     loads read VFS conf paths (write fixtures there, clean up after);
     snapshots are recorded by loaders, never by reads.
   - `unit/hooks_query` — per-file coverage for hooks.cpp: QueryBuffer
     mechanics (reads/writes/skips/strings/tail patch/header shift,
     OOB guards) and the server-query rewrite paths over a dead socket.
     Safe legs only: init with the hook off, static-link bypass,
     unresolvable player factories. Symbol lookup needs a live link
     hook and stays out.
    - `unit/module_api` — per-file coverage for module.cpp: version
      handshake, bot presence/identity, node queries, goal commands and
      the empty-graph answers. Needs one manager bot (fresh clients join
      as spectators, assign teams).
   - `unit/buying_{select,flow}` — per-file coverage for buying.cpp:
    restriction/eligibility/economics gates, prostock switch,
    single-candidate selectors, exact dispatch counts through the
    gamedll call log (buy;menuselect is 2+1), reload lanes, full
    7-state machine walk with queue effects. Needs `BotBuyHook`
    (Bot, BotWeaponsData, BotBehaviorData).
  - `unit/storage_{paths,io}` — per-file coverage for storage.cpp:
    option mapping, path forms incl. VFS split, save guards, negative
    reads (missing/magic/length), error() graph drop, full unlink.
  - `unit/fakeping_calc` — coverage for fakeping.cpp: feature gating
    (runs on gamedir-modern, the base dll lacks the capability flag),
    base bounds, ping math band, throttle timing, scoreboard emit.
  - `unit/sounds_{classify,sim}` — coverage for sounds.cpp: whole
    sample database with exact radii and ordering, attenuation/volume
    math, nearest-client attribution, loud-wins/refresh/expire rules,
    template swap, simulated buttons/ladder/footsteps, fade curve.
    Zero origins resolve nowhere; probes stay isolated as noise persists.
  - `unit/message_{dispatch,items,round}` — per-file coverage for
    message.cpp through the public dispatcher only: registration,
    stranger/dormant/handler-less drops, cold stop/collect, money
    clamps, weapon/clip tracking with fire detection, ammo slots,
    WeaponList publishing, zone icons, fade blindness, vgui/text menus,
    progress bar, item flags; round flow (wins, restart money, plant,
    burst selectors, team/score routing incl. OOB, death via
    dispatcher,     HLTV restart, ResetHUD).
  - `unit/practice_{storage,update}` — per-file coverage for practice.cpp:
    dense/sparse routing with per-field merge, default-erase, team
    isolation, OOB/cold guards, lethal-only goal rating, human/bot
    damage divisors, teamkill/non-player no-ops, round-end publish with
    stale clearing, dense half-life, disk round-trip (graph needs
    >= 8 nodes for the loader; save/load paths diverge without a VFS,
    the test mirrors the file by hand).
  - `unit/vistable_{build,serial}` — per-file coverage for vistable.cpp:
    incremental rebuild, all-pairs visibility per stance, counters,
    empty-graph/fresh guards, save/load with stale-layout rejection.
    Needs one test-only friend (`BotPracticeHook` on BotNavigateData).
  - `unit/graph_{core,edit,serial}` — per-file coverage for graph.cpp/h:
    link add/evict/unassign, reachability, nearest/bucket search,
    travel time, flag mapping, legacy conversion, text export contract
    (core); editor mutations, erase/renumber, path surgery, facing,
    notify sounds, basic-node growth, stats (edit); text roundtrip
    through a real file (serial).
  - `unit/support_{pure,conf,engine}` — per-file coverage for support.cpp:
    team converts, view cone math, date format (pure); weapon aliases,
    fake steam ids, sentence defs, cvar descriptions, wav durations (conf);
    visibility traces, nearest-player filters, spray decals, welcome
    suppression (engine).

## Configure / build / run

### Windows x64 (native MSVC/clang-cl)

```sh
cmake --preset windows-tests
cmake --build --preset windows-tests --target yapb_testhost
ctest --test-dir build/windows-tests --output-on-failure
```

### Linux x64 (native gcc/clang)

```sh
cmake --preset linux-tests
cmake --build --preset linux-tests --target yapb_testhost
ctest --test-dir build/linux-tests --output-on-failure
```

The preset pins `64BIT=ON`, `BUILD_TESTS=ON`, `WITH_TLS=OFF`, `LTO=OFF`,
`STATIC_LINKENT=ON`, `CMAKE_BUILD_TYPE=Debug`. Run it from a developer prompt
where `cl`/`clang-cl` and Ninja are on `PATH`.

### Linux (from WSL, run from the repo root)

```sh
cmake -S . -B build-host \
  -G Ninja -D64BIT=ON -DWITH_TLS=OFF -DLTO=OFF -DSTATIC_LINKENT=ON \
  -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++

cmake --build build-host --target yapb_testhost
cd build-host && ctest --output-on-failure
```

With AddressSanitizer add `-DSANITIZE=address` (both gcc and clang;
the prod shared lib link under clang+ASan is a known upstream gap —
build only the `yapb_testhost` target there).

Flags rationale:

- `64BIT=ON` — the harness assumes the 64-bit `string_t` layout.
- `WITH_TLS=OFF` — no vendored mbedtls weight for tests.
- `STATIC_LINKENT=ON` — `EntityLinkHook::initialize()` becomes a no-op,
  so no process-wide `dlsym` detour fights the harness (and ASan).
  First-class build option, test builds only.
- `YB_SINGLE_THREADED=1` (set by ctest) — disables the bot thread pool.

## How a scenario works

```cpp
testhost::FakeEngine engine;
engine.initialise (YAPB_TEST_GAMEDIR "/cstrike"); // "<gamedir>/<mod>"

GiveFnptrsToDll (&engine.funcs (), &engine.globals ()); // postload + cs load
GetEntityAPI (&table, 140);                             // install bot hooks
table.pfnServerActivate (engine.edictList (), engine.edictCount (), 16);

for (int i = 0; i < 100; ++i) {
   engine.advanceTime (0.01f);
   table.pfnStartFrame ();
}
```

- The harness runs with CWD at the staged gamedir (ctest sets
  `WORKING_DIRECTORY`), like a real server: the relative
  `cstrike/dlls/cs_amd64.so` lookup resolves from there.
- `fake_cs.so` is staged automatically after build into
  `<build>/tests/host/gamedir/cstrike/dlls/`.
- ystl `REQUIRE` only records failures, it never aborts: hard
  prerequisites must return early (`BOOT_REQUIRE` in test_boot.cpp).
- One scenario per process (separate ctest entries with filters later):
  bot singletons have no reset, so scenarios must not share a process.

## Adding a scenario

1. New file in `host/scenarios/` (one file per covered prod file,
   one or more `TEST_CASE("area/name")` per file).
2. That is it: sources are globbed (`CONFIGURE_DEPENDS`) and ctest entries
   are generated from the binary's own registry
   (`tests/cmake/DiscoverTests.cmake` runs `--list-tests` as a POST_BUILD
   step). Scenarios that need the modern/regame staging tag their case name
   with `[modern]` / `[regame]`; everything else runs in `gamedir`.
3. Drive via `FakeEngine` (trace hooks, cvars, `spawnClient`), assert via
   public bot API + `engine.calls()/heardSounds()/messages()` + FakeCS log.

Run one case with the name printed by `--list-tests`:

```sh
yapb_testhost                                         # all cases, one process each
yapb_testhost --filter=unit/engine_game
yapb_testhost --color=always                           # force ANSI colors
yapb_testhost --quiet                                  # failures + summary only
yapb_testhost --reporter=junit --output=results.xml    # CI reporter (single filter)
```

The bot singletons cannot be reset, so the host runs one case per process:
with no/loose filter it re-execs itself per case (like ctest), and a single
`--filter=` runs in-process.

Tests share the staged gamedir on disk, so run ctest serially (the
`run-tests` target does); `ctest -j` can race on the config/graph files.

## Conventions

- File header names every case: `unit/<prod>_<suffix>` (one `TEST_CASE`
  per prod area; `boot/`, `forward/`, `tier0/`, `gamedll/`, `scenario/`
  prefixes for the non-unit ones).
- Shared harness helpers live in `host/test_host.h` (`FakeCSApi`
  resolution, `HOST_REQUIRE`, entry-point declarations,
  `installEngineTables`, `testhost::pumpBots`, `testhost::findBot`,
  `testhost::csCalls`): no per-file copies.
- Per-file helpers use short names inside `namespace {`:
  `boot<X>` (full server prefix + file fixtures), `build<X>Graph`,
  plus whatever the cases need (`createHuman`, `runCmd`, ...).
- Full boots all assert `cs.entered () != 0` after `GiveFnptrsToDll`;
  tables-only boots use `installEngineTables` instead.
- Private prod access goes through one `Bot<X>Hook` struct per file,
  friended in prod headers as `// test-only accessor (defined in
  tests/, no production code)`; hook methods with no callers get
  deleted, not kept "for later".
- `HOST_REQUIRE` evaluates once (actions must never run twice);
  plain `CHECK` for the rest. Containers/strings stay bare (`Array`,
  `String`, `SmallArray`); `ystl::` qualification only where bare
  names don't resolve (`UniquePtr`, `Lambda`, `abs`).
- 3-space indent, ≤145 columns, no tabs, no trailing whitespace,
  `// ...` section comments inside long cases.
