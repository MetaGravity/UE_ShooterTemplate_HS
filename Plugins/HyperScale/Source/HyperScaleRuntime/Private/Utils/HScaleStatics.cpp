#include "Utils/HScaleStatics.h"

#include "Utils/HScaleConversionUtils.h"
#if UE_BUILD_SHIPPING
#define DEBUG_CALLSPACE_HS(Format, ...)
#else
#define DEBUG_CALLSPACE_HS(Format, ...) UE_LOG(LogNet, VeryVerbose, Format, __VA_ARGS__);
#endif

void FHScaleStatics::AppendUint64ToBuffer(const uint64_t Value, std::vector<uint8_t>& Buffer) {
	const size_t OriginalSize = Buffer.size();
	Buffer.resize(OriginalSize + sizeof(uint64_t));
	std::memcpy(Buffer.data() + OriginalSize, &Value, sizeof(uint64_t));
}

void FHScaleStatics::AppendFStringToBuffer(const FString& String, std::vector<uint8>& Buffer) {
	const std::string InputString(TCHAR_TO_UTF8(*String));

	// this requires optimization, as there is no need to write length of string
	uint8 StringLength = static_cast<uint8>(InputString.length());

	Buffer.insert(Buffer.end(), reinterpret_cast<const uint8_t*>(&StringLength), reinterpret_cast<const uint8_t*>(&StringLength) + sizeof(uint8));
	Buffer.insert(Buffer.end(), reinterpret_cast<const uint8_t*>(InputString.data()), reinterpret_cast<const uint8_t*>(InputString.data()) + StringLength);
}

uint64 FHScaleStatics::ConvertBufferToUInt64(const std::vector<uint8>& Buffer, const size_t StartIndex) {
	if (Buffer.size() < StartIndex + sizeof(uint64))
	{
		return 0;
	}

	uint64 Result = 0;
	std::memcpy(&Result, Buffer.data() + StartIndex, sizeof(uint64));

	return Result;
}

void FHScaleStatics::ConvertBufferToFString(FString& Value, const std::vector<uint8>& Buffer, size_t Offset) {
	if (Buffer.size() < Offset + sizeof(uint8))
	{
		return;
	}

	uint8 StringLength = 0;
	std::memcpy(&StringLength, Buffer.data() + Offset, sizeof(uint8));
	Offset += sizeof(uint8);

	if (Buffer.size() < Offset + StringLength)
	{
		return;
	}

	std::string Result(reinterpret_cast<const char*>(Buffer.data() + Offset), StringLength);

	Value = FString(UTF8_TO_TCHAR(Result.c_str()));
}

FString FHScaleStatics::PrintBytesFormat(const std::vector<uint8>& ByteBuffer) {
	FString HexString;
	for (uint8 Byte : ByteBuffer) { HexString += FString::Printf(TEXT("%02X"), Byte); }
	return HexString;
}

bool FHScaleStatics::IsClassSupportedForReplication(const UClass* Class) {
	if (!Class)
	{
		return false;
	}

	if (Class->HasAnyClassFlags(CLASS_ReplicationDataIsSetUp) || Class->IsSupportedForNetworking())
	{
		return true;
	}

	return false;
}

bool FHScaleStatics::IsObjectReplicated(const UObject* Object) {
	if (!IsValid(Object))
	{
		return false;
	}

	// @pavan: I have introduced this cause, static actors that are replciated are getting GUIDs and causing
	// issues while replicating
	// #todo: this should be disabled for clients other than game
	if (Object->IsFullNameStableForNetworking())
	{
		return false;
	}

	if (const AActor* Actor = Cast<AActor>(Object))
	{
		const AActor* ClassDefaultObject = Actor->GetClass()->GetDefaultObject<AActor>();

		return Actor != ClassDefaultObject && Actor->GetIsReplicated();
	}

	if (Object->IsSupportedForNetworking())
	{
		return true;
	}

	if (IsClassSupportedForReplication(Object->GetClass()))
	{
		if (const AActor* OuterActor = Object->GetTypedOuter<AActor>(); OuterActor && OuterActor->GetIsReplicated())
		{
			return true;
		}
	}

	return false;
}

int32 FHScaleStatics::GetFunctionCallspace(AActor* Actor, UFunction* Function, FFrame* Stack)
{
	if (GAllowActorScriptExecutionInEditor)
	{
		// Call local, this global is only true when we know it's being called on an editor-placed object
		DEBUG_CALLSPACE_HS(TEXT("GetFunctionCallspace ScriptExecutionInEditor: %s"), *Function->GetName());
		return FunctionCallspace::Local;
	}

	if ((Function->FunctionFlags & FUNC_Static) || (Actor->GetWorld() == nullptr))
	{
		// Use the same logic as function libraries for static/CDO called functions, will try to use the global context to check authority only/cosmetic
		DEBUG_CALLSPACE_HS(TEXT("GetFunctionCallspace Static: %s"), *Function->GetName());

		return GEngine->GetGlobalFunctionCallspace(Function, Actor, Stack);
	}

	const ENetRole LocalRole = Actor->GetLocalRole();

	// If we are on a client and function is 'skip on client', absorb it
	FunctionCallspace::Type Callspace = (LocalRole < ROLE_Authority) && Function->HasAllFunctionFlags(FUNC_BlueprintAuthorityOnly) ? FunctionCallspace::Absorbed : FunctionCallspace::Local;
	
	if (!IsValidChecked(Actor))
	{
		// Never call remote on a pending kill actor. 
		// We can call it local or absorb it depending on authority/role check above.
		DEBUG_CALLSPACE_HS(TEXT("GetFunctionCallspace: IsPendingKill %s %s"), *Function->GetName(), FunctionCallspace::ToString(Callspace));
		return Callspace;
	}

	if (Function->FunctionFlags & FUNC_NetRequest)
	{
		// Call remote
		DEBUG_CALLSPACE_HS(TEXT("GetFunctionCallspace NetRequest: %s"), *Function->GetName());
		return FunctionCallspace::Remote;
	}	
	
	if (Function->FunctionFlags & FUNC_NetResponse)
	{
		if (Function->RPCId > 0)
		{
			// Call local
			DEBUG_CALLSPACE_HS(TEXT("GetFunctionCallspace NetResponse Local: %s"), *Function->GetName());
			return FunctionCallspace::Local;
		}

		// Shouldn't happen, so skip call
		DEBUG_CALLSPACE_HS(TEXT("GetFunctionCallspace NetResponse Absorbed: %s"), *Function->GetName());
		return FunctionCallspace::Absorbed;
	}

	const EFunctionFlags bIsNetMultiCast = Function->FunctionFlags & FUNC_NetMulticast;

	const ENetMode NetMode = bIsNetMultiCast ? Actor->GetNetMode(): NM_Client;
	// Quick reject 2. Has to be a network game to continue
	if (NetMode == NM_Standalone)
	{
		if (LocalRole < ROLE_Authority && (Function->FunctionFlags & FUNC_NetServer))
		{
			// Don't let clients call server functions (in edge cases where NetMode is standalone (net driver is null))
			DEBUG_CALLSPACE_HS(TEXT("GetFunctionCallspace No Authority Server Call Absorbed: %s"), *Function->GetName());
			return FunctionCallspace::Absorbed;
		}

		// Call local
		return FunctionCallspace::Local;
	}
	
	// Dedicated servers don't care about "cosmetic" functions.
	if (NetMode == NM_DedicatedServer && Function->HasAllFunctionFlags(FUNC_BlueprintCosmetic))
	{
		DEBUG_CALLSPACE_HS(TEXT("GetFunctionCallspace Blueprint Cosmetic Absorbed: %s"), *Function->GetName());
		return FunctionCallspace::Absorbed;
	}

	if (!(Function->FunctionFlags & FUNC_Net))
	{
		// Not a network function
		DEBUG_CALLSPACE_HS(TEXT("GetFunctionCallspace Not Net: %s %s"), *Function->GetName(), FunctionCallspace::ToString(Callspace));
		return Callspace;
	}
	
	bool bIsServer = NetMode == NM_ListenServer || NetMode == NM_DedicatedServer;
	ENetRole RemoteRole = Actor->GetRemoteRole();

	// get the top most function
	while (Function->GetSuperFunction() != nullptr)
	{
		Function = Function->GetSuperFunction();
	}

	if (bIsNetMultiCast)
	{
		if(bIsServer)
		{
			// Server should execute locally and call remotely
			if (RemoteRole != ROLE_None)
			{
				DEBUG_CALLSPACE_HS(TEXT("GetFunctionCallspace Multicast: %s"), *Function->GetName());
				return (FunctionCallspace::Local | FunctionCallspace::Remote);
			}

			DEBUG_CALLSPACE_HS(TEXT("GetFunctionCallspace Multicast NoRemoteRole: %s"), *Function->GetName());
			return FunctionCallspace::Local;
		}
		else
		{
			// Client should only execute locally iff it is allowed to (function is not KismetAuthorityOnly)
			DEBUG_CALLSPACE_HS(TEXT("GetFunctionCallspace Multicast Client: %s %s"), *Function->GetName(), FunctionCallspace::ToString(Callspace));
			return Callspace;
		}
	}

	// if we are the server, and it's not a send-to-client function,
	if (bIsServer && !(Function->FunctionFlags & FUNC_NetClient))
	{
		// don't replicate
		DEBUG_CALLSPACE_HS(TEXT("GetFunctionCallspace Server calling Server function: %s %s"), *Function->GetName(), FunctionCallspace::ToString(Callspace));
		return Callspace;
	}
	// if we aren't the server, and it's not a send-to-server function,
	if (!bIsServer && !(Function->FunctionFlags & FUNC_NetServer))
	{
		// don't replicate
		DEBUG_CALLSPACE_HS(TEXT("GetFunctionCallspace Client calling Client function: %s %s"), *Function->GetName(), FunctionCallspace::ToString(Callspace));
		return Callspace;
	}

	// Check if the actor can potentially call remote functions	
	if (LocalRole == ROLE_Authority)
	{
		UNetConnection* NetConnection = Actor->GetNetConnection();
		if (NetConnection == nullptr)
		{
			UPlayer *ClientPlayer = Actor->GetNetOwningPlayer();
			if (ClientPlayer == nullptr)
			{
				// Check if a player ever owned this (topmost owner is playercontroller or beacon)
				if (Actor->HasNetOwner())
				{
					// Network object with no owning player, we must absorb
					DEBUG_CALLSPACE_HS(TEXT("GetFunctionCallspace Client without owner absorbed %s"), *Function->GetName());
					return FunctionCallspace::Absorbed;
				}
				
				// Role authority object calling a client RPC locally (ie AI owned objects)
				DEBUG_CALLSPACE_HS(TEXT("GetFunctionCallspace authority non client owner %s %s"), *Function->GetName(), FunctionCallspace::ToString(Callspace));
				return Callspace;
			}
			else if (Cast<ULocalPlayer>(ClientPlayer) != nullptr)
			{
				// This is a local player, call locally
				DEBUG_CALLSPACE_HS(TEXT("GetFunctionCallspace Client local function: %s %s"), *Function->GetName(), FunctionCallspace::ToString(Callspace));
				return Callspace;
			}
		}
		else if (!NetConnection->Driver || !NetConnection->Driver->World)
		{
			// NetDriver does not have a world, most likely shutting down
			DEBUG_CALLSPACE_HS(TEXT("GetFunctionCallspace NetConnection with no driver or world absorbed: %s %s %s"),
				*Function->GetName(), 
				NetConnection->Driver ? *NetConnection->Driver->GetName() : TEXT("NoNetDriver"),
				NetConnection->Driver && NetConnection->Driver->World ? *NetConnection->Driver->World->GetName() : TEXT("NoWorld"));
			return FunctionCallspace::Absorbed;
		}

		// There is a valid net connection, so continue and call remotely
	}

	// about to call remotely - unless the actor is not actually replicating
	if (RemoteRole == ROLE_None)
	{
		if (!bIsServer)
		{
			UE_LOG(LogNet, Warning, TEXT("Client is absorbing remote function %s on actor %s because RemoteRole is ROLE_None"), *Function->GetName(), *Actor->GetName() );
		}

		DEBUG_CALLSPACE_HS(TEXT("GetFunctionCallspace RemoteRole None absorbed %s"), *Function->GetName());
		return FunctionCallspace::Absorbed;
	}

	// Call remotely
	DEBUG_CALLSPACE_HS(TEXT("GetFunctionCallspace RemoteRole Remote %s"), *Function->GetName());
	return FunctionCallspace::Remote;
}