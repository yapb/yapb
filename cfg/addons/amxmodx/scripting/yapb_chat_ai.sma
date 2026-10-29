//
// YaPB AMXX chat AI sample.
//
// OpenAI-compatible chat completions through AmxxEasyHttp (Linux + Windows).
// Supports addressed human chat, dead chat, bot chatter, bot-to-bot turns,
// a small request queue, per-player cooldown and short bot memory.
//
// required modules:
//   easy_http
//
// expected endpoint:
//   POST /v1/chat/completions
//   {"choices":[{"message":{"content":"..."}}]}
//

#include <amxmodx>
#include <easy_http>
#include <yapb>

#pragma semicolon 1

new const PLUGIN_NAME[] = "YaPB Chat AI";
new const PLUGIN_VERSION[] = "2.2.0";
new const PLUGIN_AUTHOR[] = "YaPB Team";
new const LOG_FILE[] = "yapb_chat_ai.log";

enum {
  PROMPT_HUMAN = 0,
  PROMPT_BOT,
  PROMPT_DEAD,
  PROMPT_FOLLOWUP,
  PROMPT_COUNT
};

enum {
  MAX_QUEUE = 8,
  MAX_HISTORY = 3,
  CHAT_TEXT_SIZE = 192,
  USER_TEXT_SIZE = 384,
  HTTP_TIMEOUT_MS = 20000,
  HTTP_CONNECT_TIMEOUT_MS = 3000
};

enum _:RequestData {
  rq_sender,
  rq_bot,
  rq_team_only,
  rq_followup,
  rq_prompt,
  rq_text[CHAT_TEXT_SIZE]
};

new g_enabled;
new g_debug;
new g_override_human;
new g_override_bot;
new g_override_dead;
new g_bot_talk;
new g_addressed;
new g_bot_chance;
new g_strict;
new g_temperature;
new g_max_tokens;
new g_reply_chars;
new g_cooldown;

new g_url[256];
new g_model[64];
new g_api_key[128];
new g_cvar_url;
new g_cvar_model;
new g_cvar_api_key;
new g_cvar_prompts[PROMPT_COUNT];

new EzHttpRequest:g_request;
new bool:g_busy;
new g_queue[MAX_QUEUE][RequestData];
new g_queue_count;

new g_bot;
new g_sender;
new bool:g_team_only;
new bool:g_followup;
new g_prompt;
new g_current_text[CHAT_TEXT_SIZE];

new g_history_user[(MAX_PLAYERS + 1) * MAX_HISTORY][CHAT_TEXT_SIZE];
new g_history_bot[(MAX_PLAYERS + 1) * MAX_HISTORY][CHAT_TEXT_SIZE];
new g_history_count[MAX_PLAYERS + 1];
new Float:g_player_next[MAX_PLAYERS + 1];

new g_next_bot_talk;
new g_next_dead_talk;

stock ai_log (const fmt[], any:...) {
  if (!get_pcvar_num (g_debug)) {
    return;
  }

  new text[512];
  vformat (text, charsmax (text), fmt, 2);
  log_to_file (LOG_FILE, "%s", text);
}

public OnConfigsExecuted () {
  new url[256];
  new model[64];
  get_pcvar_string (g_cvar_url, url, charsmax (url));
  get_pcvar_string (g_cvar_model, model, charsmax (model));

  ai_log ("CONFIG enabled=%d human=%d bot=%d dead=%d talk=%d addressed=%d strict=%d temp=%.2f max_tokens=%d reply_chars=%d cooldown=%.2f url=%s model=%s",
    get_pcvar_num (g_enabled),
    get_pcvar_num (g_override_human),
    get_pcvar_num (g_override_bot),
    get_pcvar_num (g_override_dead),
    get_pcvar_num (g_bot_talk),
    get_pcvar_num (g_addressed),
    get_pcvar_num (g_strict),
    get_pcvar_float (g_temperature),
    get_pcvar_num (g_max_tokens),
    get_pcvar_num (g_reply_chars),
    get_pcvar_float (g_cooldown),
    url,
    model);
}
public plugin_precache () {
  g_enabled = register_cvar ("yb_chat_ai_enabled", "1");
  g_debug = register_cvar ("yb_chat_ai_debug", "0");
  g_override_human = register_cvar ("yb_chat_ai_override_human", "1");
  g_override_bot = register_cvar ("yb_chat_ai_override_bot", "0");
  g_override_dead = register_cvar ("yb_chat_ai_override_dead", "1");
  g_bot_talk = register_cvar ("yb_chat_ai_bot_talk", "1");
  g_addressed = register_cvar ("yb_chat_ai_addressed", "1");
  g_bot_chance = register_cvar ("yb_chat_ai_bot_chance", "35");
  g_strict = register_cvar ("yb_chat_ai_strict", "0");
  g_temperature = register_cvar ("yb_chat_ai_temperature", "0.8");
  g_max_tokens = register_cvar ("yb_chat_ai_max_tokens", "512");
  g_reply_chars = register_cvar ("yb_chat_ai_reply_chars", "160");
  g_cooldown = register_cvar ("yb_chat_ai_cooldown", "3.0");

  g_cvar_url = register_cvar ("yb_chat_ai_url", "http://127.0.0.1:8080/v1/chat/completions");
  g_cvar_model = register_cvar ("yb_chat_ai_model", "local-model");
  g_cvar_api_key = register_cvar ("yb_chat_ai_api_key", "");

  g_cvar_prompts[PROMPT_HUMAN] = register_cvar ("yb_chat_ai_prompt_human",
    "You are a cocky CS 1.6 bot talking to a human. Stay in character, react to their HP, armor, team and message, tease lightly, and use a short jab. Mirror their language, one plain chat line under 160 chars, no emojis, no quotes, never mention AI.");

  g_cvar_prompts[PROMPT_BOT] = register_cvar ("yb_chat_ai_prompt_bot",
    "You are a cocky CS 1.6 bot talking to another bot. Stay in character, keep the rivalry playful, react to the message and use a short jab. Mirror their language, one plain chat line under 160 chars, no emojis, no quotes, never mention AI.");

  g_cvar_prompts[PROMPT_DEAD] = register_cvar ("yb_chat_ai_prompt_dead",
    "You are a dead CS 1.6 bot in dead chat. Stay in character, use short dark humor and ghost jokes, but keep it friendly and never insult real players. Mirror their language, one plain line under 160 chars, no emojis, no quotes, never mention AI.");

  g_cvar_prompts[PROMPT_FOLLOWUP] = register_cvar ("yb_chat_ai_prompt_followup",
    "Continue this CS 1.6 chat in character for one more line. Build on the previous joke, avoid repeating it, stay friendly and end the exchange naturally. Mirror the language, one plain line under 160 chars, no emojis, no quotes, never mention AI.");
}

public plugin_init () {
  register_plugin (PLUGIN_NAME, PLUGIN_VERSION, PLUGIN_AUTHOR);
  AutoExecConfig (true, "yapb_chat_ai");
}

public plugin_end () {
  cancel_all ();
}

public server_changelevel (map[]) {
  cancel_all ();
  reset_all_state ();
}

public client_disconnected (id) {
  if (id == g_sender || id == g_bot) {
    cancel_all ();
  }
  drop_queued_player (id);
  reset_player_state (id);
}

public yb_chat_pre (sender, const message[], bool:team_only, YBChatSource:source) {
  if (!get_pcvar_num (g_enabled) || source == YB_CHAT_PLUGIN) {
    return PLUGIN_CONTINUE;
  }

  new prompt;
  new bot;
  new text[CHAT_TEXT_SIZE];

  new alive = yb_is_user_bot (sender) ? (_:yb_is_bot_alive (sender)) : (_:is_user_alive (sender));
  ai_log ("PRE source=%d sender=%d alive=%d team_only=%d msg=%s",
    _:source, sender, alive, _:team_only, message);

  if (source == YB_CHAT_HUMAN) {
    if (!get_pcvar_num (g_override_human) || g_player_next[sender] > get_gametime ()) {
      return PLUGIN_CONTINUE;
    }

    if (get_pcvar_num (g_addressed)) {
      bot = find_addressed_bot (sender, message, team_only, bool:alive, text, charsmax (text), !bool:alive);

      if (!bot) {
        return PLUGIN_CONTINUE;
      }
    }
    else {
      bot = find_chat_bot (sender, team_only, bool:alive, !bool:alive);
      copy (text, charsmax (text), message);
    }

    if (!bot) {
      return PLUGIN_CONTINUE;
    }
    prompt = PROMPT_HUMAN;
  }
  else if (source == YB_CHAT_BOT && !yb_is_bot_alive (sender)) {
    if (!get_pcvar_num (g_override_dead) || get_systime () < g_next_dead_talk) {
      return PLUGIN_CONTINUE;
    }
    prompt = PROMPT_DEAD;
    bot = sender;
    copy (text, charsmax (text), message);
  }
  else if (source == YB_CHAT_BOT) {
    if (!get_pcvar_num (g_override_bot)) {
      return PLUGIN_CONTINUE;
    }
    prompt = PROMPT_BOT;
    bot = find_chat_bot (sender, team_only, true);
    copy (text, charsmax (text), message);
  }
  else {
    return PLUGIN_CONTINUE;
  }

  if (!bot || !queue_reply (sender, bot, text, team_only, prompt, false)) {
    return get_pcvar_num (g_strict) ? PLUGIN_HANDLED : PLUGIN_CONTINUE;
  }

  if (prompt == PROMPT_DEAD) {
    g_next_dead_talk = get_systime () + 6;
  }

  if (source == YB_CHAT_HUMAN) {
    new Float:cooldown = get_pcvar_float (g_cooldown);

    if (cooldown < 0.0) {
      cooldown = 0.0;
    }
    g_player_next[sender] = get_gametime () + cooldown;
  }
  return PLUGIN_HANDLED;
}

public yb_chat_post (sender, const message[], bool:team_only, YBChatSource:source) {
  if (!get_pcvar_num (g_enabled) || source != YB_CHAT_BOT || get_pcvar_num (g_override_bot) || !get_pcvar_num (g_bot_talk)) {
    return;
  }

  if (get_systime () < g_next_bot_talk || random_num (1, 100) > bot_chance ()) {
    return;
  }

  new bot = find_chat_bot (sender, team_only, true);

  if (bot && queue_reply (sender, bot, message, team_only, PROMPT_BOT, true)) {
    g_next_bot_talk = get_systime () + 6;
  }
}

queue_reply (sender, bot, const text[], bool:team_only, prompt, bool:followup) {
  new bool:priority = prompt == PROMPT_HUMAN;

  if (g_queue_count >= MAX_QUEUE) {
    if (!priority) {
      return false;
    }

    new drop = g_queue_count - 1;

    while (drop >= 0 && g_queue[drop][rq_prompt] == PROMPT_HUMAN) {
      drop--;
    }

    if (drop < 0) {
      return false;
    }

    for (new i = drop; i < g_queue_count - 1; ++i) {
      move_request (i, i + 1);
    }
    g_queue_count--;
  }

  new slot = g_queue_count;

  if (priority && g_queue_count > 0) {
    for (new i = g_queue_count; i > 0; --i) {
      move_request (i, i - 1);
    }
    slot = 0;
  }

  ai_log ("QUEUE add slot=%d sender=%d bot=%d prompt=%d followup=%d", slot, sender, bot, prompt, followup);
  g_queue[slot][rq_sender] = sender;
  g_queue[slot][rq_bot] = bot;
  g_queue[slot][rq_team_only] = team_only;
  g_queue[slot][rq_followup] = followup;
  g_queue[slot][rq_prompt] = prompt;
  copy (g_queue[slot][rq_text], CHAT_TEXT_SIZE, text);
  g_queue_count++;

  pump_queue ();
  return g_busy || g_queue_count > 0;
}

pump_queue () {
  if (!get_pcvar_num (g_enabled)) {
    cancel_all ();
    return;
  }

  if (g_busy || g_queue_count == 0) {
    return;
  }

  ai_log ("QUEUE pump total=%d busy=%d", g_queue_count, g_busy);
  g_sender = g_queue[0][rq_sender];
  g_bot = g_queue[0][rq_bot];
  g_team_only = bool:g_queue[0][rq_team_only];
  g_followup = bool:g_queue[0][rq_followup];
  g_prompt = g_queue[0][rq_prompt];
  copy (g_current_text, CHAT_TEXT_SIZE, g_queue[0][rq_text]);

  g_queue_count--;
  for (new i = 0; i < g_queue_count; ++i) {
    move_request (i, i + 1);
  }

  if (!send_request ()) {
    clear_current ();
    pump_queue ();
  }
}

move_request (dest, src) {
  g_queue[dest][rq_sender] = g_queue[src][rq_sender];
  g_queue[dest][rq_bot] = g_queue[src][rq_bot];
  g_queue[dest][rq_team_only] = g_queue[src][rq_team_only];
  g_queue[dest][rq_followup] = g_queue[src][rq_followup];
  g_queue[dest][rq_prompt] = g_queue[src][rq_prompt];
  copy (g_queue[dest][rq_text], CHAT_TEXT_SIZE, g_queue[src][rq_text]);
}

send_request () {
  if (g_sender < 1 || g_sender > get_maxplayers () || g_bot < 1 || g_bot > get_maxplayers () ||
      !is_user_connected (g_sender) || !is_user_connected (g_bot) ||
      !yb_is_user_bot (g_bot) || yb_is_bot_stale (g_bot)) {
    ai_log ("SEND skipped invalid target sender=%d bot=%d", g_sender, g_bot);
    return false;
  }

  if (yb_is_user_bot (g_sender) && yb_is_bot_stale (g_sender)) {
    ai_log ("SEND skipped stale sender=%d", g_sender);
    return false;
  }

  get_pcvar_string (g_cvar_url, g_url, charsmax (g_url));
  get_pcvar_string (g_cvar_model, g_model, charsmax (g_model));
  get_pcvar_string (g_cvar_api_key, g_api_key, charsmax (g_api_key));

  ai_log ("SEND bot=%d sender=%d team_only=%d prompt=%d followup=%d max_tokens=%d url=%s model=%s has_key=%d text=%s",
    g_bot, g_sender, g_team_only, g_prompt, g_followup, get_pcvar_num (g_max_tokens), g_url, g_model, g_api_key[0] ? 1 : 0, g_current_text);

  new EzJSON:root = ezjson_init_object ();

  if (root == EzInvalid_JSON) {
    return false;
  }

  new EzJSON:messages = ezjson_init_array ();

  if (messages == EzInvalid_JSON) {
    ezjson_free (root);
    return false;
  }

  new system_text[512];
  get_pcvar_string (g_cvar_prompts[g_prompt], system_text, charsmax (system_text));
  new EzJSON:sys_msg = make_message ("system", system_text);

  if (sys_msg == EzInvalid_JSON) {
    ezjson_free (messages);
    ezjson_free (root);
    return false;
  }
  ezjson_array_append_value (messages, sys_msg);
  ezjson_free (sys_msg);

  for (new i = 0; i < g_history_count[g_bot]; ++i) {
    new EzJSON:old_user = make_message ("user", g_history_user[history_index (g_bot, i)]);
    new EzJSON:old_bot = make_message ("assistant", g_history_bot[history_index (g_bot, i)]);

    if (old_user != EzInvalid_JSON && old_bot != EzInvalid_JSON) {
      ezjson_array_append_value (messages, old_user);
      ezjson_array_append_value (messages, old_bot);
    }
    if (old_user != EzInvalid_JSON) {
      ezjson_free (old_user);
    }
    if (old_bot != EzInvalid_JSON) {
      ezjson_free (old_bot);
    }
  }

  new user_text[USER_TEXT_SIZE];
  build_user_text (g_sender, g_bot, g_current_text, g_team_only, user_text, charsmax (user_text));

  new EzJSON:user = make_message ("user", user_text);

  if (user == EzInvalid_JSON) {
    ezjson_free (messages);
    ezjson_free (root);
    return false;
  }
  ezjson_array_append_value (messages, user);
  ezjson_free (user);

  new max_tokens = get_pcvar_num (g_max_tokens);

  if (max_tokens < 16) {
    max_tokens = 16;
  }
  else if (max_tokens > 2048) {
    max_tokens = 2048;
  }

  ezjson_object_set_string (root, "model", g_model);
  ezjson_object_set_real (root, "temperature", get_pcvar_float (g_temperature));
  ezjson_object_set_number (root, "max_tokens", max_tokens);
  ezjson_object_set_value (root, "messages", messages);
  ezjson_free (messages);

  new EzHttpOptions:options = ezhttp_create_options ();
  ezhttp_option_set_timeout (options, HTTP_TIMEOUT_MS);
  ezhttp_option_set_connect_timeout (options, HTTP_CONNECT_TIMEOUT_MS);
  ezhttp_option_set_header (options, "Content-Type", "application/json");

  if (g_api_key[0]) {
    new auth[160];
    formatex (auth, charsmax (auth), "Bearer %s", g_api_key);
    ezhttp_option_set_header (options, "Authorization", auth);
  }

  ezhttp_option_set_body_from_json (options, root);
  ezjson_free (root);

  g_request = ezhttp_post (g_url, "on_ai_response", options);

  if (!ezhttp_is_request_exists (g_request)) {
    return false;
  }
  g_busy = true;
  return true;
}

public on_ai_response (EzHttpRequest:request_id) {
  if (!g_busy || request_id != g_request) {
    ai_log ("RESPONSE ignored id=%d busy=%d active=%d", _:request_id, g_busy, _:g_request);
    return;
  }
  ai_log ("RESPONSE id=%d error=%d http=%d bot=%d", _:request_id, _:ezhttp_get_error_code (request_id), ezhttp_get_http_code (request_id), g_bot);

  if (!get_pcvar_num (g_enabled)) {
    g_queue_count = 0;
    clear_current ();
    return;
  }

  new reply[CHAT_TEXT_SIZE];

  if (_:ezhttp_get_error_code (request_id) == _:EZH_OK && ezhttp_get_http_code (request_id) == 200) {
    parse_reply (request_id, reply, charsmax (reply));
  }
  ai_log ("RESPONSE reply=%s", reply);

  new bot = g_bot;
  new bool:team_only = g_team_only;
  new bool:followup = g_followup;
  new text[CHAT_TEXT_SIZE];

  copy (text, charsmax (text), g_current_text);
  clear_current ();

  clean_reply (reply);
  trim_reply (reply);

  if (reply[0] && bot > 0 && bot <= get_maxplayers () && is_user_connected (bot) &&
      yb_is_user_bot (bot) && !yb_is_bot_stale (bot)) {
    yb_bot_say (bot, reply, team_only, true);
    add_history (bot, text, reply);

    if (!followup && get_pcvar_num (g_bot_talk) && get_systime () >= g_next_bot_talk &&
        random_num (1, 100) <= bot_chance ()) {

      new target = find_chat_bot (bot, team_only, true);

      if (target && queue_reply (bot, target, reply, team_only, PROMPT_FOLLOWUP, true)) {
        g_next_bot_talk = get_systime () + 6;
      }
    }
  }

  pump_queue ();
}

parse_reply (EzHttpRequest:request_id, output[], length) {
  new EzJSON:root = ezhttp_parse_json_response (request_id);

  if (root == EzInvalid_JSON) {
    return;
  }

  new EzJSON:choices = ezjson_object_get_value (root, "choices");

  if (choices != EzInvalid_JSON) {
    new EzJSON:choice = ezjson_array_get_value (choices, 0);

    if (choice != EzInvalid_JSON) {
      new EzJSON:message = ezjson_object_get_value (choice, "message");

      if (message != EzInvalid_JSON) {
        ezjson_object_get_string (message, "content", output, length);
        ezjson_free (message);
      }
      ezjson_free (choice);
    }
    ezjson_free (choices);
  }

  if (!output[0]) {
    ezjson_object_get_string (root, "reply", output, length);
  }
  ezjson_free (root);
}

EzJSON:make_message (const role[], const content[]) {
  new EzJSON:message = ezjson_init_object ();

  if (message != EzInvalid_JSON) {
    ezjson_object_set_string (message, "role", role);
    ezjson_object_set_string (message, "content", content);
  }
  return message;
}

build_user_text (sender, bot, const message[], bool:team_only, output[], length) {
  new sender_name[32];
  new bot_name[32];
  new team_name[16];
  new map_name[32];

  get_user_name (sender, sender_name, charsmax (sender_name));
  get_user_name (bot, bot_name, charsmax (bot_name));
  get_user_team (sender, team_name, charsmax (team_name));
  get_mapname (map_name, charsmax (map_name));

  formatex (output, length,
    "map %s; player %s [%s, HP %d, armor %d, %s] says%s: %s; bot %s [HP %d, armor %d, %s, personality %d, difficulty %d]",
    map_name,
    sender_name,
    team_name,
    get_user_health (sender),
    get_user_armor (sender),
    is_user_alive (sender) ? "alive" : "dead",
    team_only ? " to the team" : "",
    message,
    bot_name,
    get_user_health (bot),
    get_user_armor (bot),
    yb_is_bot_alive (bot) ? "alive" : "dead",
    _:yb_get_bot_personality (bot),
    _:yb_get_bot_difficulty (bot));
}

clean_reply (reply[]) {
  new write = 0;

  for (new read = 0; reply[read] != EOS; ++read) {
    new ch = reply[read];

    if (ch < 32 || ch == 127 || ch == '"' || ch == ';') {
      continue;
    }
    reply[write++] = ch;
  }
  reply[write] = EOS;

  new start = 0;

  while (reply[start] == ' ') {
    start++;
  }

  new end = strlen (reply);

  while (end > start && reply[end - 1] == ' ') {
    end--;
  }

  if (start > 0 || end < strlen (reply)) {
    for (new i = 0; i < end - start; ++i) {
      reply[i] = reply[start + i];
    }
    reply[end - start] = EOS;
  }
}

trim_reply (reply[]) {
  new limit = get_pcvar_num (g_reply_chars);

  if (limit <= 0) {
    return;
  }

  if (limit > CHAT_TEXT_SIZE - 1) {
    limit = CHAT_TEXT_SIZE - 1;
  }

  if (strlen (reply) > limit) {
    reply[limit] = EOS;
  }
}

bot_chance () {
  new chance = get_pcvar_num (g_bot_chance);

  if (chance < 0) {
    return 0;
  }
  if (chance > 100) {
    return 100;
  }
  return chance;
}

history_index (bot, slot) {
  return bot * MAX_HISTORY + slot;
}

add_history (bot, const user[], const reply[]) {
  if (bot < 1 || bot > MAX_PLAYERS) {
    return;
  }

  if (g_history_count[bot] >= MAX_HISTORY) {
    for (new i = 1; i < MAX_HISTORY; ++i) {
      copy (g_history_user[history_index (bot, i - 1)], CHAT_TEXT_SIZE, g_history_user[history_index (bot, i)]);
      copy (g_history_bot[history_index (bot, i - 1)], CHAT_TEXT_SIZE, g_history_bot[history_index (bot, i)]);
    }
    g_history_count[bot] = MAX_HISTORY - 1;
  }

  copy (g_history_user[history_index (bot, g_history_count[bot])], CHAT_TEXT_SIZE, user);
  copy (g_history_bot[history_index (bot, g_history_count[bot])], CHAT_TEXT_SIZE, reply);
  g_history_count[bot]++;
}

find_addressed_bot (sender, const message[], bool:team_only, bool:alive_only, output[], length, bool:dead_only = false) {
  new name[32];
  new start = 0;

  output[0] = EOS;

  while (message[start] == ' ') {
    start++;
  }

  for (new id = 1; id <= get_maxplayers (); ++id) {
    if (id == sender || !is_user_connected (id) || !yb_is_user_bot (id) || yb_is_bot_stale (id)) {
      continue;
    }

    if (alive_only && !yb_is_bot_alive (id)) {
      continue;
    }

    if (dead_only && yb_is_bot_alive (id)) {
      continue;
    }

    if (team_only && get_user_team (id) != get_user_team (sender)) {
      continue;
    }

    if (yb_get_bot_chat (id) == YB_CHAT_OFF) {
      continue;
    }

    get_user_name (id, name, charsmax (name));

    new name_len = strlen (name);

    if (name_len == 0 || !equali (message[start], name, name_len)) {
      continue;
    }

    new offset = start + name_len;
    new sep = message[offset];

    if (sep != EOS && sep != ' ' && sep != ':' && sep != ',' && sep != ';') {
      continue;
    }

    while (message[offset] == ' ' || message[offset] == ':' || message[offset] == ',' || message[offset] == ';') {
      offset++;
    }

    if (message[offset] == EOS) {
      continue;
    }

    copy (output, length, message[offset]);
    return id;
  }
  return 0;
}

find_chat_bot (sender, bool:team_only, bool:alive_only, bool:dead_only = false) {
  new team = get_user_team (sender);

  for (new id = 1; id <= get_maxplayers (); ++id) {
    if (id == sender || !is_user_connected (id) || !yb_is_user_bot (id) || yb_is_bot_stale (id)) {
      continue;
    }

    if (alive_only && !yb_is_bot_alive (id)) {
      continue;
    }

    if (dead_only && yb_is_bot_alive (id)) {
      continue;
    }

    if (team_only && get_user_team (id) != team) {
      continue;
    }

    if (yb_get_bot_chat (id) == YB_CHAT_OFF) {
      continue;
    }
    return id;
  }
  return 0;
}

cancel_all () {
  new EzHttpRequest:request = g_request;
  new bool:was_busy = g_busy;

  g_request = EzHttpRequest:0;
  g_busy = false;
  g_queue_count = 0;

  if (was_busy && ezhttp_is_request_exists (request)) {
    ezhttp_cancel_request (request);
  }
  clear_current ();
}

drop_queued_player (id) {
  new write = 0;

  for (new read = 0; read < g_queue_count; ++read) {
    if (g_queue[read][rq_sender] == id || g_queue[read][rq_bot] == id) {
      continue;
    }
    if (write != read) {
      move_request (write, read);
    }
    write++;
  }
  g_queue_count = write;
}

reset_player_state (id) {
  if (id < 1 || id > MAX_PLAYERS) {
    return;
  }
  g_history_count[id] = 0;
  g_player_next[id] = 0.0;
}

reset_all_state () {
  for (new id = 1; id <= MAX_PLAYERS; ++id) {
    reset_player_state (id);
  }
  g_next_bot_talk = 0;
  g_next_dead_talk = 0;
}

clear_current () {
  g_request = EzHttpRequest:0;
  g_bot = 0;
  g_sender = 0;
  g_team_only = false;
  g_followup = false;
  g_prompt = 0;
  g_current_text[0] = EOS;
  g_busy = false;
}
