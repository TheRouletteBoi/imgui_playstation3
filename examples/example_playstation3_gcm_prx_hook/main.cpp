#include <cellstatus.h>
#include <sys/prx.h>
#include <sys/ppu_thread.h>
#include <sys/timer.h>
#include <cell/pad.h>

SYS_MODULE_INFO(playstation3_gcm_prx_hook, 0, 1, 1);
SYS_MODULE_START(module_start);
SYS_MODULE_STOP(module_stop);

sys_ppu_thread_t gMainPpuThreadId = SYS_PPU_THREAD_ID_INVALID;

CDECL_BEGIN
int module_start(unsigned int args, void* argp)
{
    sys_ppu_thread_create(&gMainPpuThreadId, [](uint64_t arg) -> void
    {
        // TODO!

        sys_ppu_thread_exit(0);

    }, 0, 1059, 4096, SYS_PPU_THREAD_CREATE_JOINABLE, "playstation3_gcm_prx_hook_thread");

    return 0;
}

int module_stop(unsigned int args, void* argp)
{
    if (gMainPpuThreadId != SYS_PPU_THREAD_ID_INVALID)
    {
        uint64_t exitCode;
        sys_ppu_thread_join(gMainPpuThreadId, &exitCode);
    }

    return 0;
}
CDECL_END