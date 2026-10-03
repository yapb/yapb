//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

// clang-format off

// all bot console variables defined here

// behavior
namespace bot {

inline ConVar cv_debug ("debug", "0", "Enables or disables useful messages about bot states. Not required for end users.", true, 0.0f, 4.0f);
inline ConVar cv_debug_goal ("debug_goal", "-1", "Forces all alive bots to build a path and go to the graph node specified here.", true, -1.0f, kMaxNodes);
inline ConVar cv_debug_overlay_color ("debug_overlay_color", "255 255 255", "Specifies the RGB color of the debug overlay text, e.g. '255 255 255'.", false);
inline ConVar cv_user_follow_percent ("user_follow_percent", "20", "Specifies the percent of bots that can follow a leader at each round start.", true, 0.0f, 100.0f);
inline ConVar cv_user_max_followers ("user_max_followers", "1", "Specifies how many bots can follow a single user.", true, 0.0f, static_cast<float> (kGameMaxPlayers / 4));
inline ConVar cv_jasonmode ("jasonmode", "0", "If enabled, all bots will be forced to use only the knife, skipping weapon buying routines.");
inline ConVar cv_radio_mode ("radio_mode", "2", "Allows bots to use radio or chatter.\nAllowed values: '0', '1', '2'.\nIf '0', radio and chatter is disabled.\nIf '1', only radio messages.\nIf '2', only chatter.", true, 0.0f, 2.0f);
inline ConVar cv_economics_rounds ("economics_rounds", "1", "Specifies whether bots are able to use team economics, like not buying any weapons for the whole team to keep money for better guns.");
inline ConVar cv_economics_disrespect_percent ("economics_disrespect_percent", "25", "Allows bots to ignore economics and buy weapons with disrespect to economics.", true, 0.0f, 100.0f);
inline ConVar cv_check_darkness ("check_darkness", "1", "Allows or disallows the bot to check the environment for darkness, thus allowing or not allowing the use of flashlights or NVG.");
inline ConVar cv_avoid_grenades ("avoid_grenades", "1", "Allows bots to partially avoid grenades.");
inline ConVar cv_move_during_throw ("move_during_throw", "-", "Allows bots to keep moving while throwing grenades.");
inline ConVar cv_tkpunish ("tkpunish", "1", "Allows or disallows bots to take revenge on teamkillers/team attacks.");
inline ConVar cv_freeze_bots ("freeze_bots", "0", "If enabled, the bot's think function is disabled, so bots will not move anywhere from their spawn spots.");
inline ConVar cv_spraypaints ("spraypaints", "30", "Specifies the percent chance for bots to use spray paints.", true, 0.0f, 100.0f);
inline ConVar cv_botbuy ("botbuy", "1", "Allows or disallows bots weapon buying routines.");
inline ConVar cv_destroy_breakables_around ("destroy_breakables_around", "1", "Allows bots to destroy breakables around them, even without touching them.");
inline ConVar cv_object_pickup_radius ("object_pickup_radius", "450.0", "The radius within which the bot searches the world for new objects, items, and weapons.", true, 64.0f, 1024.0f);
inline ConVar cv_object_destroy_radius ("object_destroy_radius", "400.0", "The radius within which the bot destroys breakables around it, when not touching them.", true, 64.0f, 1024.0f);
inline ConVar cv_chatter_path ("chatter_path", "sound/radio/bot", "Specifies the path for the bot chatter sound files.", false);
inline ConVar cv_attack_monsters ("attack_monsters", "0", "Allows or disallows bots to attack monsters.");
inline ConVar cv_pickup_custom_items ("pickup_custom_items", "0", "Allows or disallows bots to pick up custom items.");
inline ConVar cv_pickup_ammo_and_kits ("pickup_ammo_and_kits", "0", "Allows bots to pick up mod items like ammo, health kits, and suits.");
inline ConVar cv_pickup_best ("pickup_best", "1", "Allows or disallows bots to pick up the best weapons.");
inline ConVar cv_ignore_objectives ("ignore_objectives", "0", "Allows or disallows bots to do map objectives, i.e. plant/defuse bombs, and save hostages.");

// manager
inline ConVar cv_autovacate ("autovacate", "1", "Kicks bots to automatically make room for human players.");
inline ConVar cv_autovacate_keep_slots ("autovacate_keep_slots", "1", "How many slots the autovacate feature should keep for human players.", true, 1.0f, 8.0f);
inline ConVar cv_kick_after_player_connect ("kick_after_player_connect", "1", "Kicks the bot immediately when a human player joins the server (yb_autovacate must be enabled).");
inline ConVar cv_quota ("quota", "9", "Specifies the number of bots to be added to the game.", true, 0.0f, static_cast<float> (kGameMaxPlayers));
inline ConVar cv_quota_mode ("quota_mode", "normal", "Specifies the type of quota.\nAllowed values: 'normal', 'fill', and 'match'.\nIf 'fill', the server will adjust bots to keep N players in the game.\nIf 'match', the server will maintain a certain number of bots relative to humans.", false);
inline ConVar cv_quota_match ("quota_match", "0", "Number of players to match if yb_quota_mode is set to 'match'.", true, 0.0f, static_cast<float> (kGameMaxPlayers));
inline ConVar cv_think_fps ("think_fps", "40.0", "Specifies how many times per second the bot code will run.", true, 10.0f, 90.0f);
inline ConVar cv_think_fps_disable ("think_fps_disable", "1", "Allows to completely disable think fps on Xash3D.", true, 0.0f, 1.0f, Var::Xash3D);
inline ConVar cv_autokill_delay ("autokill_delay", "0.0", "Specifies the amount of time in seconds after which bots will be killed if no humans are left alive.", true, 0.0f, 90.0f);
inline ConVar cv_first_human_restart ("first_human_restart", "0", "Restart the game if the first human player joins a bot game.");
inline ConVar cv_join_after_player ("join_after_player", "0", "Specifies whether bots should join the server only when at least one human player is in the game.");
inline ConVar cv_join_team ("join_team", "any", "Forces all bots to join the team specified here.", false);
inline ConVar cv_join_delay ("join_delay", "5.0", "Specifies after how many seconds bots should start to join the game after the changelevel.", true, 0.0f, 30.0f);
inline ConVar cv_name_prefix ("name_prefix", "", "All bot names will be prefixed with the string specified by this cvar.", false);
inline ConVar cv_difficulty ("difficulty", "3", "All bots difficulty level. Changing at runtime will affect already created bots.", true, 0.0f, 4.0f);
inline ConVar cv_difficulty_min ("difficulty_min", "-1", "Lower bound of random difficulty on bot creation. Only affects newly created bots. -1 means only yb_difficulty is used.", true, -1.0f, 4.0f);
inline ConVar cv_difficulty_max ("difficulty_max", "-1", "Upper bound of random difficulty on bot creation. Only affects newly created bots. -1 means only yb_difficulty is used.", true, -1.0f, 4.0f);
inline ConVar cv_difficulty_auto ("difficulty_auto", "0", "Allows each bot to balance its own difficulty based on the kd-ratio of the team.\nAllowed values: '0', '1', '2'.\nIf '0', difficulty auto-balancing is disabled.\nIf '1', all bots balance their difficulty.\nIf '2', only bots in the team with human players balance their difficulty (bot-only team stays at configured difficulty).", true, 0.0f, 2.0f);
inline ConVar cv_difficulty_auto_balance_interval ("difficulty_auto_balance_interval", "30", "Interval at which bots will balance their difficulty.", true, 30.0f, 240.0f);
inline ConVar cv_show_avatars ("show_avatars", "0", "Enables or disables displaying bot avatars in front of their names in the scoreboard. Note that currently you can only see avatars of your teammates.", true, 0.0f, 1.0f);
inline ConVar cv_show_latency ("show_latency", "0", "Enables latency display in the scoreboard.\nAllowed values: '0', '1', '2'.\nIf '0', there is nothing displayed.\nIf '1', there is a 'BOT' string.\nIf '2', fake ping is displayed.", true, 0.0f, 2.0f);
inline ConVar cv_save_bots ("save_bots", "1", "Allows saving bot data upon changelevel, so bots will be restored after a map change.\nAllowed values: '0', '1', '2'.\nIf '0', nothing is saved.\nIf '1', only quota is saved.\nIf '2', full bot data is saved.", true, 0.0f, 2.0f);
inline ConVar cv_botskin_t ("botskin_t", "0", "Specifies the bot's wanted skin for the Terrorist team.", true, 0.0f, 5.0f);
inline ConVar cv_botskin_ct ("botskin_ct", "0", "Specifies the bot's wanted skin for the CT team.", true, 0.0f, 5.0f);
inline ConVar cv_preferred_personality ("preferred_personality", "none", "Sets the default personality when creating bots with quota management.\nAllowed values: 'none', 'normal', 'careful', 'rusher'.\nIf 'none' is set, random personality is chosen.", false);
inline ConVar cv_language ("language", "en", "Specifies the language for bot messages and menus.", false);
inline ConVar cv_rotate_bots ("rotate_bots", "0", "Randomly disconnects and connects bots, simulating players joining/quitting.");
inline ConVar cv_rotate_stay_min ("rotate_stay_min", "360.0", "Specifies the minimum amount of seconds a bot stays connected, if rotation is active.", true, 120.0f, 7200.0f);
inline ConVar cv_rotate_stay_max ("rotate_stay_max", "3600.0", "Specifies the maximum amount of seconds a bot stays connected, if rotation is active.", true, 1800.0f, 14400.0f);
inline ConVar cv_restricted_weapons ("restricted_weapons", "", "", false);
inline ConVar cv_fun_mode ("fun_mode", "imsober", "Enables classic podbot fun modes for everyone on the server.\nAllowed values: 'imsober', 'off', 'tronisback', 'itsnewyear', 'imhaunted', 'itstoodark', 'stonedagain', 'imonmars'.", false);

// support
inline ConVar cv_display_welcome_text ("display_welcome_text", "1", "Enables or disables showing a welcome message to the host entity on game start.");
inline ConVar cv_enable_query_hook ("enable_query_hook", "0", "Enables or disables fake server query responses, which show bots as real players in the server browser.");
inline ConVar cv_enable_fake_steamids ("enable_fake_steamids", "0", "Allows or disallows bots to return a fake Steam ID.");
inline ConVar cv_smoke_grenade_checks ("smoke_grenade_checks", "2", "Affects the bot's vision by smoke clouds.", true, 0.0f, 2.0f);
inline ConVar cv_smoke_grenade_radius ("smoke_grenade_radius", "240", "Radius to check for smoke clouds around a detonated grenade.", true, 32.0f, 320.0f);

// combat
inline ConVar cv_shoots_thru_walls ("shoots_thru_walls", "2", "Specifies whether bots are able to fire at enemies behind the wall, if they hear or suspect them.", true, 0.0f, 3.0f);
inline ConVar cv_ignore_enemies ("ignore_enemies", "0", "Enables or disables searching the world for enemies.");
inline ConVar cv_check_enemy_rendering ("check_enemy_rendering", "0", "Enables or disables checking enemy rendering flags. Useful for some mods.");
inline ConVar cv_check_enemy_invincibility ("check_enemy_invincibility", "0", "Enables or disables checking enemy invincibility. Useful for some mods.");
inline ConVar cv_stab_close_enemies ("stab_close_enemies", "1", "Enables or disables the bot's ability to stab the enemy with the knife if the bot is in good condition.");
inline ConVar cv_use_engine_pvs_check ("use_engine_pvs_check", "1", "Uses the engine to check the potential visibility of an enemy.");
inline ConVar cv_use_hitbox_enemy_targeting ("use_hitbox_enemy_targeting", "0", "Uses hitbox-based enemy targeting, instead of offset-based. Use with yb_use_engine_pvs_check enabled to reduce CPU usage.");
inline ConVar cv_aim_trace_consider_glass ("aim_trace_consider_glass", "0", "Bots will consider glass when deciding to shoot enemies. Required for very special maps only.");
inline ConVar cv_dont_shoot ("dont_shoot", "0", "If enabled, bots are not allowed to press the attack button, thus forbidding them from shooting.");
inline ConVar cv_enable_bullet_spread ("enable_bullet_spread", "0", "Enables or disables bullet spread for bots on servers running without metamod.");

// chatlib
inline ConVar cv_chat ("chat", "1", "Enables or disables bot chat functionality.");
inline ConVar cv_chat_percent ("chat_percent", "30", "Bot's chance to send random dead chat when killed.", true, 0.0f, 100.0f);

// tasks
inline ConVar cv_walking_allowed ("walking_allowed", "1", "Specifies whether bots are able to use 'shift' if they think that an enemy is near.");
inline ConVar cv_camping_allowed ("camping_allowed", "1", "Allows or disallows bots to camp. Doesn't affect bomb/hostage defending tasks.");
inline ConVar cv_camping_time_min ("camping_time_min", "15.0", "Lower bound of time from which the time for camping is calculated.", true, 5.0f, 90.0f);
inline ConVar cv_camping_time_max ("camping_time_max", "45.0", "Upper bound of time until which the time for camping is calculated.", true, 15.0f, 120.0f);
inline ConVar cv_random_knife_attacks ("random_knife_attacks", "1", "Allows or disallows the ability for random knife attacks when the bot is rushing and no enemy is nearby.");

// graph
inline ConVar cv_graph_fixcamp ("graph_fixcamp", "0", "Specifies whether the bot should not 'fix' camp directions of camp waypoints when loading the old PWF format.");
inline ConVar cv_graph_url ("graph_url", "@github", "Specifies the base URL from which bots download a missing graph. The 'graph/<map>.graph' path is appended automatically. Set to empty to disable downloads.\nAllowed values: '@github', '@russia', '@http', or any full base URL.\nIf '@github', CDN raw over https (unavailable on builds without TLS).\nIf '@russia', sourcecraft download over https, for regions with github issues (unavailable on builds without TLS).\nIf '@http' (or '@legacy'), legacy server over plain http.", false);
inline ConVar cv_graph_url_upload ("graph_url_upload", "@workers", "Specifies the base URL to which bots upload graphs. The file is POSTed to the base URL as-is. Set to empty to disable uploads.\nAllowed values: '@workers', '@russia', '@http', or any full base URL.\nIf '@workers', cloudflare worker over https (falls back to '@http' on builds without TLS).\nIf '@russia', yandex worker copy over https (falls back to '@http' on builds without TLS).\nIf '@http' (or '@legacy'), legacy server over plain http.\nAutomatic collection is requested from the server root and requires https, otherwise it's skipped.", false, 0.0f, 0.0f);
inline ConVar cv_graph_auto_save_count ("graph_auto_save_count", "15", "Every N graph nodes placed on the map, the graph will be saved automatically (without checks).", true, 0.0f, kMaxNodes);
inline ConVar cv_graph_draw_distance ("graph_draw_distance", "400", "Maximum distance to draw graph nodes from the editor viewport.", true, 64.0f, 3072.0f);
inline ConVar cv_graph_auto_collect_db ("graph_auto_collect_db", "1", "Allows bots to exchange your graph files with the graph database automatically.");

// analyze
inline ConVar cv_graph_analyze_auto_start ("graph_analyze_auto_start", "1", "Autostart analyzer if all other cases fail.");
inline ConVar cv_graph_analyze_auto_save ("graph_analyze_auto_save", "1", "Auto save results of analysis to graph file and re-add bots.");
inline ConVar cv_graph_analyze_distance ("graph_analyze_distance", "64", "The minimum distance to keep nodes from each other.", true, 42.0f, 128.0f);
inline ConVar cv_graph_analyze_max_jump_height ("graph_analyze_max_jump_height", "44", "Max jump height to test if the next node will be unreachable.", true, 44.0f, 64.0f);
inline ConVar cv_graph_analyze_slice_ms ("graph_analyze_slice_ms", "4.0", "Max milliseconds of analysis work per server frame. Bounds analysis hitch, prevents hard freezes.", true, 1.0f, 25.0f);
inline ConVar cv_graph_analyze_on_finish ("graph_analyze_on_finish", "all", "Finish steps as bitmask (1 optimize, 2 clean, 4 goals, 8 camps, 16 teams) or names (optimize,clean,goals,camps,teams,all,none).", false);

// planner
inline ConVar cv_path_heuristic_mode ("path_heuristic_mode", "0", "Selects the heuristic function mode. For debug purposes only.", true, 0.0f, 4.0f);
inline ConVar cv_path_floyd_memory_limit ("path_floyd_memory_limit", "6", "Limits the maximum Floyd-Warshall memory (megabytes). Uses Dijkstra if memory is exceeded.", true, 0.0, 32.0f);
inline ConVar cv_path_dijkstra_simple_distance ("path_dijkstra_simple_distance", "1", "Uses simple distance path calculation instead of running a full Dijkstra path cycle. Used only when Floyd matrices are unavailable due to memory limits.", true, 0.0f, 1.0f);
inline ConVar cv_path_astar_post_smooth ("path_astar_post_smooth", "0", "Enables post-smoothing for A*. Reduces zig-zags on paths at the cost of some CPU cycles.");
inline ConVar cv_path_turn_penalty ("path_turn_penalty", "0.5", "Penalty weight for sharp turns in pathfinding (0=off, higher=smoother paths).", true, 0.0f, 2.0f);

// vision
inline ConVar cv_max_nodes_for_predict ("max_nodes_for_predict", "22", "Maximum number of path nodes to predict the enemy.", true, 15.0f, 256.0f);
inline ConVar cv_whose_your_daddy ("whose_your_daddy", "0", "Enables or disables extra hard difficulty for bots.");

// fakeping
inline ConVar cv_ping_base_min ("ping_base_min", "5", "Lower bound for base bot ping shown in the scoreboard upon creation.", true, 0.0f, 100.0f);
inline ConVar cv_ping_base_max ("ping_base_max", "20", "Upper bound for base bot ping shown in the scoreboard upon creation.", true, 0.0f, 100.0f);
inline ConVar cv_ping_count_real_players ("ping_count_real_players", "1", "Count player pings when calculating the average ping for bots. If not, a random ping is chosen for bots.");
inline ConVar cv_ping_updater_interval ("ping_updater_interval", "1.25", "Interval at which the fake ping gets updated in the scoreboard.", true, 0.1f, 10.0f);

// linkage
inline ConVar cv_version ("version", product.version.chars (), Var::ReadOnly);

// config
inline ConVar cv_bind_menu_key ("bind_menu_key", "=", "Binds the specified key for opening the bot menu.", false);
inline ConVar cv_ignore_cvars_on_changelevel ("ignore_cvars_on_changelevel", "yb_quota,yb_autovacate", "Specifies a comma separated list of bot cvars that will not be overwritten by the config on changelevel.", false);

// engine
inline ConVar cv_csdm_mode ("csdm_mode", "0", "Enables or disables CSDM / FFA mode for bots.\nAllowed values: '0', '1', '2', '3'.\nIf '0', CSDM / FFA mode is auto-detected.\nIf '1', CSDM / FFA mode is enabled.\nIf '2', CSDM / FFA mode is disabled.\nIf '3', FFA mode is enabled.", true, 0.0f, 3.0f);
inline ConVar cv_ignore_map_prefix_game_mode ("ignore_map_prefix_game_mode", "0", "If enabled, bots will not apply game modes based on map name prefix (fy_ and ka_ specifically).");
inline ConVar cv_threadpool_workers ("threadpool_workers", "-1", "Maximum number of threads the bot will run to process some tasks. -1 means half of the CPU cores are used.", true, -1.0f, 128.0f);
inline ConVar cv_grenadier_mode ("grenadier_mode", "0", "If enabled, bots will not apply throwing conditions on grenades.");
inline ConVar cv_ignore_enemies_after_spawn_time ("ignore_enemies_after_spawn_time", "0", "Makes bots ignore enemies for a specified time in seconds on a new round. Useful for Zombie Plague mods.", false);
inline ConVar cv_breakable_health_limit ("breakable_health_limit", "500.0", "Specifies the maximum health of a breakable object that the bot will consider destroying.", true, 1.0f, 3000.0);

// navigate
inline ConVar cv_has_team_semiclip ("has_team_semiclip", "0", "When enabled, bots will not try to avoid teammates on their way. Assumes that some semiclip plugins are in use.");
inline ConVar cv_graph_slope_height ("graph_slope_height", "18.0", "Determines the maximum slope height change between the current and next node to consider the current link as a jump link. Only for analyzer.", true, 18.0f, 45.0f);

// control
inline ConVar cv_display_menu_text ("display_menu_text", "1", "Enables or disables display menu text, when players asks for menu. Useful only for Android.", true, 0.0f, 1.0f, Var::Xash3D);
inline ConVar cv_password ("password", "", "The value (password) for the setinfo key. If the user sets the correct password, he gains access to bot commands and menus.", false, 0.0f, 0.0f, Var::Password);
inline ConVar cv_password_key ("password_key", "_ybpw", "The name of the setinfo key used to store the password for bot commands and menus.", false);
inline ConVar cv_bots_kill_on_endround ("bots_kill_on_endround", "0", "Allows the use of classic bot kill when issuing the end-round command in menus, instead of the gamedll endround.", false);

// game console variables (references to game/mod cvars)
inline ConVar mp_c4timer ("mp_c4timer", nullptr, Var::GameRef);
inline ConVar mp_buytime ("mp_buytime", nullptr, Var::GameRef, true, "1");
inline ConVar mp_startmoney ("mp_startmoney", nullptr, Var::GameRef, true, "800");
inline ConVar mp_footsteps ("mp_footsteps", nullptr, Var::GameRef);
inline ConVar mp_limitteams ("mp_limitteams", nullptr, Var::GameRef);
inline ConVar mp_autoteambalance ("mp_autoteambalance", nullptr, Var::GameRef);
inline ConVar mp_roundtime ("mp_roundtime", nullptr, Var::GameRef);
inline ConVar mp_timelimit ("mp_timelimit", nullptr, Var::GameRef);
inline ConVar mp_freezetime ("mp_freezetime", nullptr, Var::GameRef, true, "0");
inline ConVar mp_friendlyfire ("mp_friendlyfire", nullptr, Var::GameRef);
inline ConVar mp_flashlight ("mp_flashlight", nullptr, Var::GameRef);
inline ConVar mp_maxmoney ("mp_maxmoney", nullptr, Var::GameRef, true, "16000");
inline ConVar sv_gravity ("sv_gravity", nullptr, Var::GameRef);
inline ConVar sv_stepsize ("sv_stepsize", nullptr, Var::GameRef);
inline ConVar sv_skycolor_r ("sv_skycolor_r", nullptr, Var::GameRef);
inline ConVar sv_skycolor_g ("sv_skycolor_g", nullptr, Var::GameRef);
inline ConVar sv_skycolor_b ("sv_skycolor_b", nullptr, Var::GameRef);

// clang-format on

} // namespace bot
