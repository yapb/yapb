//
// YaPB test host: fake GoldSource engine implementation.
//
// SPDX-License-Identifier: Unlicense
//

#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <ystl/ystl.h>
#include <linkage/goldsrc.h>

#include "fake_engine.h"

namespace bot {

namespace testhost {

FakeEngine *FakeEngine::s_active = nullptr;
int FakeEngine::s_lifecycles = 0;

namespace {

constexpr float kPi = 3.14159265358979323846f;

void AngleVectors (const float *angles, float *fwd, float *right, float *up) {
  const float yaw = angles[1] * (kPi / 180.0f);
  const float pitch = angles[0] * (kPi / 180.0f);
  const float roll = angles[2] * (kPi / 180.0f);

  const float sy = sinf (yaw), cy = cosf (yaw);
  const float sp = sinf (pitch), cp = cosf (pitch);
  const float sr = sinf (roll), cr = cosf (roll);

  if (fwd) {
    fwd[0] = cp * cy;
    fwd[1] = cp * sy;
    fwd[2] = -sp;
  }

  if (right) {
    right[0] = -sr * sp * cy + -cr * -sy;
    right[1] = -sr * sp * sy + -cr * cy;
    right[2] = -sr * cp;
  }

  if (up) {
    up[0] = cr * sp * cy + sr * sy;
    up[1] = cr * sp * sy - sr * cy;
    up[2] = cr * cp;
  }
}

void NoHit (const float *v1, const float *v2, TraceResult *out) {
  (void)v1;
  out->fAllSolid = 0;
  out->fStartSolid = 0;
  out->fInOpen = 1;
  out->fInWater = 0;
  out->flFraction = 1.0f;
  out->vecEndPos.x = v2[0];
  out->vecEndPos.y = v2[1];
  out->vecEndPos.z = v2[2];
  out->flPlaneDist = 0.0f;
  out->vecPlaneNormal.x = 0.0f;
  out->vecPlaneNormal.y = 0.0f;
  out->vecPlaneNormal.z = 1.0f;
  out->pHit = nullptr;
  out->iHitgroup = 0;
}

uint32_t Crc32Step (uint32_t crc, const uint8_t *data, size_t len) {
  crc ^= 0xffffffffu;

  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];

    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc & 1) ? (crc >> 1) ^ 0xedb88320u : crc >> 1;
    }
  }
  return crc ^ 0xffffffffu;
}

} // namespace

FakeEngine::FakeEngine () {
  // cvar pool never regrows: cvar_t* handed out stays valid forever
  cvars.reserve (1024);

  // wire every engine function to its thunk (dispatch via s_active)
  funcs.pfnPrecacheModel = ThunkPrecacheModel;
  funcs.pfnPrecacheSound = ThunkPrecacheSound;
  funcs.pfnSetModel = ThunkSetModel;
  funcs.pfnModelIndex = ThunkModelIndex;
  funcs.pfnModelFrames = ThunkModelFrames;
  funcs.pfnSetSize = ThunkSetSize;
  funcs.pfnChangeLevel = ThunkChangeLevel;
  funcs.pfnGetSpawnParms = ThunkGetSpawnParms;
  funcs.pfnSaveSpawnParms = ThunkSaveSpawnParms;
  funcs.pfnVecToYaw = ThunkVecToYaw;
  funcs.pfnVecToAngles = ThunkVecToAngles;
  funcs.pfnMoveToOrigin = ThunkMoveToOrigin;
  funcs.pfnChangeYaw = ThunkChangeYaw;
  funcs.pfnChangePitch = ThunkChangePitch;
  funcs.pfnFindEntityByString = ThunkFindEntityByString;
  funcs.pfnGetEntityIllum = ThunkGetEntityIllum;
  funcs.pfnFindEntityInSphere = ThunkFindEntityInSphere;
  funcs.pfnFindClientInPVS = ThunkFindClientInPvs;
  funcs.pfnEntitiesInPVS = ThunkEntitiesInPvs;
  funcs.pfnMakeVectors = ThunkMakeVectors;
  funcs.pfnAngleVectors = ThunkAngleVectors;
  funcs.pfnCreateEntity = ThunkCreateEntity;
  funcs.pfnRemoveEntity = ThunkRemoveEntity;
  funcs.pfnCreateNamedEntity = ThunkCreateNamedEntity;
  funcs.pfnMakeStatic = ThunkMakeStatic;
  funcs.pfnEntIsOnFloor = ThunkEntIsOnFloor;
  funcs.pfnDropToFloor = ThunkDropToFloor;
  funcs.pfnWalkMove = ThunkWalkMove;
  funcs.pfnSetOrigin = ThunkSetOrigin;
  funcs.pfnEmitSound = ThunkEmitSound;
  funcs.pfnEmitAmbientSound = ThunkEmitAmbientSound;
  funcs.pfnTraceLine = ThunkTraceLine;
  funcs.pfnTraceToss = ThunkTraceToss;
  funcs.pfnTraceMonsterHull = ThunkTraceMonsterHull;
  funcs.pfnTraceHull = ThunkTraceHull;
  funcs.pfnTraceModel = ThunkTraceModel;
  funcs.pfnTraceTexture = ThunkTraceTexture;
  funcs.pfnTraceSphere = ThunkTraceSphere;
  funcs.pfnGetAimVector = ThunkGetAimVector;
  funcs.pfnServerCommand = ThunkServerCommand;
  funcs.pfnServerExecute = ThunkServerExecute;
  funcs.pfnClientCommand = ThunkClientCommand;
  funcs.pfnParticleEffect = ThunkParticleEffect;
  funcs.pfnLightStyle = ThunkLightStyle;
  funcs.pfnDecalIndex = ThunkDecalIndex;
  funcs.pfnPointContents = ThunkPointContents;
  funcs.pfnMessageBegin = ThunkMessageBegin;
  funcs.pfnMessageEnd = ThunkMessageEnd;
  funcs.pfnWriteByte = ThunkWriteByte;
  funcs.pfnWriteChar = ThunkWriteChar;
  funcs.pfnWriteShort = ThunkWriteShort;
  funcs.pfnWriteLong = ThunkWriteLong;
  funcs.pfnWriteAngle = ThunkWriteAngle;
  funcs.pfnWriteCoord = ThunkWriteCoord;
  funcs.pfnWriteString = ThunkWriteString;
  funcs.pfnWriteEntity = ThunkWriteEntity;
  funcs.pfnCVarRegister = ThunkCVarRegister;
  funcs.pfnCVarGetFloat = ThunkCVarGetFloat;
  funcs.pfnCVarGetString = ThunkCVarGetString;
  funcs.pfnCVarSetFloat = ThunkCVarSetFloat;
  funcs.pfnCVarSetString = ThunkCVarSetString;
  funcs.pfnAlertMessage = ThunkAlertMessage;
  funcs.pfnEngineFprintf = ThunkEngineFprintf;
  funcs.pfnPvAllocEntPrivateData = ThunkPvAllocEntPrivateData;
  funcs.pfnPvEntPrivateData = ThunkPvEntPrivateData;
  funcs.pfnFreeEntPrivateData = ThunkFreeEntPrivateData;
  funcs.pfnSzFromIndex = ThunkSzFromIndex;
  funcs.pfnAllocString = ThunkAllocString;
  funcs.pfnGetVarsOfEnt = ThunkGetVarsOfEnt;
  funcs.pfnPEntityOfEntOffset = ThunkPEntityOfEntOffset;
  funcs.pfnEntOffsetOfPEntity = ThunkEntOffsetOfPEntity;
  funcs.pfnIndexOfEdict = ThunkIndexOfEdict;
  funcs.pfnPEntityOfEntIndex = ThunkPEntityOfEntIndex;
  funcs.pfnFindEntityByVars = ThunkFindEntityByVars;
  funcs.pfnGetModelPtr = ThunkGetModelPtr;
  funcs.pfnRegUserMsg = ThunkRegUserMsg;
  funcs.pfnAnimationAutomove = ThunkAnimationAutomove;
  funcs.pfnGetBonePosition = ThunkGetBonePosition;
  funcs.pfnFunctionFromName = ThunkFunctionFromName;
  funcs.pfnNameForFunction = ThunkNameForFunction;
  funcs.pfnClientPrintf = ThunkClientPrintf;
  funcs.pfnServerPrint = ThunkServerPrint;
  funcs.pfnCmd_Args = ThunkCmdArgs;
  funcs.pfnCmd_Argv = ThunkCmdArgv;
  funcs.pfnCmd_Argc = ThunkCmdArgc;
  funcs.pfnGetAttachment = ThunkGetAttachment;
  funcs.pfnCRC32_Init = ThunkCrC32Init;
  funcs.pfnCRC32_ProcessBuffer = ThunkCrC32ProcessBuffer;
  funcs.pfnCRC32_ProcessByte = ThunkCrC32ProcessByte;
  funcs.pfnCRC32_Final = ThunkCrC32Final;
  funcs.pfnRandomLong = ThunkRandomLong;
  funcs.pfnRandomFloat = ThunkRandomFloat;
  funcs.pfnSetView = ThunkSetView;
  funcs.pfnTime = ThunkTime;
  funcs.pfnCrosshairAngle = ThunkCrosshairAngle;
  funcs.pfnLoadFileForMe = ThunkLoadFileForMe;
  funcs.pfnFreeFile = ThunkFreeFile;
  funcs.pfnEndSection = ThunkEndSection;
  funcs.pfnCompareFileTime = ThunkCompareFileTime;
  funcs.pfnGetGameDir = ThunkGetGameDir;
  funcs.pfnCvar_RegisterVariable = ThunkCvarRegisterVariable;
  funcs.pfnFadeClientVolume = ThunkFadeClientVolume;
  funcs.pfnSetClientMaxspeed = ThunkSetClientMaxspeed;
  funcs.pfnCreateFakeClient = ThunkCreateFakeClient;
  funcs.pfnRunPlayerMove = ThunkRunPlayerMove;
  funcs.pfnNumberOfEntities = ThunkNumberOfEntities;
  funcs.pfnGetInfoKeyBuffer = ThunkGetInfoKeyBuffer;
  funcs.pfnInfoKeyValue = ThunkInfoKeyValue;
  funcs.pfnSetKeyValue = ThunkSetKeyValue;
  funcs.pfnSetClientKeyValue = ThunkSetClientKeyValue;
  funcs.pfnIsMapValid = ThunkIsMapValid;
  funcs.pfnStaticDecal = ThunkStaticDecal;
  funcs.pfnPrecacheGeneric = ThunkPrecacheGeneric;
  funcs.pfnGetPlayerUserId = ThunkGetPlayerUserId;
  funcs.pfnBuildSoundMsg = ThunkBuildSoundMsg;
  funcs.pfnIsDedicatedServer = ThunkIsDedicatedServer;
  funcs.pfnCVarGetPointer = ThunkCVarGetPointer;
  funcs.pfnGetPlayerWONId = ThunkGetPlayerWonId;
  funcs.pfnInfo_RemoveKey = ThunkInfoRemoveKey;
  funcs.pfnGetPhysicsKeyValue = ThunkGetPhysicsKeyValue;
  funcs.pfnSetPhysicsKeyValue = ThunkSetPhysicsKeyValue;
  funcs.pfnGetPhysicsInfoString = ThunkGetPhysicsInfoString;
  funcs.pfnPrecacheEvent = ThunkPrecacheEvent;
  funcs.pfnPlaybackEvent = ThunkPlaybackEvent;
  funcs.pfnSetFatPVS = ThunkSetFatPvs;
  funcs.pfnSetFatPAS = ThunkSetFatPas;
  funcs.pfnCheckVisibility = ThunkCheckVisibility;
  funcs.pfnDeltaSetField = ThunkDeltaSetField;
  funcs.pfnDeltaUnsetField = ThunkDeltaUnsetField;
  funcs.pfnDeltaAddEncoder = ThunkDeltaAddEncoder;
  funcs.pfnGetCurrentPlayer = ThunkGetCurrentPlayer;
  funcs.pfnCanSkipPlayer = ThunkCanSkipPlayer;
  funcs.pfnDeltaFindField = ThunkDeltaFindField;
  funcs.pfnDeltaSetFieldByIndex = ThunkDeltaSetFieldByIndex;
  funcs.pfnDeltaUnsetFieldByIndex = ThunkDeltaUnsetFieldByIndex;
  funcs.pfnSetGroupMask = ThunkSetGroupMask;
  funcs.pfnCreateInstancedBaseline = ThunkCreateInstancedBaseline;
  funcs.pfnCvar_DirectSet = ThunkCvarDirectSet;
  funcs.pfnForceUnmodified = ThunkForceUnmodified;
  funcs.pfnGetPlayerStats = ThunkGetPlayerStats;
  funcs.pfnAddServerCommand = ThunkAddServerCommand;
  funcs.pfnVoice_GetClientListening = ThunkVoiceGetClientListening;
  funcs.pfnVoice_SetClientListening = ThunkVoiceSetClientListening;
  funcs.pfnGetPlayerAuthId = ThunkGetPlayerAuthId;
  funcs.pfnSequenceGet = ThunkSequenceGet;
  funcs.pfnSequencePickSentence = ThunkSequencePickSentence;
  funcs.pfnGetFileSize = ThunkGetFileSize;
  funcs.pfnGetApproxWavePlayLen = ThunkGetApproxWavePlayLen;
  funcs.pfnIsCareerMatch = ThunkIsCareerMatch;
  funcs.pfnGetLocalizedStringLength = ThunkGetLocalizedStringLength;
  funcs.pfnRegisterTutorMessageShown = ThunkRegisterTutorMessageShown;
  funcs.pfnGetTimesTutorMessageShown = ThunkGetTimesTutorMessageShown;
  funcs.pfnProcessTutorMessageDecayBuffer = ThunkProcessTutorMessageDecayBuffer;
  funcs.pfnConstructTutorMessageDecayBuffer = ThunkConstructTutorMessageDecayBuffer;
  funcs.pfnResetTutorMessageDecayData = ThunkResetTutorMessageDecayData;
  funcs.pfnQueryClientCVarValue = ThunkQueryClientCVarValue;
  funcs.pfnQueryClientCVarValue2 = ThunkQueryClientCVarValue2;
  funcs.pfnCheckParm = ThunkCheckParm;
}

void FakeEngine::Initialise (const char *game_dir, int max_clients) {
  // bot singletons cache pool pointers (cvar_t*, globals), so a second
  // engine lifecycle in one process reads freed memory: fail loudly and
  // run scenarios in separate processes (ctest does this per case)
  if (s_lifecycles++ > 0) {
    fprintf (stderr, "FakeEngine: second lifecycle in one process is unsupported, run one scenario per process\n");
    abort ();
  }
  s_active = this;

  game_dir_.assign (game_dir ? game_dir : "");

  // run with CWD at the game install root (the parent of the mod dir), like
  // a real server: Game::loadCSBinary resolves "cstrike/dlls/cs*.dll" from
  // there. ctest already sets it, but this makes manual runs work from any
  // directory and keeps the -modern/-regame staging honest.
  const char *root = game_dir_.chars ();
  const char *slash = root;

  for (const char *p = root; *p; ++p) {
    if (*p == '/' || *p == '\\') {
      slash = p;
    }
  }
  if (slash > root) {
    ystl::String parent {};

    for (const char *p = root; p < slash; ++p) {
      parent += *p;
    }
    ystl::plat.set_working_directory (parent.chars ());
  }

  // wipe edicts; edict 0 is worldspawn
  for (auto &e : edicts) {
    e = edict_t {};
  }
  edicts[0].free = 0;
  edicts[0].v.pContainingEntity = &edicts[0];

  // string table from scratch; offset 0 stays "" like the real engine
  memset (strings, 0, sizeof (strings));
  strings_off_ = 1;

  globals = globalvars_t {};
  globals.maxClients = max_clients;
  globals.maxEntities = kMaxEdicts;
  globals.pStringBase = strings;
  globals.time = 1.0f;
  globals.frametime = 0.01f;
  globals.mapname = string_t (AllocString ("de_test"));
  globals.startspot = string_t (AllocString (""));

  edicts[0].v.classname = string_t (AllocString ("worldspawn"));

  // the whole pool is free: CreateFakeClient prefers the player slots,
  // everything else allocates wherever there is room
  for (int i = 1; i < kMaxEdicts; ++i) {
    edicts[i].free = 1;
    edicts[i].v.pContainingEntity = &edicts[i];
  }

  calls.clear ();
  server_commands_.clear ();
  client_commands_.clear ();
  heard_sounds_.clear ();
  messages.clear ();
  precached_models_.clear ();
  precached_sounds_.clear ();
  precached_decals_.clear ();
  user_msgs_.clear ();
  server_cmds_.clear ();
  info_keys_.clear ();
  cvars.clear ();
  cmd_args_.clear ();

  // the pool never regrows: cvar_t* handed out stays valid forever
  cvars.reserve (1024);

  // game cvars a real gamedll registers before the bot ever reads them;
  // without these, GameRef lookups (mp_footsteps et al.) stay null
  AddServerCvar ("mp_footsteps", "1");
  AddServerCvar ("mp_c4timer", "35");
  AddServerCvar ("mp_buytime", "1.5");
  AddServerCvar ("mp_startmoney", "800");
  AddServerCvar ("mp_limitteams", "2");
  AddServerCvar ("mp_autoteambalance", "1");
  AddServerCvar ("mp_roundtime", "2.5");
  AddServerCvar ("mp_timelimit", "0");
  AddServerCvar ("mp_freezetime", "6");
  AddServerCvar ("mp_friendlyfire", "0");
  AddServerCvar ("mp_flashlight", "0");
  AddServerCvar ("mp_maxmoney", "16000");
  AddServerCvar ("sv_gravity", "800");
  AddServerCvar ("sv_stepsize", "18");
  AddServerCvar ("sv_skycolor_r", "0");
  AddServerCvar ("sv_skycolor_g", "0");
  AddServerCvar ("sv_skycolor_b", "0");
  AddServerCvar ("sv_maxspeed", "270");
  AddServerCvar ("developer", "0");

  trace_line_hook_ = nullptr;
  trace_hull_hook_ = nullptr;

  memset (info_buffer_, 0, sizeof (info_buffer_));
  memset (pvs, 0xff, sizeof (pvs));

  random_state_ = 12345;
  change_level_called_ = false;
  next_user_msg_id_ = 64;
}

void FakeEngine::AdvanceTime (float dt) {
  globals.frametime = dt;
  globals.time += dt;

  // poor man's physics: integrate velocity for walking/flying edicts
  for (auto &e : edicts) {
    if (e.free) {
      continue;
    }
    if (e.v.movetype == MOVETYPE_WALK || e.v.movetype == MOVETYPE_STEP || e.v.movetype == MOVETYPE_FLY) {
      e.v.origin.x += e.v.velocity.x * dt;
      e.v.origin.y += e.v.velocity.y * dt;
      e.v.origin.z += e.v.velocity.z * dt;
    }
  }
}

string_t::Type FakeEngine::AllocString (const char *value) {
  if (!value || !*value) {
    return 0;
  }
  const size_t len = strlen (value) + 1;

  if (strings_off_ + static_cast<int> (len) >= kStringTableSize) {
    fprintf (stderr, "FakeEngine: string table overflow\n");
    abort ();
  }
  const int off = strings_off_;
  memcpy (strings + off, value, len);
  strings_off_ += static_cast<int> (len);

  return static_cast<string_t::Type> (off);
}

const char *FakeEngine::StringText (string_t offset) const {
  return strings + static_cast<int> (offset);
}

edict_t *FakeEngine::AllocEdict () {
  for (int i = 1; i < kMaxEdicts; ++i) {
    if (edicts[i].free) {
      edicts[i] = edict_t {};
      edicts[i].free = 0;
      edicts[i].v.pContainingEntity = &edicts[i];
      return &edicts[i];
    }
  }
  return nullptr;
}

edict_t *FakeEngine::SpawnEntity (const char *classname, const float *origin) {
  edict_t *e = AllocEdict ();

  if (!e) {
    return nullptr;
  }
  e->v.classname = string_t (AllocString (classname));

  if (origin) {
    e->v.origin.x = origin[0];
    e->v.origin.y = origin[1];
    e->v.origin.z = origin[2];
  }
  return e;
}

edict_t *FakeEngine::SpawnClient (const char *name) {
  edict_t *e = AllocEdict ();

  if (!e) {
    return nullptr;
  }
  e->v.classname = string_t (AllocString ("player"));
  e->v.netname = string_t (AllocString (name ? name : "fakeclient"));
  e->v.flags = FL_CLIENT;
  e->v.health = 100.0f;
  e->v.deadflag = DEAD_NO;
  e->v.takedamage = DAMAGE_YES;
  e->v.solid = SOLID_BBOX;
  e->v.movetype = MOVETYPE_WALK;
  e->v.maxspeed = 270.0f;
  return e;
}

void FakeEngine::TraceNoHit (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
  (void)no_monsters;
  (void)skip;
  NoHit (v1, v2, out);
}

void FakeEngine::Log (const char *name, const char *detail) {
  EngineCall call {};
  call.name.assign (name ? name : "");
  call.detail.assign (detail ? detail : "");
  calls.push (call);
}

void FakeEngine::SetCvar (const char *name, const char *value) {
  ThunkCVarSetString (name, value);
}

void FakeEngine::SetCvar (const char *name, float value) {
  ThunkCVarSetFloat (name, value);
}

void FakeEngine::SetInfoKey (const char *key, const char *value) {
  if (!key) {
    return;
  }
  for (size_t i = 0; i < info_keys_.size (); ++i) {
    if (strcmp (info_keys_[i].key.chars (), key) == 0) {
      info_keys_[i].value.assign (value ? value : "");
      return;
    }
  }
  InfoKey entry {};
  entry.key.assign (key);
  entry.value.assign (value ? value : "");
  info_keys_.push (entry);
}

void FakeEngine::SetCmdArgs (const ystl::Array<ystl::String> &args) {
  cmd_args_.clear ();

  for (const auto &a : args) {
    cmd_args_.push (a);
  }
}

FakeEngine::Cvar *FakeEngine::FindCvar (const char *name) {
  if (!name) {
    return nullptr;
  }
  for (size_t i = 0; i < cvars.size (); ++i) {
    if (strcmp (cvars[i].name.chars (), name) == 0) {
      return &cvars[i];
    }
  }
  return nullptr;
}

FakeEngine::Cvar &FakeEngine::FindOrCreateCvar (const char *name) {
  if (Cvar *c = FindCvar (name)) {
    return *c;
  }
  Cvar slot {};
  slot.name.assign (name);
  cvars.push (slot);

  return cvars[cvars.size () - 1];
}

void FakeEngine::SyncCvar (Cvar &slot) {
  slot.c.name = slot.name.chars ();
  slot.c.string = slot.sval.chars ();
  slot.c.value = slot.fval;
}

void FakeEngine::AddServerCvar (const char *name, const char *value) {
  Cvar &slot = FindOrCreateCvar (name);
  slot.sval.assign (value);
  slot.fval = strtof (slot.sval.chars (), nullptr);
  SyncCvar (slot);
}

// --- thunks ---

int FakeEngine::ThunkPrecacheModel (const char *s) {
  s_active->precached_models_.push (ystl::String (s ? s : ""));
  return static_cast<int> (s_active->precached_models_.size ());
}

int FakeEngine::ThunkPrecacheSound (const char *s) {
  s_active->precached_sounds_.push (ystl::String (s ? s : ""));
  return static_cast<int> (s_active->precached_sounds_.size ());
}

void FakeEngine::ThunkSetModel (edict_t *e, const char *m) {
  if (e) {
    e->v.model = string_t (s_active->AllocString (m));
  }
}

int FakeEngine::ThunkModelIndex (const char *m) {
  if (!m) {
    return -1;
  }
  for (size_t i = 0; i < s_active->precached_models_.size (); ++i) {
    if (strcmp (s_active->precached_models_[i].chars (), m) == 0) {
      return static_cast<int> (i) + 1;
    }
  }
  return -1;
}

int FakeEngine::ThunkModelFrames (int model_index) {
  (void)model_index;
  return 0;
}

void FakeEngine::ThunkSetSize (edict_t *e, const float *mn, const float *mx) {
  if (!e || !mn || !mx) {
    return;
  }
  e->v.mins.x = mn[0];
  e->v.mins.y = mn[1];
  e->v.mins.z = mn[2];
  e->v.maxs.x = mx[0];
  e->v.maxs.y = mx[1];
  e->v.maxs.z = mx[2];
  e->v.size.x = mx[0] - mn[0];
  e->v.size.y = mx[1] - mn[1];
  e->v.size.z = mx[2] - mn[2];
}

void FakeEngine::ThunkChangeLevel (char *s1, char *s2) {
  (void)s1;
  (void)s2;
  s_active->change_level_called_ = true;
}

void FakeEngine::ThunkGetSpawnParms (edict_t *ent) {
  (void)ent;
}

void FakeEngine::ThunkSaveSpawnParms (edict_t *ent) {
  (void)ent;
}

float FakeEngine::ThunkVecToYaw (const float *v) {
  return atan2f (v[1], v[0]) * 180.0f / kPi;
}

void FakeEngine::ThunkVecToAngles (const float *in, float *out) {
  const float xy = sqrtf (in[0] * in[0] + in[1] * in[1]);

  out[1] = atan2f (in[1], in[0]) * 180.0f / kPi;

  if (out[1] < 0.0f) {
    out[1] += 360.0f;
  }
  out[0] = xy > 0.001f ? -atan2f (in[2], xy) * 180.0f / kPi : (in[2] > 0.0f ? -90.0f : 90.0f);
  out[2] = 0.0f;
}

void FakeEngine::ThunkMoveToOrigin (edict_t *ent, const float *goal, float dist, int move_type) {
  (void)dist;
  (void)move_type;

  if (ent && goal) {
    ent->v.origin.x = goal[0];
    ent->v.origin.y = goal[1];
    ent->v.origin.z = goal[2];
  }
}

void FakeEngine::ThunkChangeYaw (edict_t *ent) {
  if (ent) {
    ent->v.angles.y = ent->v.ideal_yaw;
  }
}

void FakeEngine::ThunkChangePitch (edict_t *ent) {
  if (ent) {
    ent->v.angles.x = ent->v.idealpitch;
  }
}

edict_t *FakeEngine::ThunkFindEntityByString (edict_t *start, const char *field, const char *value) {
  if (!field || !value) {
    return nullptr;
  }
  const bool by_class = strcmp (field, "classname") == 0;
  const bool by_target = strcmp (field, "targetname") == 0;
  const bool by_target_value = strcmp (field, "target") == 0;

  if (!by_class && !by_target && !by_target_value) {
    return nullptr;
  }
  int from = 0;

  if (start) {
    from = static_cast<int> (start - s_active->edicts) + 1;
  }
  for (int i = from; i < kMaxEdicts; ++i) {
    edict_t *e = &s_active->edicts[i];

    if (e->free) {
      continue;
    }
    const char *text = s_active->StringText (by_class ? e->v.classname : (by_target ? e->v.targetname : e->v.target));

    if (strcmp (text, value) == 0) {
      return e;
    }
  }
  return nullptr;
}

int FakeEngine::ThunkGetEntityIllum (edict_t *ent) {
  (void)ent;
  return 200;
}

edict_t *FakeEngine::ThunkFindEntityInSphere (edict_t *start, const float *org, float rad) {
  int from = 1;

  if (start) {
    from = static_cast<int> (start - s_active->edicts) + 1;
  }
  const float r2 = rad * rad;

  for (int i = from; i < kMaxEdicts; ++i) {
    edict_t *e = &s_active->edicts[i];

    if (e->free) {
      continue;
    }
    const float dx = e->v.origin.x - org[0];
    const float dy = e->v.origin.y - org[1];
    const float dz = e->v.origin.z - org[2];

    if (dx * dx + dy * dy + dz * dz <= r2) {
      return e;
    }
  }
  return nullptr;
}

edict_t *FakeEngine::ThunkFindClientInPvs (edict_t *ent) {
  return ent;
}

edict_t *FakeEngine::ThunkEntitiesInPvs (edict_t *player) {
  (void)player;
  return nullptr;
}

void FakeEngine::ThunkMakeVectors (const float *v) {
  float fwd[3], right[3], up[3];
  AngleVectors (v, fwd, right, up);
  s_active->globals.v_forward.x = fwd[0];
  s_active->globals.v_forward.y = fwd[1];
  s_active->globals.v_forward.z = fwd[2];
  s_active->globals.v_right.x = right[0];
  s_active->globals.v_right.y = right[1];
  s_active->globals.v_right.z = right[2];
  s_active->globals.v_up.x = up[0];
  s_active->globals.v_up.y = up[1];
  s_active->globals.v_up.z = up[2];
}

void FakeEngine::ThunkAngleVectors (const float *v, float *fwd, float *right, float *up) {
  AngleVectors (v, fwd, right, up);
}

edict_t *FakeEngine::ThunkCreateEntity () {
  return s_active->AllocEdict ();
}

void FakeEngine::ThunkRemoveEntity (edict_t *e) {
  if (!e) {
    return;
  }
  if (e->pvPrivateData) {
    free (e->pvPrivateData);
    e->pvPrivateData = nullptr;
  }
  e->free = 1;
}

edict_t *FakeEngine::ThunkCreateNamedEntity (string_t class_name) {
  edict_t *e = s_active->AllocEdict ();

  if (e) {
    e->v.classname = class_name;
  }
  return e;
}

void FakeEngine::ThunkMakeStatic (edict_t *ent) {
  (void)ent;
}

int FakeEngine::ThunkEntIsOnFloor (edict_t *e) {
  (void)e;
  return 1;
}

int FakeEngine::ThunkDropToFloor (edict_t *e) {
  (void)e;
  return 0;
}

int FakeEngine::ThunkWalkMove (edict_t *ent, float yaw, float dist, int mode) {
  (void)mode;

  if (!ent) {
    return 0;
  }
  const float rad = yaw * kPi / 180.0f;
  ent->v.origin.x += cosf (rad) * dist;
  ent->v.origin.y += sinf (rad) * dist;

  return 1;
}

void FakeEngine::ThunkSetOrigin (edict_t *e, const float *org) {
  if (e && org) {
    e->v.origin.x = org[0];
    e->v.origin.y = org[1];
    e->v.origin.z = org[2];
  }
}

void FakeEngine::ThunkEmitSound (edict_t *entity, int channel, const char *sample, float volume, float attenuation, int flags, int pitch) {
  (void)channel;
  (void)flags;
  (void)pitch;
  HeardSound s {};
  s.ent_index = FakeEngine::ThunkIndexOfEdict (entity);
  s.sample.assign (sample ? sample : "");
  s.volume = volume;
  s.attenuation = attenuation;
  s_active->heard_sounds_.push (s);
}

void FakeEngine::ThunkEmitAmbientSound (edict_t *entity, float *pos, const char *samp, float vol, float attenuation, int flags, int pitch) {
  (void)pos;
  ThunkEmitSound (entity, 0, samp, vol, attenuation, flags, pitch);
}

void FakeEngine::ThunkTraceLine (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *ptr) {
  if (s_active->trace_line_hook_ != nullptr) {
    s_active->trace_line_hook_ (v1, v2, no_monsters, skip, ptr);
    return;
  }
  NoHit (v1, v2, ptr);
}

void FakeEngine::ThunkTraceToss (edict_t *pent, edict_t *ignore, TraceResult *ptr) {
  (void)ignore;

  if (pent) {
    NoHit (&pent->v.origin.x, &pent->v.origin.x, ptr);
  }
  else {
    *ptr = TraceResult {};
  }
}

int FakeEngine::ThunkTraceMonsterHull (edict_t *ent, const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *ptr) {
  (void)ent;
  (void)no_monsters;
  (void)skip;
  NoHit (v1, v2, ptr);

  return 0;
}

void FakeEngine::ThunkTraceHull (const float *v1, const float *v2, int no_monsters, int hull, edict_t *skip, TraceResult *ptr) {
  (void)hull;

  if (s_active->trace_hull_hook_ != nullptr) {
    s_active->trace_hull_hook_ (v1, v2, no_monsters, skip, ptr);
    return;
  }
  NoHit (v1, v2, ptr);
}

void FakeEngine::ThunkTraceModel (const float *v1, const float *v2, int hull, edict_t *pent, TraceResult *ptr) {
  (void)hull;
  (void)pent;
  NoHit (v1, v2, ptr);
}

const char *FakeEngine::ThunkTraceTexture (edict_t *ent, const float *v1, const float *v2) {
  (void)ent;
  (void)v1;
  (void)v2;
  return nullptr;
}

void FakeEngine::ThunkTraceSphere (const float *v1, const float *v2, int no_monsters, float radius, edict_t *skip, TraceResult *ptr) {
  (void)no_monsters;
  (void)radius;
  (void)skip;
  NoHit (v1, v2, ptr);
}

void FakeEngine::ThunkGetAimVector (edict_t *ent, float speed, float *ret) {
  if (!ent || !ret) {
    return;
  }
  float fwd[3];
  const float angles[3] = { ent->v.v_angle.x, ent->v.v_angle.y, ent->v.v_angle.z };
  AngleVectors (angles, fwd, nullptr, nullptr);
  ret[0] = fwd[0] * speed;
  ret[1] = fwd[1] * speed;
  ret[2] = fwd[2] * speed;
}

void FakeEngine::ThunkServerCommand (char *str) {
  s_active->server_commands_.push (ystl::String (str ? str : ""));
}

void FakeEngine::ThunkServerExecute () {}

void FakeEngine::ThunkClientCommand (edict_t *ent, char const *fmt, ...) {
  char buf[1024] = {};
  va_list ap;
  va_start (ap, fmt);
  vsnprintf (buf, sizeof (buf), fmt ? fmt : "", ap);
  va_end (ap);

  ystl::String detail {};
  detail.assignf ("ent=%d cmd=%s", ThunkIndexOfEdict (ent), buf);
  s_active->client_commands_.push (ystl::String (buf));
  s_active->Log ("ClientCommand", detail.chars ());
}

void FakeEngine::ThunkParticleEffect (const float *org, const float *dir, float color, float count) {
  (void)org;
  (void)dir;
  (void)color;
  (void)count;
}

void FakeEngine::ThunkLightStyle (int style, char *val) {
  ystl::String detail {};
  detail.assignf ("style=%d val=%s", style, val ? val : "");
  s_active->Log ("LightStyle", detail.chars ());
}

int FakeEngine::ThunkDecalIndex (const char *name) {
  if (!name || !*name) {
    return 0;
  }
  // like the real engine, hand out a stable nonzero index per decal
  for (size_t i = 0; i < s_active->precached_decals_.size (); ++i) {
    if (strcmp (s_active->precached_decals_[i].chars (), name) == 0) {
      return static_cast<int> (i) + 1;
    }
  }
  s_active->precached_decals_.push (ystl::String (name));
  return static_cast<int> (s_active->precached_decals_.size ());
}

int FakeEngine::ThunkPointContents (const float *v) {
  (void)v;
  return CONTENTS_EMPTY;
}

void FakeEngine::ThunkMessageBegin (int dest, int type, const float *origin, edict_t *ed) {
  (void)origin;
  ystl::String detail {};
  detail.assignf ("dest=%d type=%d ent=%d", dest, type, ThunkIndexOfEdict (ed));
  s_active->messages.push (ystl::String ("begin: ") + detail);
}

void FakeEngine::ThunkMessageEnd () {
  s_active->messages.push ("end");
}

void FakeEngine::ThunkWriteByte (int v) {
  ystl::String s {};
  s.assignf ("byte=%d", v);
  s_active->messages.push (s);
}

void FakeEngine::ThunkWriteChar (int v) {
  ystl::String s {};
  s.assignf ("char=%d", v);
  s_active->messages.push (s);
}

void FakeEngine::ThunkWriteShort (int v) {
  ystl::String s {};
  s.assignf ("short=%d", v);
  s_active->messages.push (s);
}

void FakeEngine::ThunkWriteLong (int v) {
  ystl::String s {};
  s.assignf ("long=%d", v);
  s_active->messages.push (s);
}

void FakeEngine::ThunkWriteAngle (float v) {
  ystl::String s {};
  s.assignf ("angle=%f", static_cast<double> (v));
  s_active->messages.push (s);
}

void FakeEngine::ThunkWriteCoord (float v) {
  ystl::String s {};
  s.assignf ("coord=%f", static_cast<double> (v));
  s_active->messages.push (s);
}

void FakeEngine::ThunkWriteString (const char *s) {
  s_active->messages.push (ystl::String ("string=") + (s ? s : ""));
}

void FakeEngine::ThunkWriteEntity (int v) {
  ystl::String s {};
  s.assignf ("entity=%d", v);
  s_active->messages.push (s);
}

void FakeEngine::ThunkCVarRegister (cvar_t *cvar) {
  if (!cvar || !cvar->name) {
    return;
  }
  Cvar &slot = s_active->FindOrCreateCvar (cvar->name);
  slot.sval.assign (cvar->string ? cvar->string : "");

  // like the real engine, derive the value from the string: pushConVar
  // never fills reg.value, it only carries the initial string
  slot.fval = strtof (slot.sval.chars (), nullptr);
  slot.c.flags = cvar->flags;
  s_active->SyncCvar (slot);
  cvar->string = slot.c.string;
}

float FakeEngine::ThunkCVarGetFloat (const char *name) {
  const Cvar *c = s_active->FindCvar (name);
  return c ? c->fval : 0.0f;
}

const char *FakeEngine::ThunkCVarGetString (const char *name) {
  const Cvar *c = s_active->FindCvar (name);

  if (c) {
    return c->sval.chars ();
  }
  return "";
}

void FakeEngine::ThunkCVarSetFloat (const char *name, float v) {
  char buf[64] = {};
  snprintf (buf, sizeof (buf), "%f", static_cast<double> (v));
  ThunkCVarSetString (name, buf);
}

void FakeEngine::ThunkCVarSetString (const char *name, const char *v) {
  if (!name) {
    return;
  }
  Cvar &slot = s_active->FindOrCreateCvar (name);
  slot.sval.assign (v ? v : "");
  slot.fval = strtof (slot.sval.chars (), nullptr);
  s_active->SyncCvar (slot);
}

void FakeEngine::ThunkAlertMessage (ALERT_TYPE atype, const char *fmt, ...) {
  (void)atype;
  char buf[1024] = {};
  va_list ap;
  va_start (ap, fmt);
  vsnprintf (buf, sizeof (buf), fmt ? fmt : "", ap);
  va_end (ap);
  fprintf (stderr, "[engine] %s", buf);
}

void FakeEngine::ThunkEngineFprintf (void *file, char *fmt, ...) {
  char buf[1024] = {};
  va_list ap;
  va_start (ap, fmt);
  vsnprintf (buf, sizeof (buf), fmt ? fmt : "", ap);
  va_end (ap);
  fputs (buf, file ? static_cast<FILE *> (file) : stderr);
}

void *FakeEngine::ThunkPvAllocEntPrivateData (edict_t *ent, int32_t cb) {
  if (!ent || cb <= 0) {
    return nullptr;
  }
  ent->pvPrivateData = calloc (1, static_cast<size_t> (cb));
  return ent->pvPrivateData;
}

void *FakeEngine::ThunkPvEntPrivateData (edict_t *ent) {
  return ent ? ent->pvPrivateData : nullptr;
}

void FakeEngine::ThunkFreeEntPrivateData (edict_t *ent) {
  if (ent && ent->pvPrivateData) {
    free (ent->pvPrivateData);
    ent->pvPrivateData = nullptr;
  }
}

const char *FakeEngine::ThunkSzFromIndex (int index) {
  return s_active->StringText (string_t (static_cast<string_t::Type> (index)));
}

string_t::Type FakeEngine::ThunkAllocString (const char *value) {
  return s_active->AllocString (value);
}

struct entvars_s *FakeEngine::ThunkGetVarsOfEnt (edict_t *ent) {
  // entvars_s is an opaque alias of entvars_t on the engine side
  return ent ? reinterpret_cast<struct entvars_s *> (&ent->v) : nullptr;
}

edict_t *FakeEngine::ThunkPEntityOfEntOffset (int offset) {
  const auto base = reinterpret_cast<const char *> (s_active->edicts);

  if (offset < 0 || offset >= static_cast<int> (sizeof (s_active->edicts))) {
    return nullptr;
  }
  return reinterpret_cast<edict_t *> (const_cast<char *> (base + offset));
}

int FakeEngine::ThunkEntOffsetOfPEntity (const edict_t *ent) {
  if (!ent) {
    return 0;
  }
  return static_cast<int> (reinterpret_cast<const char *> (ent) - reinterpret_cast<const char *> (s_active->edicts));
}

int FakeEngine::ThunkIndexOfEdict (const edict_t *ent) {
  if (!ent) {
    return 0;
  }
  const auto diff = ent - s_active->edicts;

  if (diff < 0 || diff >= kMaxEdicts) {
    return 0;
  }
  return static_cast<int> (diff);
}

edict_t *FakeEngine::ThunkPEntityOfEntIndex (int index) {
  if (index < 0 || index >= kMaxEdicts) {
    return nullptr;
  }
  edict_t *e = &s_active->edicts[index];
  return e->free ? nullptr : e;
}

edict_t *FakeEngine::ThunkFindEntityByVars (struct entvars_s *vars) {
  if (!vars) {
    return nullptr;
  }
  for (int i = 0; i < kMaxEdicts; ++i) {
    edict_t *e = &s_active->edicts[i];

    if (!e->free && reinterpret_cast<struct entvars_s *> (&e->v) == vars) {
      return e;
    }
  }
  return nullptr;
}

void *FakeEngine::ThunkGetModelPtr (edict_t *ent) {
  (void)ent;
  return nullptr;
}

int FakeEngine::ThunkRegUserMsg (const char *name, int size) {
  (void)size;

  if (!name) {
    return 0;
  }
  for (size_t i = 0; i < s_active->user_msgs_.size (); ++i) {
    if (strcmp (s_active->user_msgs_[i].name.chars (), name) == 0) {
      return s_active->user_msgs_[i].id;
    }
  }
  const int id = s_active->next_user_msg_id_++;
  UserMsg m {};
  m.name.assign (name);
  m.id = id;
  s_active->user_msgs_.push (m);

  return id;
}

void FakeEngine::ThunkAnimationAutomove (const edict_t *ent, float time) {
  (void)ent;
  (void)time;
}

void FakeEngine::ThunkGetBonePosition (const edict_t *ent, int bone, float *origin, float *angles) {
  (void)ent;
  (void)bone;

  if (origin) {
    origin[0] = origin[1] = origin[2] = 0.0f;
  }
  if (angles) {
    angles[0] = angles[1] = angles[2] = 0.0f;
  }
}

uint32_t FakeEngine::ThunkFunctionFromName (const char *name) {
  (void)name;
  return 0;
}

const char *FakeEngine::ThunkNameForFunction (uint32_t func) {
  (void)func;
  return nullptr;
}

void FakeEngine::ThunkClientPrintf (edict_t *ent, PRINT_TYPE ptype, const char *msg) {
  (void)ptype;
  ystl::String detail {};
  detail.assignf ("ent=%d msg=%s", ThunkIndexOfEdict (ent), msg ? msg : "");
  s_active->Log ("ClientPrintf", detail.chars ());
}

void FakeEngine::ThunkServerPrint (const char *msg) {
  s_active->Log ("ServerPrint", msg ? msg : "");
}

const char *FakeEngine::ThunkCmdArgs () {
  static ystl::String joined;

  joined.clear ();
  for (size_t i = 0; i < s_active->cmd_args_.size (); ++i) {
    if (i) {
      joined += ' ';
    }
    joined += s_active->cmd_args_[i];
  }
  return joined.chars ();
}

const char *FakeEngine::ThunkCmdArgv (int argc) {
  if (argc < 0 || argc >= static_cast<int> (s_active->cmd_args_.size ())) {
    return "";
  }
  return s_active->cmd_args_[static_cast<size_t> (argc)].chars ();
}

int FakeEngine::ThunkCmdArgc () {
  return static_cast<int> (s_active->cmd_args_.size ());
}

void FakeEngine::ThunkGetAttachment (const edict_t *ent, int attach, float *origin, float *angles) {
  (void)ent;
  (void)attach;

  if (origin) {
    origin[0] = origin[1] = origin[2] = 0.0f;
  }
  if (angles) {
    angles[0] = angles[1] = angles[2] = 0.0f;
  }
}

void FakeEngine::ThunkCrC32Init (uint32_t *crc) {
  if (crc) {
    *crc = 0;
  }
}

void FakeEngine::ThunkCrC32ProcessBuffer (uint32_t *crc, void *p, int len) {
  if (crc && p && len > 0) {
    *crc = Crc32Step (*crc, static_cast<const uint8_t *> (p), static_cast<size_t> (len));
  }
}

void FakeEngine::ThunkCrC32ProcessByte (uint32_t *crc, uint8_t ch) {
  if (crc) {
    *crc = Crc32Step (*crc, &ch, 1);
  }
}

uint32_t FakeEngine::ThunkCrC32Final (uint32_t crc) {
  return crc;
}

int32_t FakeEngine::ThunkRandomLong (int32_t lo, int32_t hi) {
  if (lo >= hi) {
    return lo;
  }
  s_active->random_state_ = s_active->random_state_ * 1103515245u + 12345u;
  const uint32_t span = static_cast<uint32_t> (hi - lo) + 1u;

  return lo + static_cast<int32_t> ((s_active->random_state_ >> 16) % span);
}

float FakeEngine::ThunkRandomFloat (float lo, float hi) {
  if (lo >= hi) {
    return lo;
  }
  s_active->random_state_ = s_active->random_state_ * 1103515245u + 12345u;
  const float t = static_cast<float> (s_active->random_state_ >> 8) / static_cast<float> (1u << 24);

  return lo + t * (hi - lo);
}

void FakeEngine::ThunkSetView (const edict_t *client, const edict_t *viewent) {
  (void)client;
  (void)viewent;
}

float FakeEngine::ThunkTime () {
  return s_active->globals.time;
}

void FakeEngine::ThunkCrosshairAngle (const edict_t *client, float pitch, float yaw) {
  (void)client;
  (void)pitch;
  (void)yaw;
}

uint8_t *FakeEngine::ThunkLoadFileForMe (char const *name, int *len) {
  if (len) {
    *len = 0;
  }
  if (!name || !*name) {
    return nullptr;
  }
  FILE *file = fopen (name, "rb");

  if (!file) {
    return nullptr;
  }
  fseek (file, 0, SEEK_END);
  const long size = ftell (file);
  fseek (file, 0, SEEK_SET);

  if (size <= 0) {
    fclose (file);
    return nullptr;
  }
  auto *data = static_cast<uint8_t *> (malloc (static_cast<size_t> (size)));

  if (fread (data, 1, static_cast<size_t> (size), file) != static_cast<size_t> (size)) {
    free (data);
    fclose (file);
    return nullptr;
  }
  fclose (file);

  if (len) {
    *len = static_cast<int> (size);
  }
  return data;
}

void FakeEngine::ThunkFreeFile (void *buffer) {
  free (buffer);
}

void FakeEngine::ThunkEndSection (const char *name) {
  (void)name;
}

int FakeEngine::ThunkCompareFileTime (char *f1, char *f2, int *cmp) {
  (void)f1;
  (void)f2;

  if (cmp) {
    *cmp = 0;
  }
  return 0;
}

void FakeEngine::ThunkGetGameDir (char *out) {
  if (out) {
    snprintf (out, 1024, "%s", s_active->game_dir_.chars ());
  }
}

void FakeEngine::ThunkCvarRegisterVariable (cvar_t *var) {
  ThunkCVarRegister (var);
}

void FakeEngine::ThunkFadeClientVolume (const edict_t *ent, int pct, int out_s, int hold, int in_s) {
  (void)ent;
  (void)pct;
  (void)out_s;
  (void)hold;
  (void)in_s;
}

void FakeEngine::ThunkSetClientMaxspeed (const edict_t *ent, float speed) {
  if (ent) {
    const_cast<edict_t *> (ent)->v.maxspeed = speed;
  }
}

edict_t *FakeEngine::ThunkCreateFakeClient (const char *netname) {
  edict_t *e = nullptr;

  // player slots first, then the general pool
  for (int i = 1; i <= s_active->globals.maxClients && i < kMaxEdicts; ++i) {
    if (s_active->edicts[i].free) {
      e = &s_active->edicts[i];
      *e = edict_t {};
      break;
    }
  }
  if (!e) {
    e = s_active->AllocEdict ();

    if (!e) {
      return nullptr;
    }
  }
  e->free = 0;
  e->v.pContainingEntity = e;
  e->v.classname = string_t (s_active->AllocString ("player"));
  e->v.netname = string_t (s_active->AllocString (netname ? netname : "bot"));
  e->v.flags = FL_CLIENT | FL_FAKECLIENT;
  e->v.health = 100.0f;
  e->v.deadflag = DEAD_NO;
  e->v.takedamage = DAMAGE_YES;
  e->v.solid = SOLID_BBOX;
  e->v.movetype = MOVETYPE_WALK;
  e->v.maxspeed = 270.0f;

  ystl::String detail {};
  detail.assignf ("ent=%d name=%s", ThunkIndexOfEdict (e), netname ? netname : "bot");
  s_active->Log ("CreateFakeClient", detail.chars ());

  return e;
}

void FakeEngine::ThunkRunPlayerMove (
  edict_t *client, const float *viewangles, float fwd, float side, float up, uint16_t buttons, uint8_t impulse, uint8_t msec) {
  (void)viewangles;
  (void)fwd;
  (void)side;
  (void)up;
  (void)buttons;
  (void)impulse;
  (void)msec;
  (void)client;
}

int FakeEngine::ThunkNumberOfEntities () {
  int n = 0;

  for (const auto &e : s_active->edicts) {
    if (!e.free) {
      ++n;
    }
  }
  return n;
}

char *FakeEngine::ThunkGetInfoKeyBuffer (edict_t *e) {
  (void)e;
  return s_active->info_buffer_;
}

char *FakeEngine::ThunkInfoKeyValue (char *buf, char const *key) {
  (void)buf;

  if (key) {
    for (size_t i = 0; i < s_active->info_keys_.size (); ++i) {
      if (strcmp (s_active->info_keys_[i].key.chars (), key) == 0) {
        return const_cast<char *> (s_active->info_keys_[i].value.chars ());
      }
    }
  }
  static char empty[1] = {};
  return empty;
}

void FakeEngine::ThunkSetKeyValue (char *buf, char *key, char *value) {
  (void)buf;
  (void)key;
  (void)value;
}

void FakeEngine::ThunkSetClientKeyValue (int idx, char *buf, char const *key, char const *value) {
  (void)idx;
  (void)buf;
  (void)key;
  (void)value;
}

int FakeEngine::ThunkIsMapValid (const char *name) {
  (void)name;
  return 1;
}

void FakeEngine::ThunkStaticDecal (const float *origin, int decal, int ent_index, int model_index) {
  (void)origin;
  (void)decal;
  (void)ent_index;
  (void)model_index;
}

int FakeEngine::ThunkPrecacheGeneric (char *s) {
  return ThunkPrecacheModel (s);
}

int FakeEngine::ThunkGetPlayerUserId (edict_t *e) {
  const int idx = ThunkIndexOfEdict (e);
  return idx > 0 ? idx : 0;
}

void FakeEngine::ThunkBuildSoundMsg (edict_t *entity, int channel, const char *sample, float volume, float attenuation, int flags, int pitch,
  int dest, int type, const float *origin, edict_t *ed) {
  (void)dest;
  (void)type;
  (void)origin;
  (void)ed;
  ThunkEmitSound (entity, channel, sample, volume, attenuation, flags, pitch);
}

int FakeEngine::ThunkIsDedicatedServer () {
  return s_active->dedicated ? 1 : 0;
}

cvar_t *FakeEngine::ThunkCVarGetPointer (const char *name) {
  Cvar *c = s_active->FindCvar (name);
  return c ? &c->c : nullptr;
}

unsigned int FakeEngine::ThunkGetPlayerWonId (edict_t *e) {
  (void)e;
  return 0;
}

void FakeEngine::ThunkInfoRemoveKey (char *s, const char *key) {
  (void)s;
  (void)key;
}

const char *FakeEngine::ThunkGetPhysicsKeyValue (const edict_t *client, const char *key) {
  (void)client;
  (void)key;
  return "";
}

void FakeEngine::ThunkSetPhysicsKeyValue (const edict_t *client, const char *key, const char *value) {
  (void)client;
  (void)key;
  (void)value;
}

const char *FakeEngine::ThunkGetPhysicsInfoString (const edict_t *client) {
  (void)client;
  return "";
}

uint16_t FakeEngine::ThunkPrecacheEvent (int type, const char *name) {
  (void)type;
  s_active->Log ("PrecacheEvent", name ? name : "");
  return 1;
}

void FakeEngine::ThunkPlaybackEvent (int flags, const edict_t *invoker, uint16_t ev_index, float delay, float *origin, float *angles, float fp1,
  float fp2, int ip1, int ip2, int bp1, int bp2) {
  (void)flags;
  (void)invoker;
  (void)ev_index;
  (void)delay;
  (void)origin;
  (void)angles;
  (void)fp1;
  (void)fp2;
  (void)ip1;
  (void)ip2;
  (void)bp1;
  (void)bp2;
}

uint8_t *FakeEngine::ThunkSetFatPvs (float *org) {
  (void)org;
  return s_active->pvs;
}

uint8_t *FakeEngine::ThunkSetFatPas (float *org) {
  (void)org;
  return s_active->pvs;
}

int FakeEngine::ThunkCheckVisibility (const edict_t *entity, uint8_t *set) {
  (void)entity;
  (void)set;
  return 1;
}

void FakeEngine::ThunkDeltaSetField (struct delta_s *fields, const char *name) {
  (void)fields;
  (void)name;
}

void FakeEngine::ThunkDeltaUnsetField (struct delta_s *fields, const char *name) {
  (void)fields;
  (void)name;
}

void FakeEngine::ThunkDeltaAddEncoder (char *name, void (*enc) (struct delta_s *fields, const uint8_t *from, const uint8_t *to)) {
  (void)name;
  (void)enc;
}

int FakeEngine::ThunkGetCurrentPlayer () {
  return 0;
}

int FakeEngine::ThunkCanSkipPlayer (const edict_t *player) {
  (void)player;
  return 0;
}

int FakeEngine::ThunkDeltaFindField (struct delta_s *fields, const char *name) {
  (void)fields;
  (void)name;
  return -1;
}

void FakeEngine::ThunkDeltaSetFieldByIndex (struct delta_s *fields, int num) {
  (void)fields;
  (void)num;
}

void FakeEngine::ThunkDeltaUnsetFieldByIndex (struct delta_s *fields, int num) {
  (void)fields;
  (void)num;
}

void FakeEngine::ThunkSetGroupMask (int mask, int op) {
  (void)mask;
  (void)op;
}

int FakeEngine::ThunkCreateInstancedBaseline (string_t classname, struct entity_state_s *baseline) {
  (void)classname;
  (void)baseline;
  return 0;
}

void FakeEngine::ThunkCvarDirectSet (struct cvar_t *var, const char *value) {
  if (!var) {
    return;
  }
  ThunkCVarSetString (var->name, value);
}

void FakeEngine::ThunkForceUnmodified (FORCE_TYPE type, float *mins, float *maxs, const char *name) {
  (void)type;
  (void)mins;
  (void)maxs;
  (void)name;
}

void FakeEngine::ThunkGetPlayerStats (const edict_t *client, int *ping, int *loss) {
  (void)client;

  if (ping) {
    *ping = 5;
  }
  if (loss) {
    *loss = 0;
  }
}

void FakeEngine::ThunkAddServerCommand (const char *name, void (*func) ()) {
  if (name && func) {
    ServerCmd cmd {};
    cmd.name.assign (name);
    cmd.func = func;
    s_active->server_cmds_.push (cmd);
    s_active->Log ("AddServerCommand", name);
  }
}

int FakeEngine::ThunkVoiceGetClientListening (int recv, int sender) {
  (void)recv;
  (void)sender;
  return 1;
}

int FakeEngine::ThunkVoiceSetClientListening (int recv, int sender, int listen) {
  (void)recv;
  (void)sender;
  (void)listen;
  return 1;
}

const char *FakeEngine::ThunkGetPlayerAuthId (edict_t *e) {
  (void)e;
  return "STEAM_0:0:12345";
}

struct sequenceEntry_s *FakeEngine::ThunkSequenceGet (const char *file, const char *entry) {
  (void)file;
  (void)entry;
  return nullptr;
}

struct sentenceEntry_s *FakeEngine::ThunkSequencePickSentence (const char *group, int method, int *picked) {
  (void)group;
  (void)method;
  (void)picked;
  return nullptr;
}

int FakeEngine::ThunkGetFileSize (char *name) {
  (void)name;
  return -1;
}

unsigned int FakeEngine::ThunkGetApproxWavePlayLen (const char *path) {
  (void)path;
  return 0;
}

int FakeEngine::ThunkIsCareerMatch () {
  return 0;
}

int FakeEngine::ThunkGetLocalizedStringLength (const char *label) {
  (void)label;
  return 0;
}

void FakeEngine::ThunkRegisterTutorMessageShown (int mid) {
  (void)mid;
}

int FakeEngine::ThunkGetTimesTutorMessageShown (int mid) {
  (void)mid;
  return 0;
}

void FakeEngine::ThunkProcessTutorMessageDecayBuffer (int *buf, int len) {
  (void)buf;
  (void)len;
}

void FakeEngine::ThunkConstructTutorMessageDecayBuffer (int *buf, int len) {
  (void)buf;
  (void)len;
}

void FakeEngine::ThunkResetTutorMessageDecayData () {}

void FakeEngine::ThunkQueryClientCVarValue (const edict_t *player, const char *name) {
  (void)player;
  (void)name;
}

void FakeEngine::ThunkQueryClientCVarValue2 (const edict_t *player, const char *name, int req_id) {
  (void)player;
  (void)name;
  (void)req_id;
}

int FakeEngine::ThunkCheckParm (const char *token, char **next) {
  (void)token;

  if (next) {
    *next = nullptr;
  }
  return 0;
}

} // namespace testhost

} // namespace bot
