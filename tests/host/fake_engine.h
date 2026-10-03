//
// YaPB test host: fake GoldSource engine (foolsgoldsource-inspired).
//
// SPDX-License-Identifier: Unlicense
//
// Provides enginefuncs_t + globalvars_t for the bot code under test.
// One scenario per process; initialise() resets everything to a clean state.
// Containers are ystl only, no std::*.
//

#pragma once

#include <cstdint>

#include <ystl/ystl.h>
#include <linkage/goldsrc.h>

namespace bot {

namespace testhost {

// observed engine call, human-readable for CHECK() diagnostics
struct EngineCall {
  ystl::String name {};
  ystl::String detail {};
};

struct HeardSound {
  int ent_index {};
  ystl::String sample {};
  float volume {};
  float attenuation {};
};

struct UserMsg {
  ystl::String name {};
  int id {};
};

struct ServerCmd {
  ystl::String name {};
  void (*func) () {};
};

struct InfoKey {
  ystl::String key {};
  ystl::String value {};
};

class FakeEngine {
public:
  static constexpr int kMaxEdicts = 1024;
  static constexpr int kStringTableSize = 65536;

  // trace script invoked instead of the default no-hit response
  using TraceHook = ystl::Lambda<void (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out)>;

public:
  FakeEngine ();
  ~FakeEngine () = default;

  // reset to clean state; gameDir must be "<gamedir>/<mod>" (e.g. ".../cstrike")
  void Initialise (const char *game_dir, int max_clients = 16);

  // engine tables handed to GiveFnptrsToDll
  enginefuncs_t &Funcs () {
    return funcs;
  }
  globalvars_t &Globals () {
    return globals;
  }

  // edict list handed to pfnServerActivate
  edict_t *EdictList () {
    return edicts;
  }
  int EdictCount () const {
    return kMaxEdicts;
  }

  // time control; also integrates velocity -> origin for walking edicts
  void AdvanceTime (float dt);

  // string table (mirrors engine ALLOC_STRING semantics)
  string_t::Type AllocString (const char *value);
  const char *StringText (string_t offset) const;

  // world building
  edict_t *SpawnEntity (const char *classname, const float *origin = nullptr);
  edict_t *SpawnClient (const char *name = "fakeclient");

  // trace scripting
  void SetTraceLineHook (TraceHook hook) {
    trace_line_hook_ = hook;
  }
  void SetTraceHullHook (TraceHook hook) {
    trace_hull_hook_ = hook;
  }
  static void TraceNoHit (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out);

  // cvars
  void SetCvar (const char *name, const char *value);
  void SetCvar (const char *name, float value);

  // server mode: dedicated by default; a listen server unlocks the
  // welcome/bind paths that are gated on !isDedicatedServer()
  void SetDedicatedServer (bool value) {
    dedicated = value;
  }

  // player info keys (pfnInfoKeyValue); unset keys read as ""
  void SetInfoKey (const char *key, const char *value);

  // fake client-command input (pfnCmd_Args/Argv/Argc)
  void SetCmdArgs (const ystl::Array<ystl::String> &args);

  // observations for assertions
  const ystl::Array<EngineCall> &Calls () const {
    return calls;
  }
  const ystl::Array<ystl::String> &ServerCommands () const {
    return server_commands_;
  }
  const ystl::Array<ystl::String> &ClientCommands () const {
    return client_commands_;
  }
  const ystl::Array<HeardSound> &HeardSounds () const {
    return heard_sounds_;
  }
  const ystl::Array<ystl::String> &Messages () const {
    return messages;
  }
  const ystl::Array<ystl::String> &PrecachedModels () const {
    return precached_models_;
  }
  const ystl::Array<ystl::String> &PrecachedSounds () const {
    return precached_sounds_;
  }
  bool ChangeLevelCalled () const {
    return change_level_called_;
  }

  void Log (const char *name, const char *detail = "");

private:
  struct Cvar {
    ystl::String name {};
    ystl::String sval {};
    float fval {};
    cvar_t c {};
  };

private:
  edict_t *AllocEdict ();
  Cvar *FindCvar (const char *name);
  Cvar &FindOrCreateCvar (const char *name);

  // default cvars a real server always carries (game + engine side)
  void AddServerCvar (const char *name, const char *value);

  // refresh cvar_t pointers after any sval/name mutation
  void SyncCvar (Cvar &slot);

private:
  static FakeEngine *s_active;
  static int s_lifecycles;

  enginefuncs_t funcs {};
  globalvars_t globals {};
  edict_t edicts[kMaxEdicts] {};

  char strings[kStringTableSize] {};
  int strings_off_ = 1;

  ystl::String game_dir_ {};
  ystl::Array<EngineCall> calls {};
  ystl::Array<ystl::String> server_commands_ {};
  ystl::Array<ystl::String> client_commands_ {};
  ystl::Array<HeardSound> heard_sounds_ {};
  ystl::Array<ystl::String> messages {};
  ystl::Array<ystl::String> precached_models_ {};
  ystl::Array<ystl::String> precached_sounds_ {};
  ystl::Array<ystl::String> precached_decals_ {};
  ystl::Array<UserMsg> user_msgs_ {};
  ystl::Array<ServerCmd> server_cmds_ {};
  ystl::Array<InfoKey> info_keys_ {};
  ystl::Array<Cvar> cvars {};
  ystl::Array<ystl::String> cmd_args_ {};

  TraceHook trace_line_hook_ {};
  TraceHook trace_hull_hook_ {};

  char info_buffer_[1024] {};
  uint8_t pvs[1024] {};
  unsigned int random_state_ = 12345;
  bool change_level_called_ = false;
  bool dedicated = true;
  int next_user_msg_id_ = 64;

  // --- engine thunks (static, dispatch via s_active) ---
  static int ThunkPrecacheModel (const char *s);
  static int ThunkPrecacheSound (const char *s);
  static void ThunkSetModel (edict_t *e, const char *m);
  static int ThunkModelIndex (const char *m);
  static int ThunkModelFrames (int model_index);
  static void ThunkSetSize (edict_t *e, const float *mn, const float *mx);
  static void ThunkChangeLevel (char *s1, char *s2);
  static void ThunkGetSpawnParms (edict_t *ent);
  static void ThunkSaveSpawnParms (edict_t *ent);
  static float ThunkVecToYaw (const float *v);
  static void ThunkVecToAngles (const float *in, float *out);
  static void ThunkMoveToOrigin (edict_t *ent, const float *goal, float dist, int move_type);
  static void ThunkChangeYaw (edict_t *ent);
  static void ThunkChangePitch (edict_t *ent);
  static edict_t *ThunkFindEntityByString (edict_t *start, const char *field, const char *value);
  static int ThunkGetEntityIllum (edict_t *ent);
  static edict_t *ThunkFindEntityInSphere (edict_t *start, const float *org, float rad);
  static edict_t *ThunkFindClientInPvs (edict_t *ent);
  static edict_t *ThunkEntitiesInPvs (edict_t *player);
  static void ThunkMakeVectors (const float *v);
  static void ThunkAngleVectors (const float *v, float *fwd, float *right, float *up);
  static edict_t *ThunkCreateEntity ();
  static void ThunkRemoveEntity (edict_t *e);
  static edict_t *ThunkCreateNamedEntity (string_t class_name);
  static void ThunkMakeStatic (edict_t *ent);
  static int ThunkEntIsOnFloor (edict_t *e);
  static int ThunkDropToFloor (edict_t *e);
  static int ThunkWalkMove (edict_t *ent, float yaw, float dist, int mode);
  static void ThunkSetOrigin (edict_t *e, const float *org);
  static void ThunkEmitSound (edict_t *entity, int channel, const char *sample, float volume, float attenuation, int flags, int pitch);
  static void ThunkEmitAmbientSound (edict_t *entity, float *pos, const char *samp, float vol, float attenuation, int flags, int pitch);
  static void ThunkTraceLine (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *ptr);
  static void ThunkTraceToss (edict_t *pent, edict_t *ignore, TraceResult *ptr);
  static int ThunkTraceMonsterHull (edict_t *ent, const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *ptr);
  static void ThunkTraceHull (const float *v1, const float *v2, int no_monsters, int hull, edict_t *skip, TraceResult *ptr);
  static void ThunkTraceModel (const float *v1, const float *v2, int hull, edict_t *pent, TraceResult *ptr);
  static const char *ThunkTraceTexture (edict_t *ent, const float *v1, const float *v2);
  static void ThunkTraceSphere (const float *v1, const float *v2, int no_monsters, float radius, edict_t *skip, TraceResult *ptr);
  static void ThunkGetAimVector (edict_t *ent, float speed, float *ret);
  static void ThunkServerCommand (char *str);
  static void ThunkServerExecute ();
  static void ThunkClientCommand (edict_t *ent, char const *fmt, ...);
  static void ThunkParticleEffect (const float *org, const float *dir, float color, float count);
  static void ThunkLightStyle (int style, char *val);
  static int ThunkDecalIndex (const char *name);
  static int ThunkPointContents (const float *v);
  static void ThunkMessageBegin (int dest, int type, const float *origin, edict_t *ed);
  static void ThunkMessageEnd ();
  static void ThunkWriteByte (int v);
  static void ThunkWriteChar (int v);
  static void ThunkWriteShort (int v);
  static void ThunkWriteLong (int v);
  static void ThunkWriteAngle (float v);
  static void ThunkWriteCoord (float v);
  static void ThunkWriteString (const char *s);
  static void ThunkWriteEntity (int v);
  static void ThunkCVarRegister (cvar_t *cvar);
  static float ThunkCVarGetFloat (const char *name);
  static const char *ThunkCVarGetString (const char *name);
  static void ThunkCVarSetFloat (const char *name, float v);
  static void ThunkCVarSetString (const char *name, const char *v);
  static void ThunkAlertMessage (ALERT_TYPE atype, const char *fmt, ...);
  static void ThunkEngineFprintf (void *file, char *fmt, ...);
  static void *ThunkPvAllocEntPrivateData (edict_t *ent, int32_t cb);
  static void *ThunkPvEntPrivateData (edict_t *ent);
  static void ThunkFreeEntPrivateData (edict_t *ent);
  static const char *ThunkSzFromIndex (int index);
  static string_t::Type ThunkAllocString (const char *value);
  static struct entvars_s *ThunkGetVarsOfEnt (edict_t *ent);
  static edict_t *ThunkPEntityOfEntOffset (int offset);
  static int ThunkEntOffsetOfPEntity (const edict_t *ent);
  static int ThunkIndexOfEdict (const edict_t *ent);
  static edict_t *ThunkPEntityOfEntIndex (int index);
  static edict_t *ThunkFindEntityByVars (struct entvars_s *vars);
  static void *ThunkGetModelPtr (edict_t *ent);
  static int ThunkRegUserMsg (const char *name, int size);
  static void ThunkAnimationAutomove (const edict_t *ent, float time);
  static void ThunkGetBonePosition (const edict_t *ent, int bone, float *origin, float *angles);
  static uint32_t ThunkFunctionFromName (const char *name);
  static const char *ThunkNameForFunction (uint32_t func);
  static void ThunkClientPrintf (edict_t *ent, PRINT_TYPE ptype, const char *msg);
  static void ThunkServerPrint (const char *msg);
  static const char *ThunkCmdArgs ();
  static const char *ThunkCmdArgv (int argc);
  static int ThunkCmdArgc ();
  static void ThunkGetAttachment (const edict_t *ent, int attach, float *origin, float *angles);
  static void ThunkCrC32Init (uint32_t *crc);
  static void ThunkCrC32ProcessBuffer (uint32_t *crc, void *p, int len);
  static void ThunkCrC32ProcessByte (uint32_t *crc, uint8_t ch);
  static uint32_t ThunkCrC32Final (uint32_t crc);
  static int32_t ThunkRandomLong (int32_t lo, int32_t hi);
  static float ThunkRandomFloat (float lo, float hi);
  static void ThunkSetView (const edict_t *client, const edict_t *viewent);
  static float ThunkTime ();
  static void ThunkCrosshairAngle (const edict_t *client, float pitch, float yaw);
  static uint8_t *ThunkLoadFileForMe (char const *name, int *len);
  static void ThunkFreeFile (void *buffer);
  static void ThunkEndSection (const char *name);
  static int ThunkCompareFileTime (char *f1, char *f2, int *cmp);
  static void ThunkGetGameDir (char *out);
  static void ThunkCvarRegisterVariable (cvar_t *var);
  static void ThunkFadeClientVolume (const edict_t *ent, int pct, int out_s, int hold, int in_s);
  static void ThunkSetClientMaxspeed (const edict_t *ent, float speed);
  static edict_t *ThunkCreateFakeClient (const char *netname);
  static void ThunkRunPlayerMove (
    edict_t *client, const float *viewangles, float fwd, float side, float up, uint16_t buttons, uint8_t impulse, uint8_t msec);
  static int ThunkNumberOfEntities ();
  static char *ThunkGetInfoKeyBuffer (edict_t *e);
  static char *ThunkInfoKeyValue (char *buf, char const *key);
  static void ThunkSetKeyValue (char *buf, char *key, char *value);
  static void ThunkSetClientKeyValue (int idx, char *buf, char const *key, char const *value);
  static int ThunkIsMapValid (const char *name);
  static void ThunkStaticDecal (const float *origin, int decal, int ent_index, int model_index);
  static int ThunkPrecacheGeneric (char *s);
  static int ThunkGetPlayerUserId (edict_t *e);
  static void ThunkBuildSoundMsg (edict_t *entity, int channel, const char *sample, float volume, float attenuation, int flags, int pitch,
    int dest, int type, const float *origin, edict_t *ed);
  static int ThunkIsDedicatedServer ();
  static cvar_t *ThunkCVarGetPointer (const char *name);
  static unsigned int ThunkGetPlayerWonId (edict_t *e);
  static void ThunkInfoRemoveKey (char *s, const char *key);
  static const char *ThunkGetPhysicsKeyValue (const edict_t *client, const char *key);
  static void ThunkSetPhysicsKeyValue (const edict_t *client, const char *key, const char *value);
  static const char *ThunkGetPhysicsInfoString (const edict_t *client);
  static uint16_t ThunkPrecacheEvent (int type, const char *name);
  static void ThunkPlaybackEvent (int flags, const edict_t *invoker, uint16_t ev_index, float delay, float *origin, float *angles, float fp1,
    float fp2, int ip1, int ip2, int bp1, int bp2);
  static uint8_t *ThunkSetFatPvs (float *org);
  static uint8_t *ThunkSetFatPas (float *org);
  static int ThunkCheckVisibility (const edict_t *entity, uint8_t *set);
  static void ThunkDeltaSetField (struct delta_s *fields, const char *name);
  static void ThunkDeltaUnsetField (struct delta_s *fields, const char *name);
  static void ThunkDeltaAddEncoder (char *name, void (*enc) (struct delta_s *fields, const uint8_t *from, const uint8_t *to));
  static int ThunkGetCurrentPlayer ();
  static int ThunkCanSkipPlayer (const edict_t *player);
  static int ThunkDeltaFindField (struct delta_s *fields, const char *name);
  static void ThunkDeltaSetFieldByIndex (struct delta_s *fields, int num);
  static void ThunkDeltaUnsetFieldByIndex (struct delta_s *fields, int num);
  static void ThunkSetGroupMask (int mask, int op);
  static int ThunkCreateInstancedBaseline (string_t classname, struct entity_state_s *baseline);
  static void ThunkCvarDirectSet (struct cvar_t *var, const char *value);
  static void ThunkForceUnmodified (FORCE_TYPE type, float *mins, float *maxs, const char *name);
  static void ThunkGetPlayerStats (const edict_t *client, int *ping, int *loss);
  static void ThunkAddServerCommand (const char *name, void (*func) ());
  static int ThunkVoiceGetClientListening (int recv, int sender);
  static int ThunkVoiceSetClientListening (int recv, int sender, int listen);
  static const char *ThunkGetPlayerAuthId (edict_t *e);
  static struct sequenceEntry_s *ThunkSequenceGet (const char *file, const char *entry);
  static struct sentenceEntry_s *ThunkSequencePickSentence (const char *group, int method, int *picked);
  static int ThunkGetFileSize (char *name);
  static unsigned int ThunkGetApproxWavePlayLen (const char *path);
  static int ThunkIsCareerMatch ();
  static int ThunkGetLocalizedStringLength (const char *label);
  static void ThunkRegisterTutorMessageShown (int mid);
  static int ThunkGetTimesTutorMessageShown (int mid);
  static void ThunkProcessTutorMessageDecayBuffer (int *buf, int len);
  static void ThunkConstructTutorMessageDecayBuffer (int *buf, int len);
  static void ThunkResetTutorMessageDecayData ();
  static void ThunkQueryClientCVarValue (const edict_t *player, const char *name);
  static void ThunkQueryClientCVarValue2 (const edict_t *player, const char *name, int req_id);
  static int ThunkCheckParm (const char *token, char **next);
};

} // namespace testhost

} // namespace bot
