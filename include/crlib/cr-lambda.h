//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) Yet Another POD-Bot Contributors <yapb@entix.io>.
//
// This software is licensed under the MIT license.
// Additional exceptions apply. For full license details, see LICENSE.txt
//

#pragma once

#include <crlib/cr-alloc.h>
#include <crlib/cr-uniqueptr.h>

CR_NAMESPACE_BEGIN

template <typename> class Lambda;
template <typename R, typename ...Args> class Lambda <R (Args...)> {
private:
   enum : uint32 {
      LamdaSmallBufferLength = sizeof (void *) * 16
   };

private:
   class LambdaFunctorWrapper {
   public:
      LambdaFunctorWrapper () = default;
      virtual ~LambdaFunctorWrapper () = default;

   public:
      virtual void move (uint8 *to) = 0;
      virtual void small (uint8 *to) const = 0;

      virtual R invoke (Args &&...) = 0;
      virtual UniquePtr <LambdaFunctorWrapper> clone () const = 0;

   public:
      void operator delete (void *ptr) {
         alloc.deallocate (ptr);
      }
   };

   template <typename T> class LambdaFunctor : public LambdaFunctorWrapper {
   private:
      T m_callable;

   public:
      LambdaFunctor (const T &callable) : LambdaFunctorWrapper (), m_callable (callable)
      { }

      LambdaFunctor (T &&callable) : LambdaFunctorWrapper (), m_callable (cr::move (callable))
      { }

      ~LambdaFunctor () override = default;

   public:
      void move (uint8 *to) override {
         new (to) LambdaFunctor<T> (cr::move (m_callable));
      }

      void small (uint8 *to) const override {
         new (to) LambdaFunctor<T> (m_callable);
      }

      R invoke (Args &&... args) override {
         return m_callable (cr::forward <Args> (args)...);
      }

      UniquePtr <LambdaFunctorWrapper> clone () const override {
         return makeUnique <LambdaFunctor <T>> (m_callable);
      }
   };

   union {
      UniquePtr <LambdaFunctorWrapper> m_functor;
      uint8 m_small[LamdaSmallBufferLength];
   };

   bool m_smallObject;

private:
   void destroy () {
      if (m_smallObject) {
         reinterpret_cast <LambdaFunctorWrapper *> (m_small)->~LambdaFunctorWrapper ();
      }
      else {
         m_functor.reset ();
      }
   }

   void swap (Lambda &rhs) noexcept {
      cr::swap (rhs, *this);
   }

public:
   explicit Lambda () noexcept : Lambda (nullptr) 
   { }

   Lambda (decltype (nullptr)) noexcept : m_functor (nullptr), m_smallObject (false)
   { }

   Lambda (const Lambda &rhs) {
      if (rhs.m_smallObject) {
         reinterpret_cast <const LambdaFunctorWrapper *> (rhs.m_small)->small (m_small);
      }
      else {
         new (m_small) UniquePtr <LambdaFunctorWrapper> (rhs.m_functor->clone ());
      }
      m_smallObject = rhs.m_smallObject;
   }

   Lambda (Lambda &&rhs) noexcept {
      if (rhs.m_smallObject) {
         reinterpret_cast <LambdaFunctorWrapper *> (rhs.m_small)->move (m_small);
         new (rhs.m_small) UniquePtr <LambdaFunctorWrapper> (nullptr);
      }
      else {
         new (m_small) UniquePtr <LambdaFunctorWrapper> (cr::move (rhs.m_functor));
      }
      m_smallObject = rhs.m_smallObject;
      rhs.m_smallObject = false;
   }

   template <typename F> Lambda (F function) {
      if (cr::fix (sizeof (function) > LamdaSmallBufferLength)) {
         m_smallObject = false;
         new (m_small) UniquePtr <LambdaFunctorWrapper> (makeUnique <LambdaFunctor <F>> (cr::move (function)));
      }
      else {
         m_smallObject = true;
         new (m_small) LambdaFunctor<F> (cr::move (function));
      }
   }

   ~Lambda () {
      destroy ();
   }

public:
   Lambda &operator = (const Lambda &rhs) {
      destroy ();

      Lambda tmp (rhs);
      swap (tmp);

      return *this;
   }

   Lambda &operator = (Lambda &&rhs) noexcept {
      destroy ();

      if (rhs.m_smallObject) {
         reinterpret_cast <LambdaFunctorWrapper *> (rhs.m_small)->move (m_small);
         new (rhs.m_small) UniquePtr <LambdaFunctorWrapper> (nullptr);
      }
      else {
         new (m_small) UniquePtr <LambdaFunctorWrapper> (cr::move (rhs.m_functor));
      }

      m_smallObject = rhs.m_smallObject;
      rhs.m_smallObject = false;

      return *this;
   }

   explicit operator bool () const noexcept {
      return m_smallObject || !!m_functor;
   }

public:
   R operator () (Args ...args) {
      return m_smallObject ? reinterpret_cast <LambdaFunctorWrapper *> (m_small)->invoke (cr::forward <Args> (args)...) : m_functor->invoke (cr::forward <Args> (args)...);
   }
};

CR_NAMESPACE_END