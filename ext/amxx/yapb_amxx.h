//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

#include <ystl/ystl.h>
#include <module.h>

// minimal amx mod x module abi, self-contained (no sdk)
namespace bot {

using cell = int32_t;

// amx native calling convention, mirrors the amx mod x sdk
#if defined(_MSC_VER)
  #define AMX_NATIVE_CALL __cdecl
#else
  #define AMX_NATIVE_CALL
#endif

struct AMX;

constexpr int kAmxxInterfaceVersion = 4;

// module interface result codes
enum class AmxxStatus : int {
  Ok = 0,
  IfVers = 1,
  Param = 2,
  FuncNotPresent = 3,
};

// check game result codes
enum class AmxxGameStatus : int {
  Ok = 0,
  Bad = 1,
};

// amx error codes we raise
enum class AmxError : int {
  Native = 10,
};

using AMX_NATIVE = cell (AMX_NATIVE_CALL *) (AMX *, cell *);

struct AMX_NATIVE_INFO {
  const char *name;
  AMX_NATIVE func;
};

struct AmxxModuleInfo {
  const char *name;
  const char *author;
  const char *version;
  int reload;
  const char *logtag;
  const char *library;
  const char *libclass;
};

using PFN_REQ_FNPTR = void *(*)(const char *);

// amxx core functions the module needs
using FnAddNatives = int (*) (const AMX_NATIVE_INFO *);
using FnGetModname = const char *(*)();
using FnLog = void (*) (const char *, ...);
using FnLogError = void (*) (AMX *, int, const char *, ...);
using FnGetAmxAddr = cell *(*)(AMX *, cell);
using FnSetAmxString = int (*) (AMX *, cell, const char *, int);
using FnCellToReal = float (*) (cell);
using FnRealToCell = cell (*) (float);

// amxx core bindings, filled once at attach
class AmxxCore final : public ystl::Singleton<AmxxCore> {
public:
  bool Bind (PFN_REQ_FNPTR request);

public:
  // variadic, so call the pointers directly
  FnLog log_ = nullptr;
  FnLogError log_error_ = nullptr;

public:
  int AddNatives (const AMX_NATIVE_INFO *list) {
    return add_natives_ (list);
  }

  const char *GetModname () {
    return get_modname_ ();
  }

  cell *GetAmxAddr (AMX *amx, cell offset) {
    return get_amx_addr_ (amx, offset);
  }

  int SetAmxString (AMX *amx, cell address, const char *source, int max) {
    return set_amx_string_ (amx, address, source, max);
  }

  float CellToReal (cell value) {
    return cell_to_real_ (value);
  }

  cell RealToCell (float value) {
    return real_to_cell_ (value);
  }

  // read/write a pawn Float:vec[3] through the cell encoding
  void ReadVector (AMX *amx, cell address, float *out) {
    auto cells = get_amx_addr_ (amx, address);

    for (size_t i = 0; i < 3; ++i) {
      out[i] = cell_to_real_ (cells[i]);
    }
  }

  void WriteVector (AMX *amx, cell address, const float *in) {
    auto cells = get_amx_addr_ (amx, address);

    for (size_t i = 0; i < 3; ++i) {
      cells[i] = real_to_cell_ (in[i]);
    }
  }

private:
  FnAddNatives add_natives_ = nullptr;
  FnGetModname get_modname_ = nullptr;
  FnGetAmxAddr get_amx_addr_ = nullptr;
  FnSetAmxString set_amx_string_ = nullptr;
  FnCellToReal cell_to_real_ = nullptr;
  FnRealToCell real_to_cell_ = nullptr;
};

YSTL_EXPOSE_GLOBAL_SINGLETON (AmxxCore, amxx);

// loads the bot dll and forwards the natives to it
class YaPBModule final : public ystl::Singleton<YaPBModule> {
private:
  using Export = IBotModule *(*)(int);

private:
  ystl::SharedLibrary botdll_;

  IBotModule *api_ = nullptr;

public:
  YaPBModule () = default;
  ~YaPBModule () = default;

public:
  void Load ();
  void Unload ();
  void DisableNatives ();

public:
  IBotModule *Api () {
    return api_;
  }
};

YSTL_EXPOSE_GLOBAL_SINGLETON (YaPBModule, yapb);

} // namespace bot
