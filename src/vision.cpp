//
// YaPB, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © YaPB Project Developers <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#include <yapb.h>

ConVar cv_max_nodes_for_predict ("yb_max_nodes_for_predict", "20", "Maximum number for path length, to predict the enemy.", true, 15.0f, 256.0f);

// game console variables
ConVar mp_flashlight ("mp_flashlight", nullptr, Var::GameRef);

float Bot::isInFOV (const Vector &destination) {
   const float entityAngle = cr::wrapAngle360 (destination.yaw ()); // find yaw angle from source to destination...
   const float viewAngle = cr::wrapAngle360 (pev->v_angle.y); // get bot's current view angle...

   // return the absolute value of angle to destination entity
   // zero degrees means straight ahead, 45 degrees to the left or
   // 45 degrees to the right is the limit of the normal view angle
   float absoluteAngle = cr::abs (viewAngle - entityAngle);

   if (absoluteAngle > 180.0f) {
      absoluteAngle = 360.0f - absoluteAngle;
   }
   return absoluteAngle;
}

bool Bot::isInViewCone (const Vector &origin) {
   // this function returns true if the spatial vector location origin is located inside
   // the field of view cone of the bot entity, false otherwise. It is assumed that entities
   // have a human-like field of view, that is, about 90 degrees.

   return util.isInViewCone (origin, ent ());
}

bool Bot::seesItem (const Vector &destination, StringRef classname) {
   TraceResult tr {};

   // trace a line from bot's eyes to destination..
   game.testLine (getEyesPos (), destination, TraceIgnore::None, ent (), &tr);

   // check if line of sight to object is not blocked (i.e. visible)
   if (tr.flFraction < 1.0f && tr.pHit && !tr.fStartSolid) {
      return classname == tr.pHit->v.classname.str ();
   }
   return true;
}

bool Bot::seesEntity (const Vector &dest, bool fromBody) {
   TraceResult tr {};

   // trace a line from bot's eyes to destination...
   game.testLine (fromBody ? pev->origin : getEyesPos (), dest, TraceIgnore::Everything, ent (), &tr);

   // check if line of sight to object is not blocked (i.e. visible)
   return tr.flFraction >= 1.0f;
}

void Bot::updateAimDir () {
   uint32_t flags = m_aimFlags;

   // don't allow bot to look at danger positions under certain circumstances
   if (!(flags & (AimFlags::Grenade | AimFlags::Enemy | AimFlags::Entity))) {

      // check if narrow place and we're duck, do not predict enemies in that situation
      const bool duckedInNarrowPlace = isInNarrowPlace () && ((m_pathFlags & NodeFlag::Crouch) || (pev->button & IN_DUCK));

      if (duckedInNarrowPlace || isOnLadder () || isInWater () || (m_pathFlags & NodeFlag::Ladder) || (m_currentTravelFlags & PathFlag::Jump)) {
         flags &= ~(AimFlags::LastEnemy | AimFlags::PredictPath);
         m_canChooseAimDirection = false;
      }
   }

   if (flags & AimFlags::Override) {
      m_lookAt = m_lookAtSafe;
   }
   else if (flags & AimFlags::Grenade) {
      m_lookAt = m_throw;

      const float throwDistance = m_throw.distance (pev->origin);
      float coordCorrection = 0.0f;

      if (throwDistance > 100.0f && throwDistance < 800.0f) {
         coordCorrection = 0.25f * (m_throw.z - pev->origin.z);
      }
      else if (throwDistance >= 800.0f) {
         float angleCorrection = 37.0f * (throwDistance - 800.0f) / 800.0f;

         if (angleCorrection > 45.0f) {
            angleCorrection = 45.0f;
         }
         coordCorrection = throwDistance * cr::tanf (cr::deg2rad (angleCorrection)) + 0.25f * (m_throw.z - pev->origin.z);
      }
      m_lookAt.z += coordCorrection * 0.5f;
   }
   else if (flags & AimFlags::Enemy) {
      focusEnemy ();
   }
   else if (flags & AimFlags::Entity) {
      m_lookAt = m_entity;

      // do not look at hostages legs
      if (m_pickupType == Pickup::Hostage) {
         m_lookAt.z += 48.0f;
      }
      else if (m_pickupType == Pickup::Weapon) {
         m_lookAt.z += 72.0f;
      }
   }
   else if (flags & AimFlags::LastEnemy) {
      m_lookAt = m_lastEnemyOrigin;

      // did bot just see enemy and is quite aggressive?
      if (m_seeEnemyTime + 2.0f - m_actualReactionTime + m_baseAgressionLevel > game.time ()) {

         // feel free to fire if shootable
         if (!usesSniper () && lastEnemyShootable ()) {
            m_wantsToFire = true;
         }
      }
   }
   else if (flags & AimFlags::PredictPath) {
      bool changePredictedEnemy = true;

      if (m_timeNextTracking < game.time () && m_trackingEdict == m_lastEnemy && util.isAlive (m_lastEnemy)) {
         changePredictedEnemy = false;
      }

      auto doFailPredict = [this] () -> void {
         if (m_timeNextTracking > game.time ()) {
            return; // do not fail instantly
         }
         m_aimFlags &= ~AimFlags::PredictPath;
         m_trackingEdict = nullptr;
         m_lookAtPredict = nullptr;
      };

      auto isPredictedIndexApplicable = [this] () -> bool {
         int pathLength = m_lastPredictLength;
         int predictNode = m_lastPredictIndex;

         if (predictNode != kInvalidNodeIndex) {
            if (!vistab.visible (m_currentNodeIndex, predictNode)) {
               predictNode = kInvalidNodeIndex;
            }
         }
         return predictNode != kInvalidNodeIndex && pathLength < cv_max_nodes_for_predict.int_ ();
      };

      if (changePredictedEnemy) {
         if (isPredictedIndexApplicable ()) {
            m_lookAtPredict = graph[m_lastPredictIndex].origin;

            m_timeNextTracking = game.time () + rg.get (0.5f, 1.0f);
            m_trackingEdict = m_lastEnemy;

            // feel free to fire if shootable
            if (!usesSniper () && lastEnemyShootable ()) {
               m_wantsToFire = true;
            }
         }
         else {
            doFailPredict ();
         }
      }
      else {
         if (!isPredictedIndexApplicable ()) {
            doFailPredict ();
         }
      }

      if (!m_lookAtPredict.empty ()) {
         m_lookAt = m_lookAtPredict;
      }
   }
   else if (flags & AimFlags::Camp) {
      m_lookAt = m_lookAtSafe;
   }
   else if (flags & AimFlags::Nav) {
      m_lookAt = m_destOrigin;

      if (m_moveToGoal && m_seeEnemyTime + 4.0f < game.time () && !m_isStuck && m_moveSpeed > getShiftSpeed () && !(pev->button & IN_DUCK) && m_currentNodeIndex != kInvalidNodeIndex && !(m_pathFlags & (NodeFlag::Ladder | NodeFlag::Crouch)) && m_pathWalk.hasNext () && pev->origin.distanceSq (m_destOrigin) < cr::sqrf (512.0f)) {
         auto nextPathIndex = m_pathWalk.next ();

         if (vistab.visible (m_currentNodeIndex, nextPathIndex)) {
            m_lookAt = graph[nextPathIndex].origin + pev->view_ofs;
         }
         else {
            m_lookAt = m_destOrigin;
         }
      }
      else if (m_seeEnemyTime + 3.0f > game.time () && !m_lastEnemyOrigin.empty ()) {
         m_lookAt = m_lastEnemyOrigin;
      }
      else {
         m_lookAt = m_destOrigin;
      }
      const bool onLadder = (m_pathFlags & NodeFlag::Ladder);

      if (m_canChooseAimDirection && m_seeEnemyTime + 4.0f < game.time () && m_currentNodeIndex != kInvalidNodeIndex && !onLadder) {
         const auto dangerIndex = practice.getIndex (m_team, m_currentNodeIndex, m_currentNodeIndex);

         if (graph.exists (dangerIndex) && vistab.visible (m_currentNodeIndex, dangerIndex) && !(graph[dangerIndex].flags & NodeFlag::Crouch)) {
            if (pev->origin.distanceSq (graph[dangerIndex].origin) < cr::sqrf (512.0f)) {
               m_lookAt = m_destOrigin;
            }
            else {
               m_lookAt = graph[dangerIndex].origin + pev->view_ofs;

               // add danger flags
               m_aimFlags |= AimFlags::Danger;
            }
         }
      }

      // try look at next node if on ladder
      if (onLadder && m_pathWalk.hasNext ()) {
         const auto &nextPath = graph[m_pathWalk.next ()];

         if ((nextPath.flags & NodeFlag::Ladder) && m_destOrigin.distanceSq (pev->origin) < cr::sqrf (120.0f) && nextPath.origin.z > m_pathOrigin.z + 45.0f) {
            m_lookAt = nextPath.origin;
         }
      }

      // don't look at bottom of node, if reached it
      if (m_lookAt == m_destOrigin && !onLadder) {
         m_lookAt.z = getEyesPos ().z;
      }
   }

   if (m_lookAt.empty ()) {
      m_lookAt = m_destOrigin;
   }
}

void Bot::checkDarkness () {

   // do not check for darkness at the start of the round
   if (m_spawnTime + 5.0f > game.time () || !graph.exists (m_currentNodeIndex)) {
      return;
   }

   // do not check every frame
   if (m_checkDarkTime > game.time () || cr::fzero (m_path->light)) {
      return;
   }

   const auto skyColor = illum.getSkyColor ();
   const auto flashOn = (pev->effects & EF_DIMLIGHT);

   if (mp_flashlight.bool_ () && !m_hasNVG) {
      const auto task = getCurrentTaskId ();

      if (!flashOn && task != Task::Camp && task != Task::Attack && m_heardSoundTime + 3.0f < game.time () && m_flashLevel > 30 && ((skyColor > 50.0f && m_path->light < 10.0f) || (skyColor <= 50.0f && m_path->light < 40.0f))) {
         pev->impulse = 100;
      }
      else if (flashOn && (((m_path->light > 15.0f && skyColor > 50.0f) || (m_path->light > 45.0f && skyColor <= 50.0f)) || task == Task::Camp || task == Task::Attack || m_flashLevel <= 0 || m_heardSoundTime + 3.0f >= game.time ())) {
         pev->impulse = 100;
      }
   }
   else if (m_hasNVG) {
      if (flashOn) {
         pev->impulse = 100;
      }
      else if (!m_usesNVG && ((skyColor > 50.0f && m_path->light < 15.0f) || (skyColor <= 50.0f && m_path->light < 40.0f))) {
         issueCommand ("nightvision");
      }
      else if (m_usesNVG && ((m_path->light > 20.0f && skyColor > 50.0f) || (m_path->light > 45.0f && skyColor <= 50.0f))) {
         issueCommand ("nightvision");
      }
   }
   m_checkDarkTime = game.time () + rg.get (2.0f, 4.0f);
}


void Bot::calculateFrustum () {
   // this function updates bot view frustum

   Vector forward, right, up;
   pev->v_angle.angleVectors (&forward, &right, &up);

   static Vector fc, nc, fbl, fbr, ftl, ftr, nbl, nbr, ntl, ntr;

   fc = getEyesPos () + forward * frustum.MaxView;
   nc = getEyesPos () + forward * frustum.MinView;

   fbl = fc + (up * frustum.farHeight * 0.5f) - (right * frustum.farWidth * 0.5f);
   fbr = fc + (up * frustum.farHeight * 0.5f) + (right * frustum.farWidth * 0.5f);
   ftl = fc - (up * frustum.farHeight * 0.5f) - (right * frustum.farWidth * 0.5f);
   ftr = fc - (up * frustum.farHeight * 0.5f) + (right * frustum.farWidth * 0.5f);
   nbl = nc + (up * frustum.nearHeight * 0.5f) - (right * frustum.nearWidth * 0.5f);
   nbr = nc + (up * frustum.nearHeight * 0.5f) + (right * frustum.nearWidth * 0.5f);
   ntl = nc - (up * frustum.nearHeight * 0.5f) - (right * frustum.nearWidth * 0.5f);
   ntr = nc - (up * frustum.nearHeight * 0.5f) + (right * frustum.nearWidth * 0.5f);

   auto setPlane = [&] (FrustumSide side, const Vector &v1, const Vector &v2, const Vector &v3) {
      auto &plane = m_frustum[side];

      plane.normal = ((v2 - v1) ^ (v3 - v1)).normalize ();
      plane.point = v2;

      plane.result = -(plane.normal | plane.point);
   };

   setPlane (FrustumSide::Top, ftl, ntl, ntr);
   setPlane (FrustumSide::Bottom, fbr, nbr, nbl);
   setPlane (FrustumSide::Left, fbl, nbl, ntl);
   setPlane (FrustumSide::Right, ftr, ntr, nbr);
   setPlane (FrustumSide::Near, nbr, ntr, ntl);
   setPlane (FrustumSide::Far, fbl, ftl, ftr);
}

bool Bot::isEnemyInFrustum (edict_t *enemy) {
   const Vector &origin = enemy->v.origin - Vector (0.0f, 0.0f, 5.0f);

   for (auto &plane : m_frustum) {
      if (!util.isObjectInsidePlane (plane, origin, 60.0f, 16.0f)) {
         return false;
      }
   }
   return true;
}

void Bot::updateBodyAngles () {
   // set the body angles to point the gun correctly
   pev->angles.x = -pev->v_angle.x * (1.0f / 3.0f);
   pev->angles.y = pev->v_angle.y;

   pev->angles.clampAngles ();

   // calculate frustum plane data here, since look angles update functions call this last one
   calculateFrustum ();
}

void Bot::updateLookAngles () {
   const float delta = cr::clamp (game.time () - m_lookUpdateTime, cr::kFloatEqualEpsilon, 1.0f / 30.0f);
   m_lookUpdateTime = game.time ();

   // adjust all body and view angles to face an absolute vector
   Vector direction = (m_lookAt - getEyesPos ()).angles ();
   direction.x = -direction.x; // invert for engine

   direction.clampAngles ();

   // lower skilled bot's have lower aiming
   if (m_difficulty == Difficulty::Noob) {
      updateLookAnglesNewbie (direction, delta);
      updateBodyAngles ();

      return;
   }

   float accelerate = 3000.0f;
   float stiffness = 200.0f;
   float damping = 25.0f;

   if (((m_aimFlags & (AimFlags::Enemy | AimFlags::Entity | AimFlags::Grenade)) || m_wantsToFire) && m_difficulty > Difficulty::Normal) {
      if (m_difficulty == Difficulty::Expert) {
         accelerate += 600.0f;
      }
      stiffness += 100.0f;
      damping -= 5.0f;
   }
   m_idealAngles = pev->v_angle;

   const float angleDiffPitch = cr::anglesDifference (direction.x, m_idealAngles.x);
   const float angleDiffYaw = cr::anglesDifference (direction.y, m_idealAngles.y);

   if (angleDiffYaw < 1.0f && angleDiffYaw > -1.0f) {
      m_lookYawVel = 0.0f;
      m_idealAngles.y = direction.y;
   }
   else {
      const float accel = cr::clamp (stiffness * angleDiffYaw - damping * m_lookYawVel, -accelerate, accelerate);

      m_lookYawVel += delta * accel;
      m_idealAngles.y += delta * m_lookYawVel;
   }
   const float accel = cr::clamp (2.0f * stiffness * angleDiffPitch - damping * m_lookPitchVel, -accelerate, accelerate);

   m_lookPitchVel += delta * accel;
   m_idealAngles.x += cr::clamp (delta * m_lookPitchVel, -89.0f, 89.0f);

   pev->v_angle = m_idealAngles;
   pev->v_angle.clampAngles ();

   updateBodyAngles ();
}

void Bot::updateLookAnglesNewbie (const Vector &direction, float delta) {
   Vector spring { 13.0f, 13.0f, 0.0f };
   Vector damperCoefficient { 0.22f, 0.22f, 0.0f };

   const float offset = cr::clamp (static_cast <float> (m_difficulty), 1.0f, 4.0f) * 25.0f;

   Vector influence = Vector (0.25f, 0.17f, 0.0f) * (100.0f - offset) / 100.f;
   Vector randomization = Vector (2.0f, 0.18f, 0.0f) * (100.0f - offset) / 100.f;

   const float noTargetRatio = 0.3f;
   const float offsetDelay = 1.2f;

   Vector stiffness;
   Vector randomize;

   m_idealAngles = direction.get2d ();
   m_idealAngles.clampAngles ();

   if (m_aimFlags & (AimFlags::Enemy | AimFlags::Entity)) {
      m_playerTargetTime = game.time ();
      m_randomizedIdealAngles = m_idealAngles;

      stiffness = spring * (0.2f + offset / 125.0f);
   }
   else {
      // is it time for bot to randomize the aim direction again (more often where moving) ?
      if (m_randomizeAnglesTime < game.time () && ((pev->velocity.length () > 1.0f && m_angularDeviation.length () < 5.0f) || m_angularDeviation.length () < 1.0f)) {
         // is the bot standing still ?
         if (pev->velocity.length () < 1.0f) {
            randomize = randomization * 0.2f; // randomize less
         }
         else {
            randomize = randomization;
         }
         // randomize targeted location bit (slightly towards the ground)
         m_randomizedIdealAngles = m_idealAngles + Vector (rg.get (-randomize.x * 0.5f, randomize.x * 1.5f), rg.get (-randomize.y, randomize.y), 0.0f);

         // set next time to do this
         m_randomizeAnglesTime = game.time () + rg.get (0.4f, offsetDelay);
      }
      float stiffnessMultiplier = noTargetRatio;

      // take in account whether the bot was targeting someone in the last N seconds
      if (game.time () - (m_playerTargetTime + offsetDelay) < noTargetRatio * 10.0f) {
         stiffnessMultiplier = 1.0f - (game.time () - m_timeLastFired) * 0.1f;

         // don't allow that stiffness multiplier less than zero
         if (stiffnessMultiplier < 0.0f) {
            stiffnessMultiplier = 0.5f;
         }
      }

      // also take in account the remaining deviation (slow down the aiming in the last 10°)
      stiffnessMultiplier *= m_angularDeviation.length () * 0.1f * 0.5f;

      // but don't allow getting below a certain value
      if (stiffnessMultiplier < 0.35f) {
         stiffnessMultiplier = 0.35f;
      }
      stiffness = spring * stiffnessMultiplier; // increasingly slow aim
   }
   // compute randomized angle deviation this time
   m_angularDeviation = m_randomizedIdealAngles - pev->v_angle;
   m_angularDeviation.clampAngles ();

   // spring/damper model aiming
   m_aimSpeed.x = stiffness.x * m_angularDeviation.x - damperCoefficient.x * m_aimSpeed.x;
   m_aimSpeed.y = stiffness.y * m_angularDeviation.y - damperCoefficient.y * m_aimSpeed.y;

   // influence of y movement on x axis and vice versa (less influence than x on y since it's
   // easier and more natural for the bot to "move its mouse" horizontally than vertically)
   m_aimSpeed.x += cr::clamp (m_aimSpeed.y * influence.y, -50.0f, 50.0f);
   m_aimSpeed.y += cr::clamp (m_aimSpeed.x * influence.x, -200.0f, 200.0f);

   // move the aim cursor
   pev->v_angle = pev->v_angle + delta * Vector (m_aimSpeed.x, m_aimSpeed.y, 0.0f);
   pev->v_angle.clampAngles ();
}
