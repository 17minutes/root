// @(#)root/base:$Id$
// Author: Rene Brun   11/11/99

/*************************************************************************
 * Copyright (C) 1995-2000, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

/** \class TMessageHandler

Handle messages that might be generated by the system.
By default a handler only keeps track of the different messages
generated for a specific class. By deriving from this class and
overriding Notify() one can implement custom message handling.
In Notify() one has access to the message id and the object
generating the message. One can install more than one message
handler per class. A message handler can be removed or again
added when needed.

  - All Root "Warnings"  are logged as message 1001
  - All Root "Errors"    are logged as message 1002
  - All Root "SysErrors" are logged as message 1003
  - All Root "Fatals"    are logged as message 1004
*/

#include "TMessageHandler.h"
#include "TClass.h"
#include "TROOT.h"
#include "TVirtualMutex.h"

ClassImp(TMessageHandler)

////////////////////////////////////////////////////////////////////////////////
/// Create a new message handler for class cl and add it to the list
/// of message handlers.

TMessageHandler::TMessageHandler(const TClass *cl, Bool_t derived)
{
   fClass   = cl;
   fMessObj = 0;
   fMessId  = 0;
   fSize    = 0;
   fCnts    = 0;
   fMessIds = 0;
   fDerived = derived;

   if (fClass)
      SetName(fClass->GetName());
   else
      SetName("DefaultMessageHandler");

   Add();
}

////////////////////////////////////////////////////////////////////////////////
/// Create a new message handler for class named cl and add it to the list
/// of message handlers.

TMessageHandler::TMessageHandler(const char *cl, Bool_t derived)
{
   fClass   = TClass::GetClass(cl);
   fMessObj = 0;
   fMessId  = 0;
   fSize    = 0;
   fCnts    = 0;
   fMessIds = 0;
   fDerived = derived;

   SetName(cl);

   SetName(fClass->GetName());
   Add();
}

////////////////////////////////////////////////////////////////////////////////
/// Clean up the message handler.

TMessageHandler:: ~TMessageHandler()
{
   Remove();
   if (fSize <= 0) return;
   delete [] fCnts;
   delete [] fMessIds;
}

////////////////////////////////////////////////////////////////////////////////
/// Add this message handler to the list of messages handlers.

void TMessageHandler::Add()
{
   R__LOCKGUARD2(gROOTMutex);
   gROOT->GetListOfMessageHandlers()->Add(this);
   if (fClass) {
      // don't emit signal when the default message handler is added
      // as this happens in the TROOT ctor and the TQObject stuff is
      // not yet properly initialized on some platforms
      Added();  // emit Added() signal
   }
}

////////////////////////////////////////////////////////////////////////////////
/// Return counter for message with ID=messid.

Int_t TMessageHandler::GetMessageCount(Int_t messId) const
{
   if (fSize <= 0) return 0;
   for (Int_t i = 0; i < fSize; i++) {
      if (fMessIds[i] == messId) return fCnts[i];
   }
   return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// Return total number of messages.

Int_t TMessageHandler::GetTotalMessageCount() const
{
   if (fSize <= 0) return 0;
   Int_t count = 0;
   for (Int_t i = 0; i < fSize; i++) {
      count += fCnts[i];
   }
   return count;
}

////////////////////////////////////////////////////////////////////////////////
/// Store message origin, keep statistics and call Notify().

void TMessageHandler::HandleMessage(Int_t id, const TObject *obj)
{
   // check if message must be managed by this message handler
   if (fClass) {
      if (fDerived) {
         if(!obj->InheritsFrom(fClass)) return;
      } else {
         if (obj->IsA() != fClass) return;
      }
   }

   fMessId  = id;
   fMessObj = obj;

   Notify();

   // increment statistics
   Int_t i;
   // first message
   if (fSize <= 0) {
      fSize    = 1;
      fCnts    = new Int_t[fSize];
      fMessIds = new Int_t[fSize];
   } else {
      // already existing message
      for (i = 0; i < fSize; i++) {
         if (fMessIds[i] == fMessId) {
            fCnts[i]++;
            return;
         }
      }
      // new message
      fSize++;
      Int_t *newCnts    = new Int_t[fSize];
      Int_t *newMessIds = new Int_t[fSize];
      for (i = 0; i < fSize-1; i++) {
         newCnts[i]    = fCnts[i];
         newMessIds[i] = fMessIds[i];
      }
      delete [] fCnts;
      delete [] fMessIds;
      fCnts    = newCnts;
      fMessIds = newMessIds;
   }
   fCnts[fSize-1]    = 1;
   fMessIds[fSize-1] = fMessId;
}

////////////////////////////////////////////////////////////////////////////////
/// This method must be overridden to handle object notification.

Bool_t TMessageHandler::Notify()
{
   if (fClass) return kFALSE;
   // case of default handler
   // encode class number in message id
   if (!fMessObj) return kFALSE;
   Int_t uid = Int_t(fMessObj->IsA()->GetUniqueID());
   fMessId += 10000*uid;
   fMessId = -fMessId;
   Notified();  // emit Notified() signal
   return kFALSE;
}

////////////////////////////////////////////////////////////////////////////////
/// Print statistics for this message handler.

void TMessageHandler::Print(Option_t *) const
{
   printf("\n ****** Message Handler: %s has a total of %d messages\n",GetName(),GetTotalMessageCount());
   if (fSize <= 0) return;
   Int_t id, uid;
   const TClass *cl;
   TIter next(gROOT->GetListOfClasses());
   for (Int_t i = 0; i < fSize; i++) {
      id = fMessIds[i];
      cl = fClass;
      if (id < 0) {
         id = -id;
         uid = id/10000;
         id = id%10000;
         next.Reset();
         while ((cl = (TClass*)next())) {
            if (cl->GetUniqueID() == UInt_t(uid)) break;
         }
      }
      if (!cl) cl = gROOT->IsA();
      if (id == 1001) {
         printf("  Class: %-20s WARNINGs       has %d counts\n",cl->GetName(),fCnts[i]);
         continue;
      }
      if (id == 1002) {
         printf("  Class: %-20s ERRORs         has %d counts\n",cl->GetName(),fCnts[i]);
         continue;
      }
      printf("  Class: %-20s MessID = %5d has %d counts\n",cl->GetName(),id,fCnts[i]);
   }
}

////////////////////////////////////////////////////////////////////////////////
/// Remove this message handler from the list of messages handlers.

void TMessageHandler::Remove()
{
   R__LOCKGUARD2(gROOTMutex);
   gROOT->GetListOfMessageHandlers()->Remove(this);
   Removed();  // emit Removed() signal
}
