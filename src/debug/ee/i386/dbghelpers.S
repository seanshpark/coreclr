// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

#if defined(_AMD64_)
.intel_syntax noprefix
#endif
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
#if defined(_AMD64_)
        int 3
        // make sure that the basic block is unique
        test rax,1
#else
        int3
        test %eax, 1
#endif
        ret
LEAF_END SignalHijackStartedFlare, _TEXT

// Start the handoff
LEAF_ENTRY ExceptionForRuntimeHandoffStartFlare, _TEXT
#if defined(_AMD64_)
        int 3
        // make sure that the basic block is unique
        test rax,2
#else
        int3
        test %eax, 2
#endif
        ret
LEAF_END ExceptionForRuntimeHandoffStartFlare, _TEXT

// Finish the handoff.
LEAF_ENTRY ExceptionForRuntimeHandoffCompleteFlare, _TEXT
#if defined(_AMD64_)
        int 3
        // make sure that the basic block is unique
        test rax,3
#else
        int3
        test %eax, 3
#endif
        ret
LEAF_END ExceptionForRuntimeHandoffCompleteFlare, _TEXT

// Signal execution return to unhijacked state
LEAF_ENTRY SignalHijackCompleteFlare, _TEXT
#if defined(_AMD64_)
        int 3
        // make sure that the basic block is unique
        test rax, 4
#else
        int3
        test %eax,4
#endif
        ret
LEAF_END SignalHijackCompleteFlare, _TEXT

// This exception is from unmanaged code.
LEAF_ENTRY ExceptionNotForRuntimeFlare, _TEXT
#if defined(_AMD64_)
        int 3
        // make sure that the basic block is unique
        test rax,5
#else
        int3
        test %eax, 5
#endif
        ret
LEAF_END ExceptionNotForRuntimeFlare, _TEXT

// The Runtime is synchronized.
LEAF_ENTRY NotifyRightSideOfSyncCompleteFlare, _TEXT
#if defined(_AMD64_)
        int 3
        // make sure that the basic block is unique
        test rax,6
#else
        int3
        test %eax, 6
#endif
        ret
LEAF_END NotifyRightSideOfSyncCompleteFlare, _TEXT
