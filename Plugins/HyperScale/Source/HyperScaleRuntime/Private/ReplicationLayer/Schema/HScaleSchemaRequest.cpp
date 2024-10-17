// Copyright 2024 Metagravity. All Rights Reserved.


#include "ReplicationLayer/Schema/HScaleSchemaRequest.h"

#include "Core/HScaleResources.h"
#include "Interfaces/IHttpResponse.h"
#include "Kismet/GameplayStatics.h"

UHScaleSchemaRequest* UHScaleSchemaRequest::BuildRequest(UObject* WorldContextObject, const FString& InURL)
{
	if (!IsValid(WorldContextObject)) return nullptr;

	UObject* Outer = WorldContextObject;
	if (UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject))
	{
		Outer = GameInstance;
	}

	UHScaleSchemaRequest* Result = NewObject<UHScaleSchemaRequest>(Outer);

	// Set verb
	// --------------------------
	Result->HttpRequest->SetVerb(TEXT("GET"));

	// Set URL
	// --------------------------
	FString URL = TEXT("https://");
	FString ServerAddress, Port;
	const FString AppendPart = TEXT("/info/schema?format=json");
	if (InURL.Split(":", &ServerAddress, &Port, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
	{
		// If the URL contains port, exclude it from URL
		URL += ServerAddress + AppendPart;
	}
	else
	{
		URL += InURL + AppendPart;
	}
	Result->HttpRequest->SetURL(URL);

	// --------------------------

	UE_LOG(Log_HyperScaleGlobals, Log, TEXT("Scheme URL request: %s %s"), *Result->HttpRequest->GetVerb(), *Result->HttpRequest->GetURL());
	return Result;
}

void UHScaleSchemaRequest::HandleOnRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	UHScaleSchema* Content = NewObject<UHScaleSchema>(GetOuter());
	check(Content);

	if (!bWasSuccessful || !Response || Response.Get()->GetResponseCode() != 200)
	{
		Content->Initialize(nullptr);
		OnCompleted.ExecuteIfBound(false, Content);
		return;
	}

	TSharedPtr<FJsonObject> OutJson;
	FString StringContent = Response.Get()->GetContentAsString();
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(MoveTemp(StringContent));

	const bool bSuccessful = FJsonSerializer::Deserialize(Reader, OutJson);
	if (bSuccessful)
	{
		Content->Initialize(OutJson);
	}
	else
	{
		Content->Initialize(nullptr);
	}

	OnCompleted.ExecuteIfBound(bSuccessful, Content);
}

void UHScaleSchemaRequest::ProcessRequest()
{
	if (bIsActive)
	{
		return;
	}
	bIsActive = true;

	if (!HttpRequest->OnProcessRequestComplete().IsBound())
	{
		HttpRequest->OnProcessRequestComplete().BindUObject(this, &ThisClass::HandleOnRequestComplete);
	}

	HttpRequest->ProcessRequest();
}
