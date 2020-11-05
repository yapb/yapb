//
// YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
// Copyright © 2004-2020 YaPB Project <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

// AMX Mod X Module API Version (bump if interface changed)
constexpr int kYaPBModuleVersion = 1;

// basic module interface, if you need to additional stuff, please post an issue
class IYaPBModule {
public:
   // get the bot version string
   virtual const char *getBotVersion () = 0;

   // checks if bots are currently in game
   virtual bool isBotsInGame () = 0;

   // checks whether specified players is a yapb bot
   virtual bool isBot (int entity) = 0;

   // gets the node nearest to origin
   virtual int getNearestNode (const Vector &origin) = 0;

   // checks wether node is valid
   virtual bool isNodeValid (int node) = 0;

   // gets the node origin
   virtual float *getNodeOrigin (int node) = 0;

   // get the bots current active node
   virtual int getCurrentNodeId (int entity) = 0;

   // force bot to go to the selected node
   virtual void setBotGoal (int entity, int node) = 0;

   // force bot to go to selected origin
   virtual void setBotGoalOrigin (int entity, float *origin) = 0;
};