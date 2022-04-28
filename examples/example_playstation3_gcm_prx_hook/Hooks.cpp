#include "Hooks.hpp"
#include <cell/gcm.h>

ImportExportDetour* cellGcmSetFlipCommandHk;

void cellGcmSetFlipCommandHook(CellGcmContextData* gcmThis, uint8_t id)
{


    cellGcmSetFlipCommandHk->GetOriginal<void>(gcmThis, id);
}

void InstallHooks()
{
    // Swap buffers?? Universal for all games
    cellGcmSetFlipCommandHk = new ImportExportDetour(ImportExportDetour::Import, "cellGcmSys", 0x21397818, (uintptr_t)cellGcmSetFlipCommandHook);
}

void RemoveHooks()
{
    delete cellGcmSetFlipCommandHk;
}