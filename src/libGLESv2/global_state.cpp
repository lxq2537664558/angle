//
// Copyright(c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// global_state.cpp : Implements functions for querying the thread-local GL and EGL state.

#include "libGLESv2/global_state.h"

#include "common/debug.h"
#include "common/platform.h"
#include "common/tls.h"

#include "libANGLE/Debug.h"
#include "libANGLE/Thread.h"

namespace gl
{

Context *GetGlobalContext()
{
    egl::Thread *thread = egl::GetCurrentThread();
    return thread->getContext();
}

Context *GetValidGlobalContext()
{
    egl::Thread *thread = egl::GetCurrentThread();
    return thread->getValidContext();
}

}  // namespace gl

namespace egl
{

namespace
{

static TLSIndex threadTLS = TLS_INVALID_INDEX;
Debug *g_Debug            = nullptr;

Thread *AllocateCurrentThread()
{
    ASSERT(threadTLS != TLS_INVALID_INDEX);
    if (threadTLS == TLS_INVALID_INDEX)
    {
        return nullptr;
    }

    Thread *thread = new Thread();
    if (!SetTLSValue(threadTLS, thread))
    {
        ERR() << "Could not set thread local storage.";
        return nullptr;
    }

    return thread;
}

void AllocateDebug()
{
    // TODO(geofflang): Lock around global allocation. http://anglebug.com/2464
    if (g_Debug == nullptr)
    {
        g_Debug = new Debug();
    }
}

}  // anonymous namespace

Thread *GetCurrentThread()
{
    // Create a TLS index if one has not been created for this DLL
    if (threadTLS == TLS_INVALID_INDEX)
    {
        threadTLS = CreateTLSIndex();
    }

    Thread *current = static_cast<Thread *>(GetTLSValue(threadTLS));

    // ANGLE issue 488: when the dll is loaded after thread initialization,
    // thread local storage (current) might not exist yet.
    return (current ? current : AllocateCurrentThread());
}

Debug *GetDebug()
{
    AllocateDebug();
    return g_Debug;
}

}  // namespace egl

#if ANGLE_FORCE_THREAD_SAFETY == ANGLE_ENABLED
namespace angle
{
namespace
{
std::mutex g_Mutex;
}  // anonymous namespace

std::mutex &GetGlobalMutex()
{
    return g_Mutex;
}
}  // namespace angle
#endif

#ifdef ANGLE_PLATFORM_WINDOWS
namespace egl
{

namespace
{

bool DeallocateCurrentThread()
{
    Thread *thread = static_cast<Thread *>(GetTLSValue(threadTLS));
    SafeDelete(thread);
    return SetTLSValue(threadTLS, nullptr);
}

void DealocateDebug()
{
    SafeDelete(g_Debug);
}

bool InitializeProcess()
{
    ASSERT(g_Debug == nullptr);
    AllocateDebug();

    threadTLS = CreateTLSIndex();
    if (threadTLS == TLS_INVALID_INDEX)
    {
        return false;
    }

    return AllocateCurrentThread() != nullptr;
}

bool TerminateProcess()
{
    DealocateDebug();

    if (!DeallocateCurrentThread())
    {
        return false;
    }

    if (threadTLS != TLS_INVALID_INDEX)
    {
        TLSIndex tlsCopy = threadTLS;
        threadTLS        = TLS_INVALID_INDEX;

        if (!DestroyTLSIndex(tlsCopy))
        {
            return false;
        }
    }

    return true;
}

}  // anonymous namespace

}  // namespace egl

extern "C" BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID)
{
    switch (reason)
    {
        case DLL_PROCESS_ATTACH:
            return static_cast<BOOL>(egl::InitializeProcess());

        case DLL_THREAD_ATTACH:
            return static_cast<BOOL>(egl::AllocateCurrentThread() != nullptr);

        case DLL_THREAD_DETACH:
            return static_cast<BOOL>(egl::DeallocateCurrentThread());

        case DLL_PROCESS_DETACH:
            return static_cast<BOOL>(egl::TerminateProcess());
    }

    return TRUE;
}
#endif  // ANGLE_PLATFORM_WINDOWS
