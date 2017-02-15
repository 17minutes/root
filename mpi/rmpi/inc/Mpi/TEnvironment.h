// @(#)root/mpi / Author: Omar.Zapata@cern.ch 2016 http://oproject.org
#ifndef ROOT_Mpi_TEnvironment
#define ROOT_Mpi_TEnvironment

#include<Mpi/Globals.h>

namespace ROOT {

   namespace Mpi {
      /**
      \class TEnvironment
         Class manipulate mpi environment, with this class you can to start/stop the communication system and
         to hanlde some information about the communication environment.
         \ingroup Mpi
       */

      class TEnvironment: public TObject {
      public:
         /**
         Default constructor to start the environment
              */
         TEnvironment();
         /**
         Constructor thar reciev command line arguments
              */
         TEnvironment(Int_t &argc, Char_t ** &argv);
         ~TEnvironment();
         /**
         Method to init the environment.
              */
         static void Init();

         /**
         Method to finalize the environment.
              */
         static void Finalize();

         // static public functions TODO
         /**
         Method to check if the communication system is finalized.
              */
         static Bool_t IsFinalized();
         /**
         Method to check if the communication system is initialized.
              */
         static Bool_t IsInitialized();

         static TString GetProcessorName();
//          static Int_t GetThreadLevel();
//          static Bool_t IsMainThread();
         ClassDef(TEnvironment, 1)
      };
   }
}

#endif
