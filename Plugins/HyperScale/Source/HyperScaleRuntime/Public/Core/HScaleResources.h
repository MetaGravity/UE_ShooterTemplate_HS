// Copyright 2024 Metagravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "HScaleCommons.h"
#include "Net/RepLayout.h"
#include "Utils/HScalePrivateAccessors.h"
#include "HScaleResources.generated.h"

// Enable access to FRepLayout.Cmds
HSCALE_IMPLEMENT_GET_PRIVATE_VAR(FRepLayout, Cmds, TArray<FRepLayoutCmd>);
HSCALE_IMPLEMENT_GET_PRIVATE_VAR(FRepLayout, Parents, TArray<FRepParentCmd>);


UENUM(BlueprintType)
enum class EHScale_Blend : uint8
{
	Owner,
	Select,
	Average
};

UENUM(BlueprintType)
enum class EHScale_Ownership : uint8
{
	Creator,
	None
};

UENUM(BlueprintType)
enum class EHScale_Lifetime : uint8
{
	Owner,
	Detached
};

enum EHScale_QuarkEventType : quark_event_class_t
{
	//Predefined System Events
	Invalid = 0,
	ObjectsDespawned = quark::known_event::object_despawned,
	ForgetObjects = quark::known_event::forget_objects,
	ForgetPlayers = quark::known_event::forget_players,
	PlayerDisconnected = quark::known_event::player_disconnected,
	GetFreePlayerObjectIds = 5,
	GetFreeGlobalObjectIds = 6,

	//Custom Events
	Custom = 1001
};

USTRUCT(BlueprintType)
struct HYPERSCALERUNTIME_API FHScale_ServerConfig
{
	GENERATED_BODY()

	FHScale_ServerConfig()
		: NameOptional("Local Host"), Address(HSCALE_LOCAL_IP_ADDRESS), Port(5670) {}

	UPROPERTY(EditAnywhere, meta = (DisplayName = "Name (Optional)"))
	FString NameOptional;

	UPROPERTY(EditAnywhere)
	FString Address;

	UPROPERTY(EditAnywhere, meta = (UIMin = 1, ClampMin = 1, UIMax = 65535, ClampMax = 65535))
	int32 Port;

public:
	bool operator==(const FHScale_ServerConfig& Other) const { return NameOptional.Equals(Other.NameOptional) && Address.Equals(Other.Address) && Port == Other.Port; }
	bool IsSame(const FHScale_ServerConfig& Other) const { return Address.Equals(Other.Address, ESearchCase::IgnoreCase) && Port == Other.Port; }

	bool IsDefault() const { return *this == FHScale_ServerConfig(); }
	bool IsLocal() const { return Address.Equals(HSCALE_LOCAL_IP_ADDRESS); }
	FString ToString() const { return Address + ':' + FString::FromInt(Port); }
	FText GetDisplayName() const { return FText::FromString(NameOptional.IsEmpty() ? ToString() : NameOptional); }
};

USTRUCT(BlueprintType)
struct HYPERSCALERUNTIME_API FHScale_ServerConfigList
{
	GENERATED_BODY()

	FHScale_ServerConfigList();

protected:
	UPROPERTY(VisibleAnywhere)
	FHScale_ServerConfig SelectedServer;

public:
	UPROPERTY(EditAnywhere, EditFixedSize)
	TArray<FHScale_ServerConfig> EditorServerList;

	void ValidateServerList();

	void SelectServer(const FHScale_ServerConfig& Server);
	void SelectExistingServer(const FHScale_ServerConfig& Server);

	const FHScale_ServerConfig& GetSelectedServer() const { return SelectedServer; }
	const TArray<FHScale_ServerConfig>& GetAvailableServers() const { return EditorServerList; }
	bool HasLimitReached() const { return EditorServerList.Num() >= HSCALE_MAX_ALLOWED_EDITOR_SERVERS; }

protected:
	int32 FindServerIndex(const FHScale_ServerConfig& Server) const
	{
		return EditorServerList.IndexOfByPredicate([&Server](const FHScale_ServerConfig& Other)
		{
			return Other == Server;
		});
	}

private:
	uint8 EditorServerActiveIndex;
};

enum EHScaleGuidFlags : uint8
{
	HSGF_None = 0,

	HSGF_NetworkRef = 0x01, ///< This indicates referencing another entity in network
	HSGF_OuterRef = 0x02,   ///< This indicates referencing static outer data for object
	//SGF_...							= 0x04,	///< Free flag
	//SGF_...							= 0x08,	///< Free flag
	//SGF_...							= 0x10,	///< Free flag
	//SGF_...							= 0x20,	///< Free flag
	//SGF_...							= 0x40,	///< Free flag
	//SGF_...							= 0x80,	///< Free flag
};

USTRUCT()
struct FHScaleNetGUID
{
	GENERATED_BODY()

#ifndef WITH_ADVANCED_HS_GUID_HASHING
#define WITH_ADVANCED_HS_GUID_HASHING			1
#endif


	FHScaleNetGUID()
		: EntityId(0) {}

	static FHScaleNetGUID Create(const uint64 NetworkId)
	{
		FHScaleNetGUID NewGuid;
		NewGuid.EntityId = NetworkId;
		return NewGuid;
	}

	static FHScaleNetGUID Create_Object(const uint64 NetworkId)
	{
		check(NetworkId != 0);
		check((NetworkId >> 32) != 0); // The upper bits cannot be 0 (it's not a valid player)

		FHScaleNetGUID NewGuid;
		NewGuid.EntityId = NetworkId;
		return NewGuid;
	}

	static FHScaleNetGUID Create_Player(const uint32 NetworkId)
	{
		check(NetworkId != 0);

		FHScaleNetGUID NewGuid;
		NewGuid.EntityId = NetworkId;
		return NewGuid;
	}

	/** A Valid but unassigned NetGUID */
	static FHScaleNetGUID GetDefault()
	{
		return Create(0);
	}

	void Reset()
	{
		EntityId = 0;
	}

	bool IsValid() const
	{
		return EntityId > 0;
	}

	bool IsStatic() const
	{
		constexpr uint64_t Mask = 0xFFFFFFFF00000000;
		return IsValid() && ((EntityId & Mask) == Mask);
	}

	bool IsDynamic() const
	{
		return IsValid() && !IsStatic();
	}

	bool IsPlayer() const
	{
		// If the object part is zero then is player
		return IsValid() && EntityId < UINT32_MAX;
	}

	uint32 GetPlayer() const
	{
		check(IsPlayer());
		return EntityId;
	}

	bool IsObject() const
	{
		return IsValid() && !IsPlayer();
	}

	uint64 GetObject() const
	{
		check(IsObject());
		return EntityId;
	}

	uint64 Get() const
	{
		return EntityId;
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("%llu"), EntityId);
	}

	friend bool operator==(const FHScaleNetGUID& X, const FHScaleNetGUID& Y)
	{
		return (X.EntityId == Y.EntityId)/* && (X.Flags == Y.Flags)*/;
	}

	friend bool operator!=(const FHScaleNetGUID& X, const FHScaleNetGUID& Y)
	{
		return (X.EntityId != Y.EntityId)/* || (X.Flags != Y.Flags)*/;
	}

	friend FArchive& operator<<(FArchive& Ar, FHScaleNetGUID& G)
	{
		Ar.SerializeIntPacked64(G.EntityId);
		return Ar;
	}

	friend uint32 GetTypeHash(const FHScaleNetGUID& Guid)
	{
		return ::GetTypeHash(Guid.EntityId);
	}

private:
	/** Reference network ID */
	uint64 EntityId;
};

struct FHScaleExportFlags
{
	union
	{
		struct
		{
			uint8 bHasPath : 1;
			uint8 bNoLoad : 1;
		};

		uint8 Value;
	};

	FHScaleExportFlags()
	{
		Value = 0;
	}
};
