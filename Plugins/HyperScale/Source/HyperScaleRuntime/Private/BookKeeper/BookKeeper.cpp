#include "BookKeeper/HScaleBookKeeper.h"

uint64 FHScaleBookKeeper::GetClassId(const FNetworkGUID& NetGUID) { return 0; }



uint64 FHScaleBookKeeper::GetClassId(const ObjectId ObjId) { return 0; }
UClass* FHScaleBookKeeper::GetClass(const uint64 ClassId) { return nullptr; }
UClass* FHScaleBookKeeper::GetClass(const FNetworkGUID& NetGUID) { return nullptr; }
ObjectId FHScaleBookKeeper::GetOrCreateObjectId(const FNetworkGUID& NetGUID) { return 0; }