//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     http://yapb.jeefo.net/license
//
// Purpose: Represents Counter-Strike Game.
//

#pragma once

// netmessage handler class
class NetworkMsg
{
private:
   Bot *m_bot;
   int m_state;
   int m_message;
   int m_registerdMessages[NETMSG_NUM];

public:
   NetworkMsg (void);
   ~NetworkMsg (void) { };

   void Execute (void *p);
   inline void Reset (void) { m_message = NETMSG_UNDEFINED; m_state = 0; m_bot = NULL; };
   void HandleMessageIfRequired (int messageType, int requiredType);

   inline void SetMessage (int message) { m_message = message; }
   inline void SetBot (Bot *bot) { m_bot = bot; }

   inline int GetId (int messageType) { return m_registerdMessages[messageType]; }
   inline void SetId (int messageType, int messsageIdentifier) { m_registerdMessages[messageType] = messsageIdentifier; }
};

// game events
class IGameEvents
{
public:

   // called when new round occurs
   virtual void OnNewRound (void) = 0;
};

class Game
{
private:
   IGameEvents *m_listener;

public:
   Game (void) : m_listener (NULL) { }
   ~Game (void) { }

public:
   
   // we have only one listener, so register it here
   void RegisterEventListener (IGameEvents *listener) { m_listener = listener; }

   // get events interface
   IGameEvents *GetGameEventListener (void) { return m_listener; }
};

// helper macros
#define events game.GetGameEventListener ()