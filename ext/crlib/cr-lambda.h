//
// YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
// Copyright © 2004-2020 YaPB Project <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <crlib/cr-memory.h>
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
   };

   template <typename T> class LambdaFunctor : public LambdaFunctorWrapper {
   private:
      T callable_;

   public:
      LambdaFunctor (const T &callable) : LambdaFunctorWrapper (), callable_ (callable)
      { }

      LambdaFunctor (T &&callable) : LambdaFunctorWrapper (), callable_ (cr::move (callable))
      { }

      ~LambdaFunctor () override = default;

   public:
      void move (uint8 *to) override {
         new (to) LambdaFunctor <T> (cr::move (callable_));
      }

      void small (uint8 *to) const override {
         new (to) LambdaFunctor <T> (callable_);
      }

      R invoke (Args &&... args) override {
         return callable_ (cr::forward <Args> (args)...);
      }

      UniquePtr <LambdaFunctorWrapper> clone () const override {
         return makeUnique <LambdaFunctor <T>> (callable_);
      }
   };

   union {
      UniquePtr <LambdaFunctorWrapper> functor_;
      uint8 small_[LamdaSmallBufferLength] { };
   };

   bool ssoObject_ = false;

private:
   void destroy () {
      if (ssoObject_) {
         reinterpret_cast <LambdaFunctorWrapper *> (small_)->~LambdaFunctorWrapper ();
      }
      else {
         functor_.reset ();
      }
   }

   void swap (Lambda &rhs) noexcept {
      cr::swap (rhs, *this);
   }

public:
   explicit Lambda () noexcept : Lambda (nullptr)
   { }

   Lambda (decltype (nullptr)) noexcept : functor_ (nullptr), ssoObject_ (false)
   { }

   Lambda (const Lambda &rhs) {
      if (rhs.ssoObject_) {
         reinterpret_cast <const LambdaFunctorWrapper *> (rhs.small_)->small (small_);
      }
      else {
         new (small_) UniquePtr <LambdaFunctorWrapper> (rhs.functor_->clone ());
      }
      ssoObject_ = rhs.ssoObject_;
   }

   Lambda (Lambda &&rhs) noexcept {
      if (rhs.ssoObject_) {
         reinterpret_cast <LambdaFunctorWrapper *> (rhs.small_)->move (small_);
         new (rhs.small_) UniquePtr <LambdaFunctorWrapper> (nullptr);
      }
      else {
         new (small_) UniquePtr <LambdaFunctorWrapper> (cr::move (rhs.functor_));
      }
      ssoObject_ = rhs.ssoObject_;
      rhs.ssoObject_ = false;
   }

   template <typename F> Lambda (F function) {
      if (cr::fix (sizeof (function) > LamdaSmallBufferLength)) {
         ssoObject_ = false;
         new (small_) UniquePtr <LambdaFunctorWrapper> (makeUnique <LambdaFunctor <F>> (cr::move (function)));
      }
      else {
         ssoObject_ = true;
         new (small_) LambdaFunctor <F> (cr::move (function));
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

      if (rhs.ssoObject_) {
         reinterpret_cast <LambdaFunctorWrapper *> (rhs.small_)->move (small_);
         new (rhs.small_) UniquePtr <LambdaFunctorWrapper> (nullptr);
      }
      else {
         new (small_) UniquePtr <LambdaFunctorWrapper> (cr::move (rhs.functor_));
      }

      ssoObject_ = rhs.ssoObject_;
      rhs.ssoObject_ = false;

      return *this;
   }

   explicit operator bool () const noexcept {
      return ssoObject_ || !!functor_;
   }

public:
   R operator () (Args ...args) {
      return ssoObject_ ? reinterpret_cast <LambdaFunctorWrapper *> (small_)->invoke (cr::forward <Args> (args)...) : functor_->invoke (cr::forward <Args> (args)...);
   }
};

CR_NAMESPACE_END
