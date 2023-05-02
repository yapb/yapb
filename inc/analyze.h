//
// YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
// Copyright © 2004-2023 YaPB Project <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

// next code is based on cs-ebot implemntation, devised by EfeDursun125
class GraphAnalyze : public Singleton <GraphAnalyze> {
public:
   GraphAnalyze () = default;
   ~GraphAnalyze () = default;

private:
   float m_updateInterval {}; // time to update analyzer

   bool m_basicsCreated {}; // basics waypoints were created?
   bool m_isCrouch {}; // is node to be created as crouch ?
   bool m_isAnalyzing {}; // we're in analyzing ?
   bool m_isAnalyzed {}; // current waypoint is analyzed
   bool m_expandedNodes[kMaxNodes] {}; // all nodes expanded ?
   bool m_optimizedNodes[kMaxNodes] {}; // all nodes expanded ?

public:
   // start analyzation process
   void start ();

   // update analyzation process
   void update ();

   // suspend aanalyzing
   void suspend ();

private:
   // flood with nodes
   void flood (const Vector &pos, const Vector &next, float range);

   // set update interval (keeps game from freezing)
   void setUpdateInterval ();

   // mark waypoints as goals
   void markGoals ();

   // terminate analyzation and save data
   void finish ();

   // optimize nodes a little
   void optimize ();

   // cleanup bad nodes
   void cleanup ();

public:

   // node should be created as crouch
   bool isCrouch () const {
      return m_isCrouch;
   }

   // is currently anaylyzing ?
   bool isAnalyzing () const {
      return m_isAnalyzing;
   }

   // current graph is analyzed graph ?
   bool isAnalyzed () const {
      return m_isAnalyzed;
   }

   // mark as optimized
   void markOptimized (const int index) {
      m_optimizedNodes[index] = true;
   }
};

// explose global
CR_EXPOSE_GLOBAL_SINGLETON (GraphAnalyze, analyzer);
