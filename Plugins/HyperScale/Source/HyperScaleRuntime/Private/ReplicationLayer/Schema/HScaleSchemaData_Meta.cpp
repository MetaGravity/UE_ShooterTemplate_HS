// Copyright 2024 Metagravity. All Rights Reserved.


#include "ReplicationLayer/Schema/HScaleSchemaData_Meta.h"

void UHScaleSchemaData_Meta::Parse(TSharedPtr<FJsonObject> Data)
{
	if (Data)
	{
		Data->TryGetStringField("name", SchemaName);
		Data->TryGetStringField("server_version", ServerVersion);
	}
}
