//
// YaPB test host: fake cs gamedll (second mock).
//
// SPDX-License-Identifier: Unlicense
//
// Baseline gamefuncs_t that yapb intercepts via GetEntityAPI. Every entry
// logs its invocation so tests can assert forwarding was not broken.
// Build-time toggles emulate gamedll variants:
//   -DFAKECS_MODERN=1  -> exports weapon_famas (modern cs + bot voice)
//   -DFAKECS_REGAME=1  -> exports trigger_random_unique (regamedll)
// Containers are ystl only, no std::*.
//

#include <cstdint>
#include <cstdio>
#include <cstring>

#include <ystl/ystl.h>
#include <linkage/goldsrc.h>
#include <linkage/metamod.h>
#include <linkage/physint.h>

namespace bot {

namespace {

ystl::Array<ystl::String> &CallLog () {
  static ystl::Array<ystl::String> log;
  return log;
}

int &Entered () {
  static int flag = 0;
  return flag;
}

void Record (const char *func, const char *detail = "") {
  Entered () = 1;

  // drop the stub_ prefix so the log reads like the engine api
  if (func && strncmp (func, "stub_", 5) == 0) {
    func += 5;
  }
  ystl::String line {};
  line.assign (func ? func : "");

  if (detail && *detail) {
    line += ' ';
    line += detail;
  }
  CallLog ().push (line);
}

void GameInit () {
  Record (__func__);
}

int Spawn (edict_t *pent) {
  (void)pent;
  Record (__func__);
  return 0;
}

void Think (edict_t *pent) {
  (void)pent;
  Record (__func__);
}

void Use (edict_t *used, edict_t *other) {
  (void)used;
  (void)other;
  Record (__func__);
}

void Touch (edict_t *touched, edict_t *other) {
  (void)touched;
  (void)other;
  Record (__func__);
}

void Blocked (edict_t *blocked, edict_t *other) {
  (void)blocked;
  (void)other;
  Record (__func__);
}

void KeyValue (edict_t *ent, KeyValueData *kvd) {
  (void)ent;
  (void)kvd;
  Record (__func__);
}

void Save (edict_t *pent, SAVERESTOREDATA *data) {
  (void)pent;
  (void)data;
  Record (__func__);
}

int Restore (edict_t *pent, SAVERESTOREDATA *data, int global) {
  (void)pent;
  (void)data;
  (void)global;
  Record (__func__);
  return 0;
}

void SetAbsBox (edict_t *pent) {
  (void)pent;
  Record (__func__);
}

void SaveWriteFields (SAVERESTOREDATA *a, const char *b, void *c, TYPEDESCRIPTION *d, int e) {
  (void)a;
  (void)b;
  (void)c;
  (void)d;
  (void)e;
  Record (__func__);
}

void SaveReadFields (SAVERESTOREDATA *a, const char *b, void *c, TYPEDESCRIPTION *d, int e) {
  (void)a;
  (void)b;
  (void)c;
  (void)d;
  (void)e;
  Record (__func__);
}

void SaveGlobalState (SAVERESTOREDATA *data) {
  (void)data;
  Record (__func__);
}

void RestoreGlobalState (SAVERESTOREDATA *data) {
  (void)data;
  Record (__func__);
}

void ResetGlobalState () {
  Record (__func__);
}

int ClientConnect (edict_t *ent, const char *name, const char *addr, char reject[128]) {
  (void)ent;
  (void)addr;
  (void)reject;
  Record (__func__, name ? name : "");
  return 1;
}

void ClientDisconnect (edict_t *ent) {
  (void)ent;
  Record (__func__);
}

void ClientKill (edict_t *ent) {
  (void)ent;
  Record (__func__);
}

void ClientPutInServer (edict_t *ent) {
  (void)ent;
  Record (__func__);
}

void ClientCommand (edict_t *ent) {
  (void)ent;
  Record (__func__);
}

void ClientUserInfoChanged (edict_t *ent, char *buf) {
  (void)ent;
  (void)buf;
  Record (__func__);
}

void ServerActivate (edict_t *list, int count, int max) {
  (void)list;
  char detail[64] = {};
  snprintf (detail, sizeof (detail), "edicts=%d max=%d", count, max);
  Record (__func__, detail);
}

void ServerDeactivate () {
  Record (__func__);
}

void PlayerPreThink (edict_t *ent) {
  (void)ent;
  Record (__func__);
}

void PlayerPostThink (edict_t *ent) {
  (void)ent;
  Record (__func__);
}

void StartFrame () {
  Record (__func__);
}

void ParmsNewLevel () {
  Record (__func__);
}

void ParmsChangeLevel () {
  Record (__func__);
}

const char *GetGameDescription () {
  Record (__func__);
  return "Fake-CS";
}

void PlayerCustomization (edict_t *ent, struct customization_t *cust) {
  (void)ent;
  (void)cust;
  Record (__func__);
}

void SpectatorConnect (edict_t *ent) {
  (void)ent;
  Record (__func__);
}

void SpectatorDisconnect (edict_t *ent) {
  (void)ent;
  Record (__func__);
}

void SpectatorThink (edict_t *ent) {
  (void)ent;
  Record (__func__);
}

void SysError (const char *msg) {
  Record (__func__, msg ? msg : "");
}

void PmMove (playermove_t *pm, int server) {
  (void)pm;
  (void)server;
  Record (__func__);
}

void PmInit (playermove_t *pm) {
  (void)pm;
  Record (__func__);
}

char PmFindTextureType (char *name) {
  (void)name;
  Record (__func__);
  return 'C';
}

void SetupVisibility (struct edict_s *view, struct edict_s *client, uint8_t **pvs, uint8_t **pas) {
  (void)view;
  (void)client;
  (void)pvs;
  (void)pas;
  Record (__func__);
}

void UpdateClientData (const struct edict_s *ent, int weapons, struct clientdata_s *cd) {
  (void)ent;
  (void)weapons;
  (void)cd;
  Record (__func__);
}

int AddToFullPack (struct entity_state_s *state, int e, edict_t *ent, edict_t *host, int flags, int player, uint8_t *set) {
  (void)state;
  (void)e;
  (void)ent;
  (void)host;
  (void)flags;
  (void)player;
  (void)set;
  Record (__func__);
  return 0;
}

void CreateBaseline (int player, int eindex, struct entity_state_s *baseline, struct edict_s *entity, int model, float *mins, float *maxs) {
  (void)player;
  (void)eindex;
  (void)baseline;
  (void)entity;
  (void)model;
  (void)mins;
  (void)maxs;
  Record (__func__);
}

void RegisterEncoders () {
  Record (__func__);
}

int GetWeaponData (struct edict_s *player, struct weapon_data_s *info) {
  (void)player;
  (void)info;
  Record (__func__);
  return 0;
}

void CmdStart (const edict_t *player, usercmd_t *cmd, unsigned int seed) {
  (void)player;
  (void)cmd;
  (void)seed;
  Record (__func__);
}

void CmdEnd (const edict_t *player) {
  (void)player;
  Record (__func__);
}

int ConnectionlessPacket (const struct netadr_s *from, const char *args, char *resp, int *size) {
  (void)from;
  (void)args;
  (void)resp;
  (void)size;
  Record (__func__);
  return 0;
}

int GetHullBounds (int hull, float *mins, float *maxs) {
  (void)hull;

  if (mins) {
    mins[0] = mins[1] = mins[2] = -16.0f;
  }
  if (maxs) {
    maxs[0] = maxs[1] = 16.0f;
    maxs[2] = 36.0f;
  }
  Record (__func__);
  return 1;
}

void CreateInstancedBaselines () {
  Record (__func__);
}

int InconsistentFile (const struct edict_s *player, const char *name, char *msg) {
  (void)player;
  (void)name;
  (void)msg;
  Record (__func__);
  return 0;
}

int AllowLagCompensation () {
  Record (__func__);
  return 1;
}

void OnFreeEntPrivateData (edict_t *ent) {
  (void)ent;
  Record (__func__);
}

void GameShutdown () {
  Record (__func__);
}

int ShouldCollide (edict_t *touched, edict_t *other) {
  (void)touched;
  (void)other;
  Record (__func__);
  return 1;
}

void CvarValue (const edict_t *ent, const char *value) {
  (void)ent;
  (void)value;
  Record (__func__);
}

void CvarValue2 (const edict_t *ent, int req, const char *name, const char *value) {
  (void)ent;
  (void)req;
  (void)name;
  (void)value;
  Record (__func__);
}

} // namespace

extern "C" {

void GiveFnptrsToDll (enginefuncs_t *funcs, globalvars_t *globals) {
  (void)funcs;
  (void)globals;
  Record ("GiveFnptrsToDll");
}

int GetEntityAPI (gamefuncs_t *table, int version) {
  char detail[64] = {};
  snprintf (detail, sizeof (detail), "version=%d", version);
  Record ("GetEntityAPI", detail);

  if (!table) {
    return 0;
  }
  memset (table, 0, sizeof (*table));

  table->pfnGameInit = GameInit;
  table->pfnSpawn = Spawn;
  table->pfnThink = Think;
  table->pfnUse = Use;
  table->pfnTouch = Touch;
  table->pfnBlocked = Blocked;
  table->pfnKeyValue = KeyValue;
  table->pfnSave = Save;
  table->pfnRestore = Restore;
  table->pfnSetAbsBox = SetAbsBox;
  table->pfnSaveWriteFields = SaveWriteFields;
  table->pfnSaveReadFields = SaveReadFields;
  table->pfnSaveGlobalState = SaveGlobalState;
  table->pfnRestoreGlobalState = RestoreGlobalState;
  table->pfnResetGlobalState = ResetGlobalState;
  table->pfnClientConnect = ClientConnect;
  table->pfnClientDisconnect = ClientDisconnect;
  table->pfnClientKill = ClientKill;
  table->pfnClientPutInServer = ClientPutInServer;
  table->pfnClientCommand = ClientCommand;
  table->pfnClientUserInfoChanged = ClientUserInfoChanged;
  table->pfnServerActivate = ServerActivate;
  table->pfnServerDeactivate = ServerDeactivate;
  table->pfnPlayerPreThink = PlayerPreThink;
  table->pfnPlayerPostThink = PlayerPostThink;
  table->pfnStartFrame = StartFrame;
  table->pfnParmsNewLevel = ParmsNewLevel;
  table->pfnParmsChangeLevel = ParmsChangeLevel;
  table->pfnGetGameDescription = GetGameDescription;
  table->pfnPlayerCustomization = PlayerCustomization;
  table->pfnSpectatorConnect = SpectatorConnect;
  table->pfnSpectatorDisconnect = SpectatorDisconnect;
  table->pfnSpectatorThink = SpectatorThink;
  table->pfnSys_Error = SysError;
  table->pfnPM_Move = PmMove;
  table->pfnPM_Init = PmInit;
  table->pfnPM_FindTextureType = PmFindTextureType;
  table->pfnSetupVisibility = SetupVisibility;
  table->pfnUpdateClientData = UpdateClientData;
  table->pfnAddToFullPack = AddToFullPack;
  table->pfnCreateBaseline = CreateBaseline;
  table->pfnRegisterEncoders = RegisterEncoders;
  table->pfnGetWeaponData = GetWeaponData;
  table->pfnCmdStart = CmdStart;
  table->pfnCmdEnd = CmdEnd;
  table->pfnConnectionlessPacket = ConnectionlessPacket;
  table->pfnGetHullBounds = GetHullBounds;
  table->pfnCreateInstancedBaselines = CreateInstancedBaselines;
  table->pfnInconsistentFile = InconsistentFile;
  table->pfnAllowLagCompensation = AllowLagCompensation;

  return 1;
}

int GetNewDLLFunctions (newgamefuncs_t *table, int *version) {
  (void)version;
  Record ("GetNewDLLFunctions");

  if (!table) {
    return 0;
  }
  memset (table, 0, sizeof (*table));

  table->pfnOnFreeEntPrivateData = OnFreeEntPrivateData;
  table->pfnGameShutdown = GameShutdown;
  table->pfnShouldCollide = ShouldCollide;
  table->pfnCvarValue = CvarValue;
  table->pfnCvarValue2 = CvarValue2;

  return 1;
}

int Server_GetBlendingInterface (
  int version, struct sv_blending_interface_s **ppinterface, struct engine_studio_api_s *pstudio, float *rotationmatrix, float *bonetransform) {
  (void)version;
  (void)ppinterface;
  (void)pstudio;
  (void)rotationmatrix;
  (void)bonetransform;
  Record ("Server_GetBlendingInterface");

  return 0;
}

#if defined(FAKECS_MODERN) && FAKECS_MODERN
void weapon_famas (entvars_t *pev) {
  (void)pev;
  Record ("weapon_famas");
}
#endif

#if defined(FAKECS_REGAME) && FAKECS_REGAME
void trigger_random_unique (entvars_t *pev) {
  (void)pev;
  Record ("trigger_random_unique");
}
#endif

int FakeCS_CallCount () {
  return static_cast<int> (CallLog ().size ());
}

const char *FakeCS_CallAt (int index) {
  if (index < 0 || index >= static_cast<int> (CallLog ().size ())) {
    return "";
  }
  return CallLog ()[static_cast<size_t> (index)].chars ();
}

void FakeCS_Reset () {
  CallLog ().clear ();
  Entered () = 0;
}

int FakeCS_Entered () {
  return Entered ();
}

} // extern "C"

} // namespace bot
