//
// YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
// Copyright © 2004-2020 YaPB Project <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <crlib/cr-memory.h>
#include <crlib/cr-platform.h>

void *operator new (size_t size) {
   return cr::Memory::get <void *> (size);
}

void *operator new [] (size_t size) {
   return cr::Memory::get <void *> (size);
}

void operator delete (void *ptr) noexcept {
   cr::Memory::release (ptr);
}

void operator delete [] (void *ptr) noexcept {
   cr::Memory::release (ptr);
}

void operator delete (void *ptr, size_t) noexcept {
   cr::Memory::release (ptr);
}

void operator delete [] (void *ptr, size_t) noexcept {
   cr::Memory::release (ptr);
}

CR_LINKAGE_C void __cxa_pure_virtual () {
   cr::plat.abort ("pure virtual function call");
}
