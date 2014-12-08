/*
    * WinDBG Anti-RootKit extension
    * Copyright � 2013-2014  Vyacheslav Rusakoff
    * 
    * This program is free software: you can redistribute it and/or modify
    * it under the terms of the GNU General Public License as published by
    * the Free Software Foundation, either version 3 of the License, or
    * (at your option) any later version.
    * 
    * This program is distributed in the hope that it will be useful,
    * but WITHOUT ANY WARRANTY; without even the implied warranty of
    * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    * GNU General Public License for more details.
    * 
    * You should have received a copy of the GNU General Public License
    * along with this program.  If not, see <http://www.gnu.org/licenses/>.

    * This work is licensed under the terms of the GNU GPL, version 3.  See
    * the COPYING file in the top-level directory.
*/

/*

Look for PsEstablishWin32Callouts.

Prior Windows 8 callout's names are:

XP SP3:
---
nt!PspW32ProcessCallout
nt!PspW32ThreadCallout
nt!ExGlobalAtomTableCallout
nt!KeGdiFlushUserBatch
nt!PopEventCallout
nt!PopStateCallout
nt!PspW32JobCallout
nt!ExDesktopOpenProcedureCallout
nt!ExDesktopOkToCloseProcedureCallout
nt!ExDesktopCloseProcedureCallout
nt!ExDesktopDeleteProcedureCallout
nt!ExWindowStationOkToCloseProcedureCallout
nt!ExWindowStationCloseProcedureCallout
nt!ExWindowStationDeleteProcedureCallout
nt!ExWindowStationParseProcedureCallout
nt!ExWindowStationOpenProcedureCallout

VISTA:
---
nt!IopWin32DataCollectionProcedureCallout

WINDOWS 7:
---
nt!PopWin32InfoCallout

WINDOWS 8:
---
void __stdcall PsEstablishWin32Callouts(int a1)
{
  int v1; // eax@1
  void *v2; // esi@1

  v1 = (int)ExAllocateCallBack(a1, 0);
  v2 = (void *)v1;
  if ( v1 )
  {
    if ( ExCompareExchangeCallBack(v1, (signed __int32 *)&PsWin32CallBack, 0) )
      PsWin32CalloutsEstablished = 1;
    else
      ExFreeCallBack(v2);
  }
}

*/

#include <string>
#include <vector>
#include <memory>

#include "wdbgark.hpp"
#include "analyze.hpp"

EXT_COMMAND(wa_callouts, "Output kernel-mode win32k callouts", "") {
    RequireKernelMode();

    if ( !Init() )
        throw ExtStatusException(S_OK, "global init failed");

    out << "Displaying Win32k callouts" << endlout;

    std::unique_ptr<WDbgArkAnalyze> display(new WDbgArkAnalyze(WDbgArkAnalyze::AnalyzeTypeDefault));

    if ( !display->SetOwnerModule("win32k") )
        warn << __FUNCTION__ ": SetOwnerModule failed" << endlwarn;

    display->PrintHeader();

    try {
        if ( m_minor_build < W8RTM_VER ) {
            for ( const std::string &callout_name : callout_names ) {
                unsigned __int64 offset = 0;

                if ( GetSymbolOffset(callout_name.c_str(), true, &offset) ) {
                    ExtRemoteData callout_routine(offset, m_PtrSize);
                    display->AnalyzeAddressAsRoutine(callout_routine.GetPtr(), callout_name, "");
                }
            }
        } else {
            unsigned __int64 offset = 0;

            if ( GetSymbolOffset("nt!PsWin32CallBack", true, &offset) ) {
                ExtRemoteData callout_block(offset, m_PtrSize);

                unsigned __int64 ex_callback_fast_ref = callout_block.GetPtr();

                if ( ex_callback_fast_ref ) {
                    ExtRemoteData routine_block(
                        m_obj_helper->ExFastRefGetObject(ex_callback_fast_ref) + GetTypeSize("nt!_EX_RUNDOWN_REF"),
                        m_PtrSize);

                    display->AnalyzeAddressAsRoutine(routine_block.GetPtr(), "nt!PsWin32CallBack", "");
                }
            }
        }
    }
    catch ( const ExtRemoteException &Ex ) {
        err << __FUNCTION__ << ": " << Ex.GetMessage() << endlerr;
    }
    catch( const ExtInterruptException& ) {
        throw;
    }

    display->PrintFooter();
    display->PrintFooter();
}
