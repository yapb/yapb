//
// YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
// Copyright © 2004-2020 YaPB Project <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <crlib/cr-basic.h>
#include <crlib/cr-movable.h>
#include <crlib/cr-platform.h>

// provide placment new to avoid stdc++ <new> header
inline void *operator new (const size_t, void *ptr) noexcept {
   return ptr;
}

CR_NAMESPACE_BEGIN

// default allocator for cr-objects
class Allocator : public Singleton <Allocator> {
public:
   Allocator () = default;
   ~Allocator () = default;

public:
   template <typename T> T *allocate (const size_t length = 1) {
      auto memory = reinterpret_cast <T *> (malloc (cr::max <size_t> (1u, length * sizeof (T))));

      if (!memory) {
         plat.abort ();
      }
      return memory;
   }

   template <typename T> void deallocate (T *memory) {
      free (memory);
      memory = nullptr;
   }

public:
   template <typename T, typename ...Args> void construct (T *memory, Args &&...args) {
      new (memory) T (cr::forward <Args> (args)...);
   }

   template <typename T> void destruct (T *memory) {
      memory->~T ();
   }

public:
   template <typename T, typename ...Args> T *create (Args &&...args) {
      auto memory = allocate <T> ();
      new (memory) T (cr::forward <Args> (args)...);

      return memory;
   }

   template <typename T> T *createArray (const size_t amount) {
      auto memory = allocate <T> (amount);

      for (size_t i = 0; i < amount; ++i) {
         new (memory + i) T ();
      }
      return memory;
   }


   template <typename T> void destroy (T *memory) {
      if (memory) {
         destruct (memory);
         deallocate (memory);
      }
   }
};

CR_EXPOSE_GLOBAL_SINGLETON (Allocator, alloc);

template <typename T> class UniquePtr;

// implment singleton with UniquePtr
template <typename T> T &Singleton <T>::instance () {
   static const UniquePtr <T> instance_ { alloc.create <T> () };
   return *instance_;
}

// declare destructor for pure-virtual classes
#define CR_DECLARE_DESTRUCTOR()        \
   void operator delete (void *ptr) {  \
      alloc.deallocate (ptr);          \
   }                                   \

CR_NAMESPACE_END
