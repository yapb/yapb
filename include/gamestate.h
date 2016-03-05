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

//
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