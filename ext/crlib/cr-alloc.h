//
// CRLib - Simple library for STL replacement in private projects.
// Copyright © 2020 YaPB Development Team <team@yapb.ru>.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 

#pragma once

#include <new>

#include <crlib/cr-basic.h>
#include <crlib/cr-movable.h>
#include <crlib/cr-platform.h>

CR_NAMESPACE_BEGIN

// default allocator for cr-objects
class Allocator : public Singleton <Allocator> {
public:
   Allocator () = default;
   ~Allocator () = default;

public:
   template <typename T> T *allocate (const size_t length = 1) {
      auto ptr = reinterpret_cast <T *> (malloc (length * sizeof (T)));

      if (!ptr) {
         plat.abort ();
      }
      return ptr;
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
      auto d = allocate <T> ();
      new (d) T (cr::forward <Args> (args)...);

      return d;
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

CR_NAMESPACE_END
