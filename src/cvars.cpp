//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include <yapb.h>

namespace bot {

void ConVar::Revert () {
  if (!ptr) {
    return;
  }

  for (const auto &var : cvars.Get ()) {
    if (var.name == ptr->name) {
      Set (var.value.chars ());
      break;
    }
  }
}

ConVar::ConVar (const ConVarDesc &desc) {
  SetRegistrationData (desc);
  CvarRegistry::instance ().Add (this);
}

void CvarRegistry::Add (ConVar *cv) {
  if (cv) {
    pending_.push (cv);
  }
}

void CvarRegistry::Push (const ConVarSpec &spec, ConVar *self) {
  ConVarReg reg {};

  // engine-visible pointers aim at ConVar POD members: assigned once, never relocated
  reg.reg.name = spec.name.chars ();
  reg.reg.string = spec.value.chars ();
  reg.reg.desc = spec.info.chars ();
  reg.name = spec.name;
  reg.reg_missing = spec.reg_missing;
  reg.value = spec.value;
  reg.info = spec.info;
  reg.bounded = spec.bounded;

  if (!spec.reg_val.empty ()) {
    reg.reg_val = spec.reg_val;
  }

  if (spec.bounded) {
    reg.min = spec.min;
    reg.max = spec.max;
    reg.initial = spec.value.as<float> ();
  }

  int eflags = FCVAR_EXTDLL;

  if (spec.type == Var::Normal) {
    eflags |= FCVAR_SERVER;
  }
  else if (spec.type == Var::ReadOnly) {
    eflags |= FCVAR_SERVER | FCVAR_SPONLY | FCVAR_PRINTABLEONLY;
  }
  else if (spec.type == Var::Password) {
    eflags |= FCVAR_PROTECTED;
  }

  reg.reg.flags = eflags;
  reg.self = self;
  reg.type = spec.type;

  registry_.push (ystl::move (reg));
}

void CvarRegistry::Register (bool game_vars) {
  for (size_t i = registry_.size (); i < pending_.size (); ++i) {
    Push (pending_[i]->Spec (), pending_[i]);
  }

  for (auto &var : registry_) {
    ConVar &self = *var.self;
    ConVarEngineReg &reg = var.reg;

    if (var.type != Var::GameRef) {
      if (var.type == Var::Xash3D && !game.Is (GameFlags::Xash3D)) {
        continue;
      }

      // engine may drop external cvars on restart, trust its lookup over our cached pointer
      self.ptr = engfuncs.pfnCVarGetPointer (reg.name);

      if (!self.ptr) {
        // restore stable sources, the engine replaced them on previous registration
        reg.name = self.RegName ();
        reg.string = self.RegValue ();

        // xash3d extended cvar marker, sized against the pointer width
        constexpr uintptr_t kSentinel = sizeof (void *) == 8 ? static_cast<uintptr_t> (0xDEADBEEFDEADBEEFULL) : static_cast<uintptr_t> (0xDEADBEEFu);

        // xash3d picks the description up when next holds the sentinel
        if (game.Is (GameFlags::Xash3D) && !ystl::strings.is_empty (reg.desc)) {
          reg.next = reinterpret_cast<void *> (kSentinel);
        }

        // fix metamod' memlocs not found
        if (game.Is (GameFlags::Metamod)) {
          static ConVarEngineReg metamod_rg {};
          metamod_rg = var.reg;

          engfuncs.pfnCVarRegister (reinterpret_cast<cvar_t *> (&metamod_rg));
        }
        else {
          engfuncs.pfnCVarRegister (reinterpret_cast<cvar_t *> (&var.reg));
        }
        self.ptr = engfuncs.pfnCVarGetPointer (reg.name);

        if (!self.ptr) {
          ystl::logger.error ("Got nullptr on cvar %s!", reg.name);
        }
      }
    }
    else {
      self.ptr = engfuncs.pfnCVarGetPointer (reg.name);

      if (game_vars && var.reg_missing && !self.ptr) {
        // restore stable sources in case the engine was restarted
        reg.name = self.RegName ();

        const char *ref_value = self.RegRefValue ();
        if (ref_value[0] != '\0') {
          reg.string = ref_value;
          reg.flags |= FCVAR_SERVER;
        }
        else {
          reg.string = self.RegValue ();
        }
        engfuncs.pfnCVarRegister (reinterpret_cast<cvar_t *> (&var.reg));
        self.ptr = engfuncs.pfnCVarGetPointer (reg.name);
      }

      if (game_vars && !self.ptr) {
        ystl::logger.error ("Got nullptr on cvar %s!", reg.name);
      }
    }
  }
}

void CvarRegistry::CheckBounds () {
  for (const auto &var : registry_) {
    if (!var.self || !var.self->ptr) {
      continue;
    }

    // read only cvar is not changeable
    if (var.type == Var::ReadOnly && !var.value.empty ()) {
      if (var.value != var.self->As<ystl::StringRef> ()) {
        var.self->Set (var.value.chars ());
      }
      continue;
    }

    if (!var.bounded || !var.self) {
      continue;
    }
    auto value = var.self->As<float> ();
    const auto str = var.self->As<ystl::StringRef> ();

    // check the bounds and set default if out of bounds
    if (value > var.max || value < var.min || (!str.empty () && isalpha (str[0]))) {
      var.self->Set (var.initial);

      // notify about that
      ctrl.Msg ("Bogus value for cvar '%s', min is '%.1f' and max is '%.1f', and we're got '%s', value reverted to default '%.1f'.", var.name,
        var.min, var.max, str, var.initial);
      continue;
    }

    // prevent min/max problems
    const auto max_pos = var.name.find ("_max");

    if (max_pos != ystl::StringRef::InvalidIndex) {
      const auto var_view = ystl::StringRef (var.name.chars (), var.name.size ());

      for (auto &mv : registry_) {
        if (mv.name.size () != var.name.size ()) {
          continue;
        }
        const auto min_pos = mv.name.find ("_min");

        if (min_pos != max_pos) {
          continue;
        }
        const auto mv_view = ystl::StringRef (mv.name.chars (), mv.name.size ());

        if (mv_view.substr (0, max_pos) == var_view.substr (0, max_pos) && mv_view.substr (max_pos + 4) == var_view.substr (max_pos + 4)) {
          const auto min_value = mv.self->As<float> ();

          if (min_value > value) {
            var.self->Set (min_value);
            mv.self->Set (value);

            ctrl.Msg ("Bogus value for min/max cvar '%s' can't be higher than '%s'. Values swapped.", mv.name, var.name);
          }
        }
      }
    }
  }

  // force simulating on xash3d so startframe runs without players
  if (game.Is (GameFlags::Xash3DLegacy)) {
    static ConVarRef sv_forcesimulating ("sv_forcesimulating");

    if (sv_forcesimulating.Exists () && !ystl::fequal (sv_forcesimulating.Value (), 1.0f)) {
      game.Print ("Force-enable Xash3D sv_forcesimulating cvar.");
      sv_forcesimulating.Set ("1.0");
    }
  }
}

void CvarRegistry::SetDescription (const ConVar &cv, ystl::StringRef info) {
  for (auto &var : registry_) {
    if (var.name == cv.Name ()) {
      var.info = info;
      break;
    }
  }
}

} // namespace bot
