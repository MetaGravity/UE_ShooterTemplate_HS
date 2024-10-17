// Copyright 2024 Metagravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "NativeGameplayTags.h"
#include "InstancedStruct.h"
#include "HScaleReplicationResources.generated.h"

class UHScaleConnection;
class UHScaleRepDriver;

namespace HScaleGameplayTag
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(HyperScale);

	//~ Begin Roles
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(HyperScale_Role);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(HyperScale_Role_Client);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(HyperScale_Role_WorldInitAgent);
	//~ End Roles
}

/**
 * Defines rules and conditions that has to be met for class
 * for its data replication to network
 */
USTRUCT(BlueprintType)
struct FHyperScale_ReplicationRule
{
	GENERATED_BODY()

	virtual ~FHyperScale_ReplicationRule() = default;

	virtual bool IsReplicated(UHScaleConnection* Connection) const PURE_VIRTUAL(FHyperScale_ReplicationRule::IsReplicated, return false;);
};

/**
 * Class with this rule option will not be replicated never to network from client site
 * even when the class has 'bReplicates' option enabled
 */
USTRUCT(BlueprintType, DisplayName="Never Replicate")
struct FHyperScale_ReplicationRule_NeverReplicate : public FHyperScale_ReplicationRule
{
	GENERATED_BODY()

	virtual bool IsReplicated(UHScaleConnection* Connection) const override { return false; }
};

/**
 * Class with this rule option will be replicated only
 * if a level was opened with specific role option
 */
USTRUCT(BlueprintType, DisplayName="Role Replicated")
struct FHyperScale_ReplicationRule_RoleReplicated : public FHyperScale_ReplicationRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories="HyperScale.Role"))
	FGameplayTag Role;

	virtual bool IsReplicated(UHScaleConnection* Connection) const override;
};

/**
 * Class with this rule option will be replicated only
 * if a level was opened with any option from the list
 */
USTRUCT(BlueprintType, DisplayName="Role-query Replicated")
struct FHyperScale_ReplicationRule_RoleQueryReplicated : public FHyperScale_ReplicationRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories="HyperScale.Role"))
	FGameplayTagQuery Query;

	virtual bool IsReplicated(UHScaleConnection* Connection) const override;
};

/**
 * 
 */
USTRUCT(BlueprintType)
struct FHScale_ReplicationClassOptions
{
	GENERATED_BODY()

	FHScale_ReplicationClassOptions()
		: bStaticClass(false), bEditable(true) {}

	explicit FHScale_ReplicationClassOptions(const bool bIsStaticClass, const bool bIsEditable)
		: bStaticClass(bIsStaticClass), bEditable(bIsEditable) {}

	explicit FHScale_ReplicationClassOptions(FString&& InNote, const bool bIsStaticClass, const bool bIsEditable)
		: Note(MoveTemp(InNote)), bStaticClass(bIsStaticClass), bEditable(bIsEditable) {}

	/**
	 * Defines class that will be customized with conditions for networking replication
	 * The class definition includes all children classes
	 */
	UPROPERTY(VisibleAnywhere)
	FString Note;

	/**
	 * Defines class that will be customized with conditions for networking replication
	 * The class definition includes all children classes
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "!bStaticClass"))
	TSoftClassPtr<AActor> ActorClass;

	/** 
	 * Defines replication rule, in which condition can be class replicated
	 * If None, then the class will be replicated
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "bEditable", ExcludeBaseStruct, BaseStruct = "/Script/HyperScaleRuntime.HyperScale_ReplicationRule"))
	FInstancedStruct ReplicationCondition;

	bool operator==(const AActor* Other) const { return ::IsValid(Other) && Other->GetClass() == ActorClass.Get(); }
	bool operator<=(const TSoftClassPtr<AActor>& Other) const;
	bool IsValid() const { return ActorClass.ToSoftObjectPath().IsValid(); }
	bool IsEditable() const { return bEditable; }

protected:
	UPROPERTY()
	uint8 bStaticClass : 1;

	UPROPERTY()
	uint8 bEditable : 1;
};

struct FHScale_ReplicationExcludedClass
{
	TSoftClassPtr<AActor> ActorClass;
	FHyperScale_ReplicationRule RepCondition;
};

struct FHScale_ReplicationExcludedList
{
	void Add(const FHScale_ReplicationExcludedClass& Item);
	bool CanReplicated(const AActor* Actor) const;

private:
	TArray<FHScale_ReplicationExcludedClass> ExcludedData;
};

struct FHScaleRepFlags : public FReplicationFlags
{
	/** True, only if scheme defines authority over the object */
	uint8 bOwnerBlendMode : 1;
	uint8 bHasAnyNetOwner : 1;
};