// @(#)root/cont:$Id$
// Author: Fons Rademakers   11/08/95

/*************************************************************************
 * Copyright (C) 1995-2000, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// This class registers for all classes their name, id and dictionary   //
// function in a hash table. Classes are automatically added by the     //
// ctor of a special init class when a global of this init class is     //
// initialized when the program starts (see the ClassImp macro).        //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "RConfig.h"
#include <stdlib.h>
#include <string>
#include <map>
#include <typeinfo>
#include "Riostream.h"
#include <memory>

#include "TClassTable.h"
#include "TClass.h"
#include "TClassEdit.h"
#include "TProtoClass.h"
#include "TROOT.h"
#include "TString.h"
#include "TError.h"
#include "TRegexp.h"

#include "TObjString.h"
#include "TMap.h"

#include "TInterpreter.h"
using namespace ROOT;

TClassTable *gClassTable;

TClassAlt  **TClassTable::fgAlternate;
TClassRec  **TClassTable::fgTable;
TClassRec  **TClassTable::fgSortedTable;
UInt_t       TClassTable::fgSize;
UInt_t       TClassTable::fgTally;
Bool_t       TClassTable::fgSorted;
UInt_t       TClassTable::fgCursor;
TClassTable::IdMap_t *TClassTable::fgIdMap;

ClassImp(TClassTable)

//______________________________________________________________________________
namespace ROOT {

   class TClassRec {
   public:
      TClassRec(TClassRec *next) :
        fName(0), fId(0), fDict(0), fInfo(0), fProto(0), fNext(next)
      {}

      ~TClassRec() {
         // TClassTable::fgIdMap->Remove(r->fInfo->name());
         delete [] fName;
         delete fProto;
         delete fNext;
      }

      char            *fName;
      Version_t        fId;
      Int_t            fBits;
      DictFuncPtr_t    fDict;
      const type_info *fInfo;
      TProtoClass     *fProto;
      TClassRec       *fNext;
   };

   class TClassAlt {
   public:
      TClassAlt(const char*alternate, const char *normName, TClassAlt *next) :
         fName(alternate), fNormName(normName), fNext(next)
      {}

      ~TClassAlt() {
         // Nothing more to delete.
      }

      const char *fName;     // Do not own
      const char *fNormName; // Do not own
      std::unique_ptr<TClassAlt> fNext;
   };


   class TMapTypeToClassRec {
#if defined R__USE_STD_MAP
     // This wrapper class allow to avoid putting #include <map> in the
     // TROOT.h header file.
   public:
#ifdef R__GLOBALSTL
      typedef std::map<string, TClassRec*>           IdMap_t;
#else
      typedef std::map<std::string, TClassRec*> IdMap_t;
#endif
      typedef IdMap_t::key_type                 key_type;
      typedef IdMap_t::const_iterator           const_iterator;
      typedef IdMap_t::size_type                size_type;
#ifdef R__WIN32
      // Window's std::map does NOT defined mapped_type
      typedef TClassRec*                        mapped_type;
#else
      typedef IdMap_t::mapped_type              mapped_type;
#endif

   private:
      IdMap_t fMap;

   public:
      void Add(const key_type &key, mapped_type &obj) {
         fMap[key] = obj;
      }

      mapped_type Find(const key_type &key) const {

         IdMap_t::const_iterator iter = fMap.find(key);
         mapped_type cl = 0;
         if (iter != fMap.end()) cl = iter->second;
         return cl;
      }

      void Remove(const key_type &key) { fMap.erase(key); }

      void Print() {
         Info("TMapTypeToClassRec::Print", "printing the typeinfo map in TClassTable");
         for (const_iterator iter = fMap.begin(); iter != fMap.end(); iter++) {
            printf("Key: %40s 0x%lx\n", iter->first.c_str(), iter->second);
         }
      }
#else
   private:
      TMap fMap;
   public:
#ifdef R__COMPLETE_MEM_TERMINATION
      ~TMapTypeToClassRec() {
         TIter next(&fMap);
         TObjString *key;
         while((key = (TObjString*)next())) {
            delete key;
         }
      }
#endif

      void Add(const char *key, TClassRec *&obj)
      {
         // Add <key,value> pair to the map.

         TObjString *realkey = new TObjString(key);
         fMap.Add(realkey, (TObject*)obj);
      }

      TClassRec *Find(const char *key) const {
         // Find the value corresponding the key.
         const TPair *a = (const TPair *)fMap.FindObject(key);
         if (a) return (TClassRec*) a->Value();
         return 0;
      }

      void Remove(const char *key) {
         // Remove the value corresponding the key.
         TObjString realkey(key);
         TObject *actual = fMap.Remove(&realkey);
         delete actual;
      }

      void Print() {
         // Print the content of the map.
         Info("TMapTypeToClassRec::Print", "printing the typeinfo map in TClassTable");
         TIter next(&fMap);
         TObjString *key;
         while((key = (TObjString*)next())) {
            printf("Key: %s\n",key->String().Data());
            TClassRec *data = (TClassRec*)fMap.GetValue(key);
            if (data) {
               printf("  class: %s %d\n",data->fName,data->fId);
            } else {
               printf("  no class: \n");
            }
         }
      }
#endif
   };

   static UInt_t ClassTableHash(const char *name, UInt_t size)
   {
      auto p = reinterpret_cast<const unsigned char*>( name );
      UInt_t slot = 0;

      while (*p) slot = slot<<1 ^ *p++;
      slot %= size;

      return slot;
   }
}

//______________________________________________________________________________
TClassTable::TClassTable()
{
   // TClassTable is a singleton (i.e. only one can exist per application).

   if (gClassTable) return;

   fgSize  = 1009;  //this is the result of (int)TMath::NextPrime(1000);
   fgTable = new TClassRec* [fgSize];
   fgAlternate = new TClassAlt* [fgSize];
   fgIdMap = new IdMap_t;
   memset(fgTable, 0, fgSize*sizeof(TClassRec*));
   memset(fgAlternate, 0, fgSize*sizeof(TClassAlt*));
   gClassTable = this;
}

//______________________________________________________________________________
TClassTable::~TClassTable()
{
   // TClassTable singleton is deleted in Terminate().

   // Try to avoid spurrious warning from memory leak checkers.
   if (gClassTable != this) return;

   for (UInt_t i = 0; i < fgSize; i++) {
      delete fgTable[i]; // Will delete all the elements in the chain.
   }
   delete [] fgTable; fgTable = 0;
   delete [] fgSortedTable; fgSortedTable = 0;
   delete fgIdMap; fgIdMap = 0;
}

//______________________________________________________________________________
void TClassTable::Print(Option_t *option) const
{
   // Print the class table. Before printing the table is sorted
   // alphabetically. Only classes specified in option are listed.
   // The default is to list all classes.
   // Standard wilcarding notation supported.

   if (fgTally == 0 || !fgTable)
      return;

   SortTable();

   int n = 0, ninit = 0, nl = 0;

   int nch = strlen(option);
   TRegexp re(option, kTRUE);

   Printf("\nDefined classes");
   Printf("class                                 version  bits  initialized");
   Printf("================================================================");
   for (UInt_t i = 0; i < fgTally; i++) {
      TClassRec *r = fgSortedTable[i];
      if (!r) break;
      n++;
      TString s = r->fName;
      if (nch && strcmp(option,r->fName) && s.Index(re) == kNPOS) continue;
      nl++;
      if (TClass::GetClass(r->fName, kFALSE)) {
         ninit++;
         Printf("%-35s %6d %7d       Yes", r->fName, r->fId, r->fBits);
      } else
         Printf("%-35s %6d %7d       No",  r->fName, r->fId, r->fBits);
   }
   Printf("----------------------------------------------------------------");
   Printf("Listed Classes: %4d  Total classes: %4d   initialized: %4d",nl, n, ninit);
   Printf("================================================================\n");
}

//---- static members --------------------------------------------------------

//______________________________________________________________________________
char *TClassTable::At(UInt_t index)
{
    // Returns class at index from sorted class table. Don't use this iterator
    // while modifying the class table. The class table can be modified
    // when making calls like TClass::GetClass(), etc.
    // Returns 0 if index points beyond last class name.

   SortTable();
   if (index < fgTally) {
      TClassRec *r = fgSortedTable[index];
      if (r) return r->fName;
   }
   return 0;
}

//______________________________________________________________________________
int   TClassTable::Classes() { return fgTally; }
//______________________________________________________________________________
void  TClassTable::Init() { fgCursor = 0; SortTable(); }

namespace ROOT { class TForNamespace {}; } // Dummy class to give a typeid to namespace (see also TGenericClassInfo)

//______________________________________________________________________________
void TClassTable::Add(const char *cname, Version_t id,  const type_info &info,
                      DictFuncPtr_t dict, Int_t pragmabits)
{
   // Add a class to the class table (this is a static function).
   // Note that the given cname *must* be already normalized.

   if (!gClassTable)
      new TClassTable;

   // check if already in table, if so return
   TClassRec *r = FindElementImpl(cname, kTRUE);
   if (r->fName && r->fInfo) {
      if ( strcmp(r->fInfo->name(),typeid(ROOT::TForNamespace).name())==0
           && strcmp(info.name(),typeid(ROOT::TForNamespace).name())==0 ) {
         // We have a namespace being reloaded.
         // This okay we just keep the old one.
         return;
      }
//       if (splitname.IsSTLCont()==0) {
      if (!TClassEdit::IsStdClass(cname)) {
         // Warn only for class that are not STD classes
         ::Warning("TClassTable::Add", "class %s already in TClassTable", cname);
      }
      return;
   } else if (ROOT::gROOTLocal && gCling) {
      TClass *oldcl = (TClass*)gROOT->GetListOfClasses()->FindObject(cname);
      if (oldcl) { //  && oldcl->GetClassInfo()) {
         // As a work-around to ROOT-6012, we need to register the class even if
         // it is not a template instance, because a forward declaration in the header
         // files loaded by the current dictionary wil also de-activate the update
         // class info mechanism!

         // The TClass exist and already has a class info, so it must
         // correspond to a class template instantiation which the interpreter
         // was able to make with the library containing the TClass Init.
         // Because it is already known to the interpreter, the update class info
         // will not be triggered, we need to force it.
         gCling->RegisterTClassUpdate(oldcl,dict);
      }
   }

   if (!r->fName) r->fName = StrDup(cname);
   r->fId   = id;
   r->fBits = pragmabits;
   r->fDict = dict;
   r->fInfo = &info;

   fgIdMap->Add(info.name(),r);

   fgSorted = kFALSE;
}

//______________________________________________________________________________
void TClassTable::Add(TProtoClass *proto)
{
   // Add a class to the class table (this is a static function).

   if (!gClassTable)
      new TClassTable;

   // By definition the name in the TProtoClass is (must be) the normalized
   // name, so there is no need to tweak it.
   const char *cname = proto->GetName();

   // check if already in table, if so return
   TClassRec *r = FindElementImpl(cname, kTRUE);
   if (r->fName) {
      r->fProto = proto;
      return;
   } else if (ROOT::gROOTLocal && gCling) {
      TClass *oldcl = (TClass*)gROOT->GetListOfClasses()->FindObject(cname);
      if (oldcl) { //  && oldcl->GetClassInfo()) {
                   // As a work-around to ROOT-6012, we need to register the class even if
                   // it is not a template instance, because a forward declaration in the header
                   // files loaded by the current dictionary wil also de-activate the update
                   // class info mechanism!

         ::Warning("TClassTable::Add(TProtoClass*)","Called for existing class without a prior call add the dictionary function.");
      }
   }

   r->fName = StrDup(cname);
   r->fId   = 0;
   r->fBits = 0;
   r->fDict = 0;
   r->fInfo = 0;
   r->fProto= proto;

   fgSorted = kFALSE;
}

//______________________________________________________________________________
void TClassTable::AddAlternate(const char *normName, const char *alternate)
{
   if (!gClassTable)
      new TClassTable;

   UInt_t slot = ROOT::ClassTableHash(alternate, fgSize);

   for (const TClassAlt *a = fgAlternate[slot]; a; a = a->fNext.get()) {
      if (strcmp(alternate,a->fName)==0) {
         if (strcmp(normName,a->fNormName) != 0) {
            fprintf(stderr,"Error in TClassTable::AddAlternate: "
                    "Second registration of %s with a different normalized name (old: '%s', new: '%s')\n",
                    alternate, a->fNormName, normName);
            return;
         }
      }
   }

   fgAlternate[slot] = new TClassAlt(alternate,normName,fgAlternate[slot]);
}

//______________________________________________________________________________
Bool_t TClassTable::Check(const char *cname, std::string &normname)
{
   if (!gClassTable || !fgTable) return kFALSE;

   UInt_t slot = ROOT::ClassTableHash(cname, fgSize);

   // Check if 'cname' is a known normalized name.
   for (TClassRec *r = fgTable[slot]; r; r = r->fNext)
      if (strcmp(cname,r->fName)==0) return kTRUE;

   // See if 'cname' is register in the list of alternate names
   for (const TClassAlt *a = fgAlternate[slot]; a; a = a->fNext.get()) {
      if (strcmp(cname,a->fName)==0) {
         normname = a->fNormName;
         return kTRUE;
      }
   }

   return kFALSE;
}

//______________________________________________________________________________
void TClassTable::Remove(const char *cname)
{
   // Remove a class from the class table. This happens when a shared library
   // is unloaded (i.e. the dtor's of the global init objects are called).

   if (!gClassTable || !fgTable) return;

   UInt_t slot = ROOT::ClassTableHash(cname,fgSize);

   TClassRec *r;
   TClassRec *prev = 0;
   for (r = fgTable[slot]; r; r = r->fNext) {
      if (!strcmp(r->fName, cname)) {
         if (prev)
            prev->fNext = r->fNext;
         else
            fgTable[slot] = r->fNext;
         fgIdMap->Remove(r->fInfo->name());
         r->fNext = 0; // Do not delete the others.
         delete r;
         fgTally--;
         fgSorted = kFALSE;
         break;
      }
      prev = r;
   }
}

//______________________________________________________________________________
TClassRec *TClassTable::FindElementImpl(const char *cname, Bool_t insert)
{
   // Find a class by name in the class table (using hash of name). Returns
   // 0 if the class is not in the table. Unless arguments insert is true in
   // which case a new entry is created and returned.

   UInt_t slot = ROOT::ClassTableHash(cname,fgSize);

   for (TClassRec *r = fgTable[slot]; r; r = r->fNext)
      if (strcmp(cname,r->fName)==0) return r;

   if (!insert) return 0;

   fgTable[slot] = new TClassRec(fgTable[slot]);

   fgTally++;
   return fgTable[slot];
}

//______________________________________________________________________________
TClassRec *TClassTable::FindElement(const char *cname, Bool_t insert)
{
   // Find a class by name in the class table (using hash of name). Returns
   // 0 if the class is not in the table. Unless arguments insert is true in
   // which case a new entry is created and returned.
   // cname can be any spelling of the class name.  See FindElementImpl if the
   // name is already normalized.

   if (!fgTable) return 0;

   // The recorded name is normalized, let's make sure we convert the
   // input accordingly.
   std::string normalized;
   TClassEdit::GetNormalizedName(normalized,cname);

   return FindElementImpl(normalized.c_str(), insert);
}

//______________________________________________________________________________
Version_t TClassTable::GetID(const char *cname)
{
   // Returns the ID of a class.

   TClassRec *r = FindElement(cname);
   if (r) return r->fId;
   return -1;
}

//______________________________________________________________________________
Int_t TClassTable::GetPragmaBits(const char *cname)
{
   // Returns the pragma bits as specified in the LinkDef.h file.

   TClassRec *r = FindElement(cname);
   if (r) return r->fBits;
   return 0;
}

//______________________________________________________________________________
DictFuncPtr_t TClassTable::GetDict(const char *cname)
{
   // Given the class name returns the Dictionary() function of a class
   // (uses hash of name).

   if (gDebug > 9) {
      ::Info("GetDict", "searches for %s", cname);
      fgIdMap->Print();
   }

   TClassRec *r = FindElement(cname);
   if (r) return r->fDict;
   return 0;
}

//______________________________________________________________________________
DictFuncPtr_t TClassTable::GetDict(const type_info& info)
{
   // Given the type_info returns the Dictionary() function of a class
   // (uses hash of type_info::name()).

   if (gDebug > 9) {
      ::Info("GetDict", "searches for %s at 0x%lx", info.name(), (Long_t)&info);
      fgIdMap->Print();
   }

   TClassRec *r = fgIdMap->Find(info.name());
   if (r) return r->fDict;
   return 0;
}

//______________________________________________________________________________
DictFuncPtr_t TClassTable::GetDictNorm(const char *cname)
{
   // Given the normalized class name returns the Dictionary() function of a class
   // (uses hash of name).

   if (gDebug > 9) {
      ::Info("GetDict", "searches for %s", cname);
      fgIdMap->Print();
   }

   TClassRec *r = FindElementImpl(cname,kFALSE);
   if (r) return r->fDict;
   return 0;
}

//______________________________________________________________________________
TProtoClass *TClassTable::GetProto(const char *cname)
{
   // Given the class name returns the TClassProto object for the class.
   // (uses hash of name).

   if (gDebug > 9) {
      ::Info("GetDict", "searches for %s", cname);
      fgIdMap->Print();
   }

   TClassRec *r = FindElement(cname);
   if (r) return r->fProto;
   return 0;
}

//______________________________________________________________________________
TProtoClass *TClassTable::GetProtoNorm(const char *cname)
{
   // Given the class normalized name returns the TClassProto object for the class.
   // (uses hash of name).

   if (gDebug > 9) {
      ::Info("GetDict", "searches for %s", cname);
      fgIdMap->Print();
   }

   TClassRec *r = FindElementImpl(cname,kFALSE);
   if (r) return r->fProto;
   return 0;
}

//______________________________________________________________________________
extern "C" {
   static int ClassComp(const void *a, const void *b)
   {
      // Function used for sorting classes alphabetically.

      return strcmp((*(TClassRec **)a)->fName, (*(TClassRec **)b)->fName);
   }
}

//______________________________________________________________________________
char *TClassTable::Next()
{
    // Returns next class from sorted class table. Don't use this iterator
    // while modifying the class table. The class table can be modified
    // when making calls like TClass::GetClass(), etc.

   if (fgCursor < fgTally) {
      TClassRec *r = fgSortedTable[fgCursor++];
      return r->fName;
   } else
      return 0;
}

//______________________________________________________________________________
void TClassTable::PrintTable()
{
   // Print the class table. Before printing the table is sorted
   // alphabetically.

   if (fgTally == 0 || !fgTable)
      return;

   SortTable();

   int n = 0, ninit = 0;

   Printf("\nDefined classes");
   Printf("class                                 version  bits  initialized");
   Printf("================================================================");
   UInt_t last = fgTally;
   for (UInt_t i = 0; i < last; i++) {
      TClassRec *r = fgSortedTable[i];
      if (!r) break;
      n++;
      // Do not use TClass::GetClass to avoid any risk of autoloading.
      if (gROOT->GetListOfClasses()->FindObject(r->fName)) {
         ninit++;
         Printf("%-35s %6d %7d       Yes", r->fName, r->fId, r->fBits);
      } else
         Printf("%-35s %6d %7d       No",  r->fName, r->fId, r->fBits);
   }
   Printf("----------------------------------------------------------------");
   Printf("Total classes: %4d   initialized: %4d", n, ninit);
   Printf("================================================================\n");
}

//______________________________________________________________________________
void TClassTable::SortTable()
{
   // Sort the class table by ascending class ID's.

   if (!fgSorted) {
      delete [] fgSortedTable;
      fgSortedTable = new TClassRec* [fgTally];

      int j = 0;
      for (UInt_t i = 0; i < fgSize; i++)
         for (TClassRec *r = fgTable[i]; r; r = r->fNext)
            fgSortedTable[j++] = r;

      ::qsort(fgSortedTable, fgTally, sizeof(TClassRec *), ::ClassComp);
      fgSorted = kTRUE;
   }
}

//______________________________________________________________________________
void TClassTable::Terminate()
{
   // Deletes the class table (this static class function calls the dtor).

   if (gClassTable) {
      for (UInt_t i = 0; i < fgSize; i++)
         delete fgTable[i]; // Will delete all the elements in the chain.

      delete [] fgTable; fgTable = 0;
      delete [] fgSortedTable; fgSortedTable = 0;
      delete fgIdMap; fgIdMap = 0;
      fgSize = 0;
      SafeDelete(gClassTable);
   }
}

//______________________________________________________________________________
void ROOT::AddClass(const char *cname, Version_t id,
                    const type_info& info,
                    DictFuncPtr_t dict,
                    Int_t pragmabits)
{
   // Global function called by the ctor of a class's init class
   // (see the ClassImp macro).

   TClassTable::Add(cname, id, info, dict, pragmabits);
}

//______________________________________________________________________________
void ROOT::AddClassAlternate(const char *normName, const char *alternate)
{
   // Global function called by GenerateInitInstance.
   // (see the ClassImp macro).

   TClassTable::AddAlternate(normName,alternate);
}

//______________________________________________________________________________
void ROOT::ResetClassVersion(TClass *cl, const char *cname, Short_t newid)
{
   // Global function to update the version number.
   // This is called via the RootClassVersion macro.
   //
   // if cl!=0 and cname==-1, set the new class version if and only is
   // greater than the existing one and greater or equal to 2;
   // and also ignore the request if fVersionUsed is true.
   //
   // Note on class version number:
   //   If no class has been specified, TClass::GetVersion will return -1
   //   The Class Version 0 request the whole object to be transient
   //   The Class Version 1, unless specify via ClassDef indicates that the
   //      I/O should use the TClass checksum to distinguish the layout of the class

   if (cname && cname!=(void*)-1) {
      TClassRec *r = TClassTable::FindElement(cname,kFALSE);
      if (r) r->fId = newid;
   }
   if (cl) {
      if (cl->fVersionUsed) {
         // Problem, the reset is called after the first usage!
         if (cname!=(void*)-1)
            Error("ResetClassVersion","Version number of %s can not be changed after first usage!",
                  cl->GetName());
      } else {
         if (newid < 0) {
            Error("SetClassVersion","The class version (for %s) must be positive (value %d is ignored)",cl->GetName(),newid);
         }
         if (cname==(void*)-1) {
            if (cl->fClassVersion<newid && 2<=newid) {
               cl->SetClassVersion(newid);
            }
         } else {
            cl->SetClassVersion(newid);
         }
      }
   }
}


//______________________________________________________________________________
void ROOT::RemoveClass(const char *cname)
{
   // Global function called by the dtor of a class's init class
   // (see the ClassImp macro).

   // don't delete class information since it is needed by the I/O system
   // to write the StreamerInfo to file
   if (cname) {
      // Let's still remove this information to allow reloading later.
      // Anyway since the shared library has been unloaded, the dictionary
      // pointer is now invalid ....
      // We still keep the TClass object around because TFile needs to
      // get to the TStreamerInfo.
      if (gROOT && gROOT->GetListOfClasses()) {
         TObject *pcname;
         if ((pcname=gROOT->GetListOfClasses()->FindObject(cname))) {
            TClass *cl = dynamic_cast<TClass*>(pcname);
            if (cl) cl->SetUnloaded();
         }
      }
      TClassTable::Remove(cname);
   }
}

//______________________________________________________________________________
TNamed *ROOT::RegisterClassTemplate(const char *name, const char *file,
                                    Int_t line)
{
   // Global function to register the implementation file and line of
   // a class template (i.e. NOT a concrete class).

   static TList table;
   static Bool_t isInit = kFALSE;
   if (!isInit) {
      table.SetOwner(kTRUE);
      isInit = kTRUE;
   }

   TString classname(name);
   Ssiz_t loc = classname.Index("<");
   if (loc >= 1) classname.Remove(loc);
   if (file) {
      TNamed *obj = new TNamed((const char*)classname, file);
      obj->SetUniqueID(line);
      table.Add(obj);
      return obj;
   } else {
      return (TNamed*)table.FindObject(classname);
   }
}
