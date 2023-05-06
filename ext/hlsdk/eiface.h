/***
 *
 *   Copyright (c) 1999-2005, Valve Corporation. All rights reserved.
 *
 *   This product contains software technology licensed from Id
 *   Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
 *   All Rights Reserved.
 *
 *   This source code contains proprietary and confidential information of
 *   Valve LLC and its suppliers.  Access to this code is restricted to
 *   persons who have executed a written SDK license with Valve.  Any access,
 *   use or distribution of this code by or to any unlicensed person is illegal.
 *
 ****/
#pragma once

struct cvar_t {
   char *name;
   char *string;
   int flags;
   float value;
   cvar_t *next;
};

typedef struct hudtextparms_s {
   float x;
   float y;
   int effect;
   uint8_t r1, g1, b1, a1;
   uint8_t r2, g2, b2, a2;
   float fadeinTime;
   float fadeoutTime;
   float holdTime;
   float fxTime;
   int channel;
} hudtextparms_t;

// Returned by TraceLine
typedef struct {
   int fAllSolid; // if true, plane is not valid
   int fStartSolid; // if true, the initial point was in a solid area
   int fInOpen;
   int fInWater;
   float flFraction; // time completed, 1.0 = didn't hit anything
   vec3_t vecEndPos; // final position
   float flPlaneDist;
   vec3_t vecPlaneNormal; // surface normal at impact
   edict_t *pHit; // entity the surface is on
   int iHitgroup; // 0 == generic, non zero is specific body part
} TraceResult;

typedef struct usercmd_s {
   short	lerp_msec;      // Interpolation time on client
   byte	msec;           // Duration in ms of command
   vec3_t	viewangles;     // Command view angles.

   // intended velocities
   float	forwardmove;    // Forward velocity.
   float	sidemove;       // Sideways velocity.
   float	upmove;         // Upward velocity.
   byte	lightlevel;     // Light level at spot where we are standing.
   unsigned short  buttons;  // Attack buttons
   byte    impulse;          // Impulse command issued.
   byte	weaponselect;	// Current weapon id

   // Experimental player impact stuff.
   int		impact_index;
   vec3_t	impact_position;
} usercmd_t;

typedef uint32_t CRC32_t;

// Engine hands this to DLLs for functionality callbacks

typedef struct enginefuncs_s {
   int (*pfnPrecacheModel) (const char *s);
   int (*pfnPrecacheSound) (const char *s);
   void (*pfnSetModel) (edict_t *e, const char *m);
   int (*pfnModelIndex) (const char *m);
   int (*pfnModelFrames) (int modelIndex);
   void (*pfnSetSize) (edict_t *e, const float *rgflMin, const float *rgflMax);
   void (*pfnChangeLevel) (char *s1, char *s2);
   void (*pfnGetSpawnParms) (edict_t *ent);
   void (*pfnSaveSpawnParms) (edict_t *ent);
   float (*pfnVecToYaw) (const float *rgflVector);
   void (*pfnVecToAngles) (const float *rgflVectorIn, float *rgflVectorOut);
   void (*pfnMoveToOrigin) (edict_t *ent, const float *pflGoal, float dist, int iMoveType);
   void (*pfnChangeYaw) (edict_t *ent);
   void (*pfnChangePitch) (edict_t *ent);
   edict_t *(*pfnFindEntityByString) (edict_t *pentEdictStartSearchAfter, const char *pszField, const char *pszValue);
   int (*pfnGetEntityIllum) (edict_t *pEnt);
   edict_t *(*pfnFindEntityInSphere) (edict_t *pentEdictStartSearchAfter, const float *org, float rad);
   edict_t *(*pfnFindClientInPVS) (edict_t *ent);
   edict_t *(*pfnEntitiesInPVS) (edict_t *pplayer);
   void (*pfnMakeVectors) (const float *rgflVector);
   void (*pfnAngleVectors) (const float *rgflVector, float *forward, float *right, float *up);
   edict_t *(*pfnCreateEntity) ();
   void (*pfnRemoveEntity) (edict_t *e);
   edict_t *(*pfnCreateNamedEntity) (int className);
   void (*pfnMakeStatic) (edict_t *ent);
   int (*pfnEntIsOnFloor) (edict_t *e);
   int (*pfnDropToFloor) (edict_t *e);
   int (*pfnWalkMove) (edict_t *ent, float yaw, float dist, int mode);
   void (*pfnSetOrigin) (edict_t *e, const float *rgflOrigin);
   void (*pfnEmitSound) (edict_t *entity, int channel, const char *sample, float volume, float attenuation, int fFlags, int pitch);
   void (*pfnEmitAmbientSound) (edict_t *entity, float *pos, const char *samp, float vol, float attenuation, int fFlags, int pitch);
   void (*pfnTraceLine) (const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr);
   void (*pfnTraceToss) (edict_t *pent, edict_t *pentToIgnore, TraceResult *ptr);
   int (*pfnTraceMonsterHull) (edict_t *ent, const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr);
   void (*pfnTraceHull) (const float *v1, const float *v2, int fNoMonsters, int hullNumber, edict_t *pentToSkip, TraceResult *ptr);
   void (*pfnTraceModel) (const float *v1, const float *v2, int hullNumber, edict_t *pent, TraceResult *ptr);
   const char *(*pfnTraceTexture) (edict_t *pTextureEntity, const float *v1, const float *v2);
   void (*pfnTraceSphere) (const float *v1, const float *v2, int fNoMonsters, float radius, edict_t *pentToSkip, TraceResult *ptr);
   void (*pfnGetAimVector) (edict_t *ent, float speed, float *rgflReturn);
   void (*pfnServerCommand) (char *str);
   void (*pfnServerExecute) ();
   void (*pfnClientCommand) (edict_t *ent, char const *szFmt, ...);
   void (*pfnParticleEffect) (const float *org, const float *dir, float color, float count);
   void (*pfnLightStyle) (int style, char *val);
   int (*pfnDecalIndex) (const char *name);
   int (*pfnPointContents) (const float *rgflVector);
   void (*pfnMessageBegin) (int msg_dest, int msg_type, const float *pOrigin, edict_t *ed);
   void (*pfnMessageEnd) ();
   void (*pfnWriteByte) (int value);
   void (*pfnWriteChar) (int value);
   void (*pfnWriteShort) (int value);
   void (*pfnWriteLong) (int value);
   void (*pfnWriteAngle) (float flValue);
   void (*pfnWriteCoord) (float flValue);
   void (*pfnWriteString) (const char *sz);
   void (*pfnWriteEntity) (int value);
   void (*pfnCVarRegister) (cvar_t *pCvar);
   float (*pfnCVarGetFloat) (const char *szVarName);
   const char *(*pfnCVarGetString) (const char *szVarName);
   void (*pfnCVarSetFloat) (const char *szVarName, float flValue);
   void (*pfnCVarSetString) (const char *szVarName, const char *szValue);
   void (*pfnAlertMessage) (ALERT_TYPE atype, const char *szFmt, ...);
   void (*pfnEngineFprintf) (void *pfile, char *szFmt, ...);
   void *(*pfnPvAllocEntPrivateData) (edict_t *ent, int32_t cb);
   void *(*pfnPvEntPrivateData) (edict_t *ent);
   void (*pfnFreeEntPrivateData) (edict_t *ent);
   const char *(*pfnSzFromIndex) (int stingPtr);
   int (*pfnAllocString) (const char *szValue);
   struct entvars_s *(*pfnGetVarsOfEnt) (edict_t *ent);
   edict_t *(*pfnPEntityOfEntOffset) (int iEntOffset);
   int (*pfnEntOffsetOfPEntity) (const edict_t *ent);
   int (*pfnIndexOfEdict) (const edict_t *ent);
   edict_t *(*pfnPEntityOfEntIndex) (int entIndex);
   edict_t *(*pfnFindEntityByVars) (struct entvars_s *pvars);
   void *(*pfnGetModelPtr) (edict_t *ent);
   int (*pfnRegUserMsg) (const char *pszName, int iSize);
   void (*pfnAnimationAutomove) (const edict_t *ent, float flTime);
   void (*pfnGetBonePosition) (const edict_t *ent, int iBone, float *rgflOrigin, float *rgflAngles);
   uint32_t (*pfnFunctionFromName) (const char *pName);
   const char *(*pfnNameForFunction) (uint32_t function);
   void (*pfnClientPrintf) (edict_t *ent, PRINT_TYPE ptype, const char *szMsg); // JOHN: engine callbacks so game DLL can print messages to individual clients
   void (*pfnServerPrint) (const char *szMsg);
   const char *(*pfnCmd_Args) (); // these 3 added
   const char *(*pfnCmd_Argv) (int argc); // so game DLL can easily
   int (*pfnCmd_Argc) (); // access client 'cmd' strings
   void (*pfnGetAttachment) (const edict_t *ent, int iAttachment, float *rgflOrigin, float *rgflAngles);
   void (*pfnCRC32_Init) (CRC32_t *pulCRC);
   void (*pfnCRC32_ProcessBuffer) (CRC32_t *pulCRC, void *p, int len);
   void (*pfnCRC32_ProcessByte) (CRC32_t *pulCRC, uint8_t ch);
   CRC32_t (*pfnCRC32_Final) (CRC32_t pulCRC);
   int32_t (*pfnRandomLong) (int32_t lLow, int32_t lHigh);
   float (*pfnRandomFloat) (float flLow, float flHigh);
   void (*pfnSetView) (const edict_t *client, const edict_t *pViewent);
   float (*pfnTime) ();
   void (*pfnCrosshairAngle) (const edict_t *client, float pitch, float yaw);
   uint8_t *(*pfnLoadFileForMe) (char const *szFilename, int *pLength);
   void (*pfnFreeFile) (void *buffer);
   void (*pfnEndSection) (const char *pszSectionName); // trigger_endsection
   int (*pfnCompareFileTime) (char *filename1, char *filename2, int *compare);
   void (*pfnGetGameDir) (char *szGetGameDir);
   void (*pfnCvar_RegisterVariable) (cvar_t *variable);
   void (*pfnFadeClientVolume) (const edict_t *ent, int fadePercent, int fadeOutSeconds, int holdTime, int fadeInSeconds);
   void (*pfnSetClientMaxspeed) (const edict_t *ent, float fNewMaxspeed);
   edict_t *(*pfnCreateFakeClient) (const char *netname); // returns nullptr if fake client can't be created
   void (*pfnRunPlayerMove) (edict_t *fakeclient, const float *viewangles, float forwardmove, float sidemove, float upmove, uint16_t buttons, uint8_t impulse, uint8_t msec);
   int (*pfnNumberOfEntities) ();
   char *(*pfnGetInfoKeyBuffer) (edict_t *e); // passing in nullptr gets the serverinfo
   char *(*pfnInfoKeyValue) (char *infobuffer, char const *key);
   void (*pfnSetKeyValue) (char *infobuffer, char *key, char *value);
   void (*pfnSetClientKeyValue) (int clientIndex, char *infobuffer, char const *key, char const *value);
   int (*pfnIsMapValid) (char *szFilename);
   void (*pfnStaticDecal) (const float *origin, int decalIndex, int entityIndex, int modelIndex);
   int (*pfnPrecacheGeneric) (char *s);
   int (*pfnGetPlayerUserId) (edict_t *e); // returns the server assigned userid for this player.  useful for logging frags, etc.  returns -1 if the edict couldn't be found in the list of clients
   void (*pfnBuildSoundMsg) (edict_t *entity, int channel, const char *sample, float volume, float attenuation, int fFlags, int pitch, int msg_dest, int msg_type, const float *pOrigin, edict_t *ed);
   int (*pfnIsDedicatedServer) (); // is this a dedicated server?
   cvar_t *(*pfnCVarGetPointer) (const char *szVarName);
   unsigned int (*pfnGetPlayerWONId) (edict_t *e); // returns the server assigned WONid for this player.  useful for logging frags, etc.  returns -1 if the edict couldn't be found in the list of clients

   void (*pfnInfo_RemoveKey) (char *s, const char *key);
   const char *(*pfnGetPhysicsKeyValue) (const edict_t *client, const char *key);
   void (*pfnSetPhysicsKeyValue) (const edict_t *client, const char *key, const char *value);
   const char *(*pfnGetPhysicsInfoString) (const edict_t *client);
   uint16_t (*pfnPrecacheEvent) (int type, const char *psz);
   void (*pfnPlaybackEvent) (int flags, const edict_t *pInvoker, uint16_t evIndexOfEntity, float delay, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2);
   uint8_t *(*pfnSetFatPVS) (float *org);
   uint8_t *(*pfnSetFatPAS) (float *org);
   int (*pfnCheckVisibility) (const edict_t *entity, uint8_t *pset);
   void (*pfnDeltaSetField) (struct delta_s *pFields, const char *fieldname);
   void (*pfnDeltaUnsetField) (struct delta_s *pFields, const char *fieldname);
   void (*pfnDeltaAddEncoder) (char *name, void (*conditionalencode) (struct delta_s *pFields, const uint8_t *from, const uint8_t *to));
   int (*pfnGetCurrentPlayer) ();
   int (*pfnCanSkipPlayer) (const edict_t *player);
   int (*pfnDeltaFindField) (struct delta_s *pFields, const char *fieldname);
   void (*pfnDeltaSetFieldByIndex) (struct delta_s *pFields, int fieldNumber);
   void (*pfnDeltaUnsetFieldByIndex) (struct delta_s *pFields, int fieldNumber);
   void (*pfnSetGroupMask) (int mask, int op);
   int (*pfnCreateInstancedBaseline) (int classname, struct entity_state_s *baseline);
   void (*pfnCvar_DirectSet) (struct cvar_t *var, const char *value);
   void (*pfnForceUnmodified) (FORCE_TYPE type, float *mins, float *maxs, const char *szFilename);
   void (*pfnGetPlayerStats) (const edict_t *client, int *ping, int *packet_loss);
   void (*pfnAddServerCommand) (const char *cmd_name, void (*function) ());

   int (*pfnVoice_GetClientListening) (int iReceiver, int iSender);
   int (*pfnVoice_SetClientListening) (int iReceiver, int iSender, int bListen);

   const char *(*pfnGetPlayerAuthId) (edict_t *e);

   struct sequenceEntry_s *(*pfnSequenceGet) (const char *fileName, const char *entryName);
   struct sentenceEntry_s *(*pfnSequencePickSentence) (const char *groupName, int pickMethod, int *picked);

   int (*pfnGetFileSize) (char *szFilename);
   unsigned int (*pfnGetApproxWavePlayLen) (const char *filepath);

   int (*pfnIsCareerMatch) ();
   int (*pfnGetLocalizedStringLength) (const char *label);
   void (*pfnRegisterTutorMessageShown) (int mid);
   int (*pfnGetTimesTutorMessageShown) (int mid);
   void (*pfnProcessTutorMessageDecayBuffer) (int *buffer, int bufferLength);
   void (*pfnConstructTutorMessageDecayBuffer) (int *buffer, int bufferLength);
   void (*pfnResetTutorMessageDecayData) ();

   void (*pfnQueryClientCVarValue) (const edict_t *player, const char *cvarName);
   void (*pfnQueryClientCVarValue2) (const edict_t *player, const char *cvarName, int requestID);
   int (*pfnCheckParm) (const char *pchCmdLineToken, char **ppnext);
   // edict_t *(*pfnPEntityOfEntIndexAllEntities) (int iEntIndex);
} enginefuncs_t;

// Passed to pfnKeyValue
typedef struct KeyValueData_s {
   char *szClassName; // in: entity classname
   char const *szKeyName; // in: name of key
   char *szValue; // in: value of key
   int32_t fHandled; // out: DLL sets to true if key-value pair was understood
} KeyValueData;

typedef struct customization_s customization_t;

typedef struct {
   // Initialize/shutdown the game (one-time call after loading of game .dll )
   void (*pfnGameInit) ();
   int (*pfnSpawn) (edict_t *pent);
   void (*pfnThink) (edict_t *pent);
   void (*pfnUse) (edict_t *pentUsed, edict_t *pentOther);
   void (*pfnTouch) (edict_t *pentTouched, edict_t *pentOther);
   void (*pfnBlocked) (edict_t *pentBlocked, edict_t *pentOther);
   void (*pfnKeyValue) (edict_t *pentKeyvalue, KeyValueData *pkvd);
   void (*pfnSave) (edict_t *pent, struct SAVERESTOREDATA *pSaveData);
   int (*pfnRestore) (edict_t *pent, SAVERESTOREDATA *pSaveData, int globalEntity);
   void (*pfnSetAbsBox) (edict_t *pent);

   void (*pfnSaveWriteFields) (SAVERESTOREDATA *, const char *, void *, struct TYPEDESCRIPTION *, int);
   void (*pfnSaveReadFields) (SAVERESTOREDATA *, const char *, void *, TYPEDESCRIPTION *, int);

   void (*pfnSaveGlobalState) (SAVERESTOREDATA *);
   void (*pfnRestoreGlobalState) (SAVERESTOREDATA *);
   void (*pfnResetGlobalState) ();

   int (*pfnClientConnect) (edict_t *ent, const char *pszName, const char *pszAddress, char szRejectReason[128]);

   void (*pfnClientDisconnect) (edict_t *ent);
   void (*pfnClientKill) (edict_t *ent);
   void (*pfnClientPutInServer) (edict_t *ent);
   void (*pfnClientCommand) (edict_t *ent);
   void (*pfnClientUserInfoChanged) (edict_t *ent, char *infobuffer);

   void (*pfnServerActivate) (edict_t *edictList, int edictCount, int clientMax);
   void (*pfnServerDeactivate) ();

   void (*pfnPlayerPreThink) (edict_t *ent);
   void (*pfnPlayerPostThink) (edict_t *ent);

   void (*pfnStartFrame) ();
   void (*pfnParmsNewLevel) ();
   void (*pfnParmsChangeLevel) ();

   // Returns string describing current .dll.  E.g., TeamFotrress 2, Half-Life
   const char *(*pfnGetGameDescription) ();

   // Notify dll about a player customization.
   void (*pfnPlayerCustomization) (edict_t *ent, struct customization_s *pCustom);

   // Spectator funcs
   void (*pfnSpectatorConnect) (edict_t *ent);
   void (*pfnSpectatorDisconnect) (edict_t *ent);
   void (*pfnSpectatorThink) (edict_t *ent);

   // Notify game .dll that engine is going to shut down.  Allows mod authors to set a breakpoint.
   void (*pfnSys_Error) (const char *error_string);

   void (*pfnPM_Move) (struct playermove_s *ppmove, int server);
   void (*pfnPM_Init) (struct playermove_s *ppmove);
   char (*pfnPM_FindTextureType) (char *name);
   void (*pfnSetupVisibility) (struct edict_s *pViewEntity, struct edict_s *client, uint8_t **pvs, uint8_t **pas);
   void (*pfnUpdateClientData) (const struct edict_s *ent, int sendweapons, struct clientdata_s *cd);
   int (*pfnAddToFullPack) (struct entity_state_s *state, int e, edict_t *ent, edict_t *host, int hostflags, int player, uint8_t *pSet);
   void (*pfnCreateBaseline) (int player, int eindex, struct entity_state_s *baseline, struct edict_s *entity, int playermodelindex, float *player_mins, float *player_maxs);
   void (*pfnRegisterEncoders) ();
   int (*pfnGetWeaponData) (struct edict_s *player, struct weapon_data_s *info);

   void (*pfnCmdStart) (const edict_t *player, usercmd_t *cmd, unsigned int random_seed);
   void (*pfnCmdEnd) (const edict_t *player);

   // Return 1 if the packet is valid.  Set response_buffer_size if you want to send a response packet.  Incoming, it holds the max
   //  size of the response_buffer, so you must zero it out if you choose not to respond.
   int (*pfnConnectionlessPacket) (const struct netadr_s *net_from, const char *args, char *response_buffer, int *response_buffer_size);

   // Enumerates player hulls.  Returns 0 if the hull number doesn't exist, 1 otherwise
   int (*pfnGetHullBounds) (int hullnumber, float *mins, float *maxs);

   // Create baselines for certain "unplaced" items.
   void (*pfnCreateInstancedBaselines) ();

   // One of the pfnForceUnmodified files failed the consistency check for the specified player
   // Return 0 to allow the client to continue, 1 to force immediate disconnection ( with an optional disconnect message of up to 256 characters )
   int (*pfnInconsistentFile) (const struct edict_s *player, const char *szFilename, char *disconnect_message);

   // The game .dll should return 1 if lag compensation should be allowed ( could also just set
   //  the sv_unlag cvar.
   // Most games right now should return 0, until client-side weapon prediction code is written
   //  and tested for them.
   int (*pfnAllowLagCompensation) ();
} gamefuncs_t;

typedef struct {
   // Called right before the object's memory is freed.
   // Calls its destructor.
   void (*pfnOnFreeEntPrivateData) (edict_t *pEnt);
   void (*pfnGameShutdown) ();
   int (*pfnShouldCollide) (edict_t *pentTouched, edict_t *pentOther);

   void (*pfnCvarValue) (const edict_t *pEnt, const char *value);
   void (*pfnCvarValue2) (const edict_t *pEnt, int requestID, const char *cvarName, const char *value);
} newgamefuncs_t;
