//
// YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
// Copyright © 2004-2020 YaPB Project <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#include <yapb.h>

// module interface implementation
class YaPBModule : public IYaPBModule {
private:
   Bot *getBot (int index) {
      if (index < 1) {
         return nullptr;
      }
      return bots[game.playerOfIndex (index - 1)];
   }

public:
   // get the bot version string
   virtual const char *getBotVersion () override {
      return product.version.chars ();
   }

   // checks if bots are currently in game
   virtual bool isBotsInGame () override {
      return bots.getBotCount () > 0;
   }

   // checks whether specified players is a yapb bot
   virtual bool isBot (int entity) override {
      return getBot (entity) != nullptr;
   }

   // gets the node nearest to origin
   virtual int getNearestNode (float *origin) override {
      if (graph.length () > 0) {
         return graph.getNearestNoBuckets (origin);
      }
      return kInvalidNodeIndex;
   }

   // checks wether node is valid
   virtual bool isNodeValid (int node) override {
      return graph.exists (node);
   }

   // gets the node origin
   virtual float *getNodeOrigin (int node) override {
      if (!graph.exists (node)) {
         return nullptr;
      }
      return graph[node].origin;
   }

   // get the bots current active node
   virtual int getCurrentNodeId (int entity) override {
      auto bot = getBot (entity);

      if (bot) {
         return bot->getCurrentNodeIndex ();
      }
      return kInvalidNodeIndex;
   }

   // force bot to go to the selected node
   virtual void setBotGoal (int entity, int node) override {
      if (!graph.exists (node)) {
         return;
      }
      auto bot = getBot (entity);

      if (bot) {
         return bot->sendBotToOrigin (graph[node].origin);
      }
   }

   // force bot to go to selected origin
   virtual void setBotGoalOrigin (int entity, float *origin) override {
      auto bot = getBot (entity);

      if (bot) {
         return bot->sendBotToOrigin (origin);
      }
   }
};

// export all the stuff, maybe add versioned interface ?
CR_EXPORT IYaPBModule *GetBotAPI (int version) {
   if (version != kYaPBModuleVersion) {
      return nullptr;
   }
   static YaPBModule botModule;

   return &botModule;
}
