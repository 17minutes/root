// @(#)root/r:$Id$
// Author: Omar Zapata   29/05/2013


/*************************************************************************
 * Copyright (C) 2013-2014, Omar Andres Zapata Mesa                      *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/
#ifndef ROOT_R_RExports
#define ROOT_R_RExports
//ROOT headers
#include <Rtypes.h>

#include <TString.h>

#include <TVector.h>

#include <TMatrixT.h>

#include <TArrayD.h>

#include <TArrayF.h>

#include <TArrayI.h>

//std headers
#include<string>
#include<vector>
//support for std c++11 classes
// #if __cplusplus > 199711L
#include<array>
// #endif

//pragma to disable warnings on Rcpp which have
//so many noise compiling
#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#endif


//Some useful typedefs
typedef std::vector<TString> TVectorString;

#define ROOT_R_RAW_PTR_PROTOTYPE(Type)       \
   typedef Type *Type##ptr;                  \
   typedef ROOT::R::TRPtr<Type> TRPtr##Type; \
   template <>                               \
   SEXP wrap(const TRPtr##Type &o);          \
   template <>                               \
   TRPtr##Type as(SEXP);                     \
   template <>                               \
   SEXP wrap(const Type##ptr &o);            \
   template <>                               \
   Type##ptr as(SEXP);

#define ROOT_R_EXPORT_PTR(Type)              \
   namespace Rcpp {                          \
   typedef Type *Type##ptr;                  \
   typedef ROOT::R::TRPtr<Type> TRPtr##Type; \
   template <>                               \
   SEXP wrap(const TRPtr##Type &o)           \
   {                                         \
      return (SEXP)o;                        \
   }                                         \
   template <>                               \
   TRPtr##Type as(SEXP o)                    \
   {                                         \
      return TRPtr##Type(o);                 \
   }                                         \
   template <>                               \
   SEXP wrap(const Type##ptr &o)             \
   {                                         \
      return TRPtr##Type(o);                 \
   }                                         \
   template <>                               \
   Type##ptr as(SEXP o)                      \
   {                                         \
      return TRPtr##Type(o).As();            \
   }                                         \
   }

#include<RcppCommon.h>
#include <TRPtr.h>
namespace ROOT {
   namespace R {
      class TRFunctionExport;
      class TRFunctionImport;
      class TRDataFrame;
      class TRObject;
   }
}

namespace Rcpp {

//TString
   template<> inline SEXP wrap(const TString &s)
   {
      return wrap(std::string(s.Data()));
   }
   template<> inline TString as(SEXP s)
   {
      return TString(::Rcpp::as<std::string>(s).c_str());
   }

   // Bool_t
   template <>
   inline SEXP wrap(const Bool_t &s)
   {
      return wrap((unsigned int)s);
   }
   template <>
   inline Bool_t as(SEXP s)
   {
      return ::Rcpp::as<unsigned int>(s);
   }
   // Char_t
   template <>
   inline SEXP wrap(const Char_t &s)
   {
      TString str(s);
      return wrap(str);
   }

   // TVectorT
   template<> SEXP wrap(const TVectorT<Double_t> &v);
   template<> TVectorT<Double_t> as(SEXP v);

   template<> SEXP wrap(const TVectorT<Float_t> &v);
   template<> TVectorT<Float_t> as(SEXP v);

//TMatrixT
   template<> SEXP wrap(const TMatrixT<Double_t> &m);
   template<> TMatrixT<Double_t> as(SEXP) ;
   template<> SEXP wrap(const TMatrixT<Float_t> &m);
   template<> TMatrixT<Float_t> as(SEXP) ;

//TRDataFrame
   template<> SEXP wrap(const ROOT::R::TRDataFrame &o);
   template<> ROOT::R::TRDataFrame as(SEXP) ;

//TRObject
   template<> SEXP wrap(const ROOT::R::TRObject &o);
   template<> ROOT::R::TRObject as(SEXP) ;

   // TRPtr
   ROOT_R_RAW_PTR_PROTOTYPE(Double_t)
   ROOT_R_RAW_PTR_PROTOTYPE(Float_t)
   ROOT_R_RAW_PTR_PROTOTYPE(Int_t)
   ROOT_R_RAW_PTR_PROTOTYPE(Char_t)
   ROOT_R_RAW_PTR_PROTOTYPE(Option_t)

   // TRFunctionImport
   template<> SEXP wrap(const ROOT::R::TRFunctionImport &o);
   template<> ROOT::R::TRFunctionImport as(SEXP) ;

   template<class T, size_t i> std::array<T, i> as(SEXP &obj)
   {
      std::vector<T> v = Rcpp::as<std::vector<T> >(obj);
      std::array<T, i> a;
      std::copy(v.begin(), v.end(), a.begin());
      return a;
   }

   namespace traits {
      template <typename T, size_t i>
      class Exporter<std::array<T, i> > {
      public:
         Exporter(SEXP x)
         {
            t = Rcpp::as<T, i>(x);
         }
         std::array<T, i> get()
         {
            return t;
         }
      private:
         std::array<T, i> t;
      } ;
   }
}
//added to fix bug in last version of Rcpp on mac
#if !defined(R_Version)
#define R_Version(v,p,s) ((v * 65536) + (p * 256) + (s))
#endif
#include<Rcpp.h>//this headers should be called after templates definitions
#include<Rcpp/Named.h>
#undef HAVE_UINTPTR_T
#include<RInside.h>

namespace ROOT {
   namespace R {
      //reference to internal ROOTR's Module that call ROOT's classes in R
      extern  VARIABLE_IS_NOT_USED SEXP ModuleSymRef;
      template<class T> class class_: public Rcpp::class_<T> {
      public:
         class_(const Char_t *name_, const Char_t *doc = 0) : Rcpp::class_<T>(name_, doc) {}
      };

      //________________________________________________________________________________________________________
      template <class T>
      void function(const Char_t *name_, T fun, const Char_t *docstring = 0)
      {
         //template function required to create modules using the macro ROOTR_MODULE
         Rcpp::function(name_, fun, docstring);
      }

      extern Rcpp::internal::NamedPlaceHolder Label;
   }
}


//macros redifined to be accord with the namespace
#define ROOTR_MODULE RCPP_MODULE
#define ROOTR_EXPOSED_CLASS RCPP_EXPOSED_CLASS

//modified definiton to support ROOTR namespace
#define ROOTR_EXPOSED_CLASS_INTERNAL(CLASS)\
   namespace ROOT{                         \
      namespace R{                         \
         class CLASS;                      \
      }}                                   \
   RCPP_EXPOSED_CLASS_NODECL(ROOT::R::CLASS)



//modified macro for ROOTR global Module Object Symbol Reference ROOT::R::ModuleSymRef
#define LOAD_ROOTR_MODULE(NAME) Rf_eval( Rf_lang2( ( ROOT::R::ModuleSymRef == NULL ? ROOT::R::ModuleSymRef = Rf_install("Module") : ROOT::R::ModuleSymRef ), _rcpp_module_boot_##NAME() ), R_GlobalEnv )
#endif
