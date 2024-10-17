// Copyright 2024 Metagravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "HScaleSchema.h"
#include "HttpModule.h"
#include "UObject/Object.h"
#include "HScaleSchemaRequest.generated.h"

/**
 * 
 */
UCLASS()
class UHScaleSchemaRequest : public UObject
{
	GENERATED_BODY()
	
	DECLARE_DELEGATE_TwoParams(FHScale_SchemaRequestCompleted, const bool /** bSuccessful */, UHScaleSchema* /** Content */);

	UHScaleSchemaRequest()
		: bIsActive(false) {}

public:
	/**
	 * Creates request that will try to download schema data from server
	 * This data will be used for replication system and defines replication rules for each class
	 * To download the data call function ProcessRequest()
	 */
	static UHScaleSchemaRequest* BuildRequest(UObject* WorldContextObject, const FString& InURL);

	/** Fired, after the request will be completed */
	FHScale_SchemaRequestCompleted OnCompleted;

	/**
	 * Start request process of downloading data
	 */
	void ProcessRequest();

private:
	void HandleOnRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

private:
	bool bIsActive;
	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
};
