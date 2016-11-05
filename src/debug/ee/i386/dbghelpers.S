// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

.intel_syntax noprefix
#include "unixasmmacros.inc"

//extern FuncEvalHijackWorker:proc

// @dbgtodo- once we port Funceval, use the ExceptionHijack stub instead of this func-eval stub.
NESTED_ENTRY FuncEvalHijack, _TEXT, UnhandledExceptionHandlerUnix
        push eax       // the ptr to the DebuggerEval
        call FuncEvalHijackWorker
        jmp  eax       // return is the patch addresss to jmp to
NESTED_END FuncEvalHijack, _TEXT

//extern ExceptionHijackWorker:proc

//
// Flares for interop debugging.
// Flares are exceptions (breakpoints) at well known addresses which the RS
// listens for when interop debugging.
//

// This exception is from managed code.
LEAF_ENTRY SignalHijackStartedFlare, _TEXT
        int 3
        // make sure that the basic block is unique
        test rax,1
        ret
LEAF_END SignalHijackStartedFlare, _TEXT

// Start the handoff
LEAF_ENTRY ExceptionForRuntimeHandoffStartFlare, _TEXT
        int 3
        // make sure that the basic block is unique
        test rax,2
        ret
LEAF_END ExceptionForRuntimeHandoffStartFlare, _TEXT

// Finish the handoff.
LEAF_ENTRY ExceptionForRuntimeHandoffCompleteFlare, _TEXT
        int 3
        // make sure that the basic block is unique
        test rax,3
        ret
LEAF_END ExceptionForRuntimeHandoffCompleteFlare, _TEXT

// Signal execution return to unhijacked state
LEAF_ENTRY SignalHijackCompleteFlare, _TEXT
        int 3
        // make sure that the basic block is unique
        test rax,4
        ret
LEAF_END SignalHijackCompleteFlare, _TEXT

// This exception is from unmanaged code.
LEAF_ENTRY ExceptionNotForRuntimeFlare, _TEXT
        int 3
        // make sure that the basic block is unique
        test rax,5
        ret
LEAF_END ExceptionNotForRuntimeFlare, _TEXT

// The Runtime is synchronized.
LEAF_ENTRY NotifyRightSideOfSyncCompleteFlare, _TEXT
        int 3
        // make sure that the basic block is unique
        test rax,6
        ret
LEAF_END NotifyRightSideOfSyncCompleteFlare, _TEXT
