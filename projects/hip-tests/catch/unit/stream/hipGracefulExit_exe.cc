#include <hip/hip_runtime.h>

#include <windows.h>
#include <tlhelp32.h>

#include <cstdio>
#include <set>

// Lightweight kernel — just enough to keep the worker thread busy.
__global__ void increment(float* data, int n) {
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i < n) data[i] += 1.0f;
}

static std::set<DWORD> snapshotThreads() {
  std::set<DWORD> ids;
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (snap == INVALID_HANDLE_VALUE) return ids;
  THREADENTRY32 te{};
  te.dwSize = sizeof(te);
  DWORD pid = GetCurrentProcessId();
  if (Thread32First(snap, &te)) {
    do {
      if (te.th32OwnerProcessID == pid) ids.insert(te.th32ThreadID);
    } while (Thread32Next(snap, &te));
  }
  CloseHandle(snap);
  return ids;
}

// Spawn a HIP stream (which internally creates a HostQueue worker thread in
// clr/rocclr/platform/commandqueue.cpp), queue async GPU work on it, then
// kill the worker thread ungracefully via TerminateThread so that the atexit
// cleanup path (__hipUnregisterFatBinary -> SyncAllStreams -> HostQueue::finish)
// is forced to call finish() on a dead worker thread.
//
// Without the fix for rocm-systems PR#3790: finish() blocks waiting for a
// signal from the dead thread -> hangs.
// With the fix: exits cleanly with code 0.
int main() {
  float* d_data = nullptr;
  if (hipMalloc(&d_data, 1024 * sizeof(float)) != hipSuccess) return 1;

  // Stabilize all HIP-internal background threads before snapshotting.
  (void)hipDeviceSynchronize();
  Sleep(200);

  auto before = snapshotThreads();
  printf("[hipGracefulExit_exe] threads before hipStreamCreate: %zu\n", before.size());
  fflush(stdout);

  hipStream_t stream;
  if (hipStreamCreate(&stream) != hipSuccess) return 1;
  Sleep(100);  // let the HostQueue worker thread start

  auto after = snapshotThreads();
  printf("[hipGracefulExit_exe] threads after  hipStreamCreate: %zu\n", after.size());
  fflush(stdout);

  // Queue async work while the worker thread is still alive.
  increment<<<4, 256, 0, stream>>>(d_data, 1024);

  // The HostQueue worker is spawned by the host thread inside hipStreamCreate,
  // so it has the highest TID among all new threads. Iterate in reverse (descending
  // TID) and kill the first new thread we find — that is the worker thread.
  int killed = 0;
  for (auto it = after.rbegin(); it != after.rend(); ++it) {
    DWORD tid = *it;
    if (!before.count(tid)) {
      HANDLE h = OpenThread(THREAD_TERMINATE, FALSE, tid);
      if (h) {
        printf("[hipGracefulExit_exe] TerminateThread tid=%lu (worker)\n", tid);
        fflush(stdout);
        TerminateThread(h, 0);
        CloseHandle(h);
        ++killed;
      }
      break;  // only kill the worker, leave the host thread alive
    }
  }

  if (killed == 0) {
    printf("[hipGracefulExit_exe] no new threads found (AMD_DIRECT_DISPATCH mode?), skipping\n");
    fflush(stdout);
  }

  // Do NOT call hipStreamDestroy — the stream stays in the active list.
  // atexit: __hipUnregisterFatBinary -> SyncAllStreams -> finish() is called
  // on this stream whose worker thread is now dead.
  // Without the fix: finish() blocks on the dead thread -> hangs.
  // With the fix: exits cleanly with code 0.
  return 0;
}
