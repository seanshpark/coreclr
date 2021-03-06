// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.
// ===========================================================================
// File: perfmap.cpp
//

#include "common.h"

#if defined(FEATURE_PERFMAP) && !defined(DACCESS_COMPILE)
#include "perfmap.h"
#include "perfinfo.h"
#include "pal.h"

// The code addresses are actually native image offsets during crossgen. Print
// them as 32-bit numbers for consistent output when cross-targeting and to
// make the output more compact.

#ifdef CROSSGEN_COMPILE
#define FMT_CODE_ADDR "%08x"
#else
#define FMT_CODE_ADDR "%p"
#endif

PerfMap * PerfMap::s_Current = nullptr;
bool PerfMap::s_ShowOptimizationTiers = false;

// Initialize the map for the process - called from EEStartupHelper.
void PerfMap::Initialize()
{
    LIMITED_METHOD_CONTRACT;

    // Only enable the map if requested.
    if (CLRConfig::GetConfigValue(CLRConfig::EXTERNAL_PerfMapEnabled))
    {
        // Get the current process id.
        int currentPid = GetCurrentProcessId();

        // Create the map.
        s_Current = new PerfMap(currentPid);

        int signalNum = (int) CLRConfig::GetConfigValue(CLRConfig::EXTERNAL_PerfMapIgnoreSignal);

        if (signalNum > 0)
        {
            PAL_IgnoreProfileSignal(signalNum);
        }

        if (CLRConfig::GetConfigValue(CLRConfig::EXTERNAL_PerfMapShowOptimizationTiers) != 0)
        {
            s_ShowOptimizationTiers = true;
        }
    }
}

// Destroy the map for the process - called from EEShutdownHelper.
void PerfMap::Destroy()
{
    LIMITED_METHOD_CONTRACT;

    if (s_Current != nullptr)
    {
        delete s_Current;
        s_Current = nullptr;
    }
}

// Construct a new map for the process.
PerfMap::PerfMap(int pid)
{
    LIMITED_METHOD_CONTRACT;

    // Initialize with no failures.
    m_ErrorEncountered = false;

    m_StubsMapped = 0;

    // Build the path to the map file on disk.
    WCHAR tempPath[MAX_LONGPATH+1];
    if(!GetTempPathW(MAX_LONGPATH, tempPath))
    {
        return;
    }
    
    SString path;
    path.Printf("%Sperf-%d.map", &tempPath, pid);

    // Open the map file for writing.
    OpenFile(path);

    m_PerfInfo = new PerfInfo(pid);
}

// Construct a new map without a specified file name.
// Used for offline creation of NGEN map files.
PerfMap::PerfMap()
  : m_FileStream(nullptr)
  , m_PerfInfo(nullptr)
{
    LIMITED_METHOD_CONTRACT;

    // Initialize with no failures.
    m_ErrorEncountered = false;

    m_StubsMapped = 0;
}

// Clean-up resources.
PerfMap::~PerfMap()
{
    LIMITED_METHOD_CONTRACT;

    delete m_FileStream;
    m_FileStream = nullptr;

    delete m_PerfInfo;
    m_PerfInfo = nullptr;
}

// Open the specified destination map file.
void PerfMap::OpenFile(SString& path)
{
    STANDARD_VM_CONTRACT;

    // Open the file stream.
    m_FileStream = new (nothrow) CFileStream();
    if(m_FileStream != nullptr)
    {
        HRESULT hr = m_FileStream->OpenForWrite(path.GetUnicode());
        if(FAILED(hr))
        {
            delete m_FileStream;
            m_FileStream = nullptr;
        }
    }
}

// Write a line to the map file.
void PerfMap::WriteLine(SString& line)
{
    STANDARD_VM_CONTRACT;

    EX_TRY
    {
        // Write the line.
        // The PAL already takes a lock when writing, so we don't need to do so here.
        StackScratchBuffer scratch;
        const char * strLine = line.GetANSI(scratch);
        ULONG inCount = line.GetCount();
        ULONG outCount;
        m_FileStream->Write(strLine, inCount, &outCount);

        if (inCount != outCount)
        {
            // This will cause us to stop writing to the file.
            // The file will still remain open until shutdown so that we don't have to take a lock at this level when we touch the file stream.
            m_ErrorEncountered = true;
        }

    }
    EX_CATCH{} EX_END_CATCH(SwallowAllExceptions);
}

// Log a method to the map.
void PerfMap::LogMethod(MethodDesc * pMethod, PCODE pCode, size_t codeSize, const char *optimizationTier)
{
    CONTRACTL{
        THROWS;
        GC_NOTRIGGER;
        MODE_PREEMPTIVE;
        PRECONDITION(pMethod != nullptr);
        PRECONDITION(pCode != nullptr);
        PRECONDITION(codeSize > 0);
    } CONTRACTL_END;

    if (m_FileStream == nullptr || m_ErrorEncountered)
    {
        // A failure occurred, do not log.
        return;
    }

    // Logging failures should not cause any exceptions to flow upstream.
    EX_TRY
    {
        // Get the full method signature.
        SString fullMethodSignature;
        pMethod->GetFullMethodInfo(fullMethodSignature);

        // Build the map file line.
        StackScratchBuffer scratch;
        SString line;
        line.Printf(FMT_CODE_ADDR " %x %s", pCode, codeSize, fullMethodSignature.GetANSI(scratch));
        if (optimizationTier != nullptr && s_ShowOptimizationTiers)
        {
            line.AppendPrintf("[%s]\n", optimizationTier);
        }
        else
        {
            line.Append(W('\n'));
        }

        // Write the line.
        WriteLine(line);
    }
    EX_CATCH{} EX_END_CATCH(SwallowAllExceptions);
}


void PerfMap::LogImageLoad(PEFile * pFile)
{
    if (s_Current != nullptr)
    {
        s_Current->LogImage(pFile);
    }
}

// Log an image load to the map.
void PerfMap::LogImage(PEFile * pFile)
{
    CONTRACTL{
        THROWS;
        GC_NOTRIGGER;
        MODE_PREEMPTIVE;
        PRECONDITION(pFile != nullptr);
    } CONTRACTL_END;


    if (m_FileStream == nullptr || m_ErrorEncountered)
    {
        // A failure occurred, do not log.
        return;
    }

    EX_TRY
    {
        WCHAR wszSignature[39];
        GetNativeImageSignature(pFile, wszSignature, lengthof(wszSignature));

        m_PerfInfo->LogImage(pFile, wszSignature);
    }
    EX_CATCH{} EX_END_CATCH(SwallowAllExceptions);
}


// Log a method to the map.
void PerfMap::LogJITCompiledMethod(MethodDesc * pMethod, PCODE pCode, size_t codeSize, PrepareCodeConfig *pConfig)
{
    LIMITED_METHOD_CONTRACT;

    if (s_Current == nullptr)
    {
        return;
    }

    const char *optimizationTier = nullptr;
#ifndef CROSSGEN_COMPILE
    if (s_ShowOptimizationTiers)
    {
        optimizationTier = PrepareCodeConfig::GetJitOptimizationTierStr(pConfig, pMethod);
    }
#endif // CROSSGEN_COMPILE

    s_Current->LogMethod(pMethod, pCode, codeSize, optimizationTier);
}

// Log a set of stub to the map.
void PerfMap::LogStubs(const char* stubType, const char* stubOwner, PCODE pCode, size_t codeSize)
{
    LIMITED_METHOD_CONTRACT;

    if (s_Current == nullptr || s_Current->m_FileStream == nullptr)
    {
        return;
    }

    // Logging failures should not cause any exceptions to flow upstream.
    EX_TRY
    {
        if(!stubOwner)
        {
            stubOwner = "?";
        }
        if(!stubType)
        {
            stubOwner = "?";
        }

        // Build the map file line.
        SString line;
        line.Printf(FMT_CODE_ADDR " %x stub<%d> %s<%s>\n", pCode, codeSize, ++(s_Current->m_StubsMapped), stubType, stubOwner);

        // Write the line.
        s_Current->WriteLine(line);
    }
    EX_CATCH{} EX_END_CATCH(SwallowAllExceptions);
}

void PerfMap::GetNativeImageSignature(PEFile * pFile, WCHAR * pwszSig, unsigned int nSigSize)
{
    CONTRACTL{
        PRECONDITION(pFile != nullptr);
        PRECONDITION(pwszSig != nullptr);
        PRECONDITION(nSigSize >= 39);
    } CONTRACTL_END;

    // We use the MVID as the signature, since ready to run images
    // don't have a native image signature.
    GUID mvid;
    pFile->GetMVID(&mvid);
    if(!StringFromGUID2(mvid, pwszSig, nSigSize))
    {
        pwszSig[0] = '\0';
    }
}

// Create a new native image perf map.
NativeImagePerfMap::NativeImagePerfMap(Assembly * pAssembly, BSTR pDestPath)
  : PerfMap()
{
    STANDARD_VM_CONTRACT;

    // Generate perfmap path.

    // Get the assembly simple name.
    LPCUTF8 lpcSimpleName = pAssembly->GetSimpleName();

    // Get the native image signature (GUID).
    // Used to ensure that we match symbols to the correct NGEN image.
    WCHAR wszSignature[39];
    GetNativeImageSignature(pAssembly->GetManifestFile(), wszSignature, lengthof(wszSignature));

    // Build the path to the perfmap file, which consists of <inputpath><imagesimplename>.ni.<signature>.map.
    // Example: /tmp/mscorlib.ni.{GUID}.map
    SString sDestPerfMapPath;
    sDestPerfMapPath.Printf("%S%s.ni.%S.map", pDestPath, lpcSimpleName, wszSignature);

    // Open the perf map file.
    OpenFile(sDestPerfMapPath);
}

// Log data to the perfmap for the specified module.
void NativeImagePerfMap::LogDataForModule(Module * pModule)
{
    STANDARD_VM_CONTRACT;

    PEImageLayout * pLoadedLayout = pModule->GetFile()->GetLoaded();
    _ASSERTE(pLoadedLayout != nullptr);

    SIZE_T baseAddr = (SIZE_T)pLoadedLayout->GetBase();

#ifdef FEATURE_PREJIT
    if (!pLoadedLayout->HasReadyToRunHeader())
    {
        MethodIterator mi((PTR_Module)pModule);
        while (mi.Next())
        {
            MethodDesc *hotDesc = mi.GetMethodDesc();
            hotDesc->CheckRestore();

            LogPreCompiledMethod(hotDesc, mi.GetMethodStartAddress(), baseAddr, nullptr);
        }
        return;
    }
#endif

    ReadyToRunInfo::MethodIterator mi(pModule->GetReadyToRunInfo());
    while (mi.Next())
    {
        MethodDesc* hotDesc = mi.GetMethodDesc();

        LogPreCompiledMethod(hotDesc, mi.GetMethodStartAddress(), baseAddr, "ReadyToRun");
    }
}

// Log a pre-compiled method to the perfmap.
void NativeImagePerfMap::LogPreCompiledMethod(MethodDesc * pMethod, PCODE pCode, SIZE_T baseAddr, const char *optimizationTier)
{
    STANDARD_VM_CONTRACT;

    // Get information about the NGEN'd method code.
    EECodeInfo codeInfo(pCode);
    _ASSERTE(codeInfo.IsValid());

    IJitManager::MethodRegionInfo methodRegionInfo;
    codeInfo.GetMethodRegionInfo(&methodRegionInfo);

    // NGEN can split code between hot and cold sections which are separate in memory.
    // Emit an entry for each section if it is used.
    if (methodRegionInfo.hotSize > 0)
    {
        LogMethod(pMethod, (PCODE)methodRegionInfo.hotStartAddress - baseAddr, methodRegionInfo.hotSize, optimizationTier);
    }

    if (methodRegionInfo.coldSize > 0)
    {
        LogMethod(pMethod, (PCODE)methodRegionInfo.coldStartAddress - baseAddr, methodRegionInfo.coldSize, optimizationTier);
    }
}

#endif // FEATURE_PERFMAP && !DACCESS_COMPILE
