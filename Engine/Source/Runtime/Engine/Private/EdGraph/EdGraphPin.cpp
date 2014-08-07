// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "BlueprintUtilities.h"
#if WITH_EDITOR
#include "Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h"
#include "Slate.h"
#include "ScopedTransaction.h"
#include "Editor/UnrealEd/Public/Kismet2/Kismet2NameValidators.h"
#endif

#define LOCTEXT_NAMESPACE "EdGraph"

/////////////////////////////////////////////////////
// FEdGraphPinType

bool FEdGraphPinType::Serialize(FArchive& Ar)
{
	if (Ar.UE4Ver() < VER_UE4_EDGRAPHPINTYPE_SERIALIZATION)
	{
		return false;
	}

	Ar << PinCategory;
	Ar << PinSubCategory;

	// See: FArchive& operator<<( FArchive& Ar, FWeakObjectPtr& WeakObjectPtr )
	// The PinSubCategoryObject should be serialized into the package.
	if(!Ar.IsObjectReferenceCollector() || Ar.IsModifyingWeakAndStrongReferences() || Ar.IsPersistent())
	{
		UObject* Object = PinSubCategoryObject.Get(true);
		Ar << Object;
		if( Ar.IsLoading() || Ar.IsModifyingWeakAndStrongReferences() )
		{
			PinSubCategoryObject = Object;
		}
	}

	Ar << bIsArray;
	Ar << bIsReference;
	Ar << bIsWeakPointer;
	
	if (Ar.UE4Ver() >= VER_UE4_MEMBERREFERENCE_IN_PINTYPE)
	{
		Ar << PinSubCategoryMemberReference;
	}
	else if (Ar.IsLoading() && Ar.IsPersistent())
	{
		if ((PinCategory == TEXT("delegate")) || (PinCategory == TEXT("mcdelegate")))
		{
			if (const UFunction* Signature = Cast<const UFunction>(PinSubCategoryObject.Get()))
			{
				PinSubCategoryMemberReference.MemberName = Signature->GetFName();
				PinSubCategoryMemberReference.MemberParentClass = Signature->GetOwnerClass();
				PinSubCategoryObject = NULL;
			}
			else
			{
				ensure(true);
			}
		}
	}

	return true;
}

/////////////////////////////////////////////////////
// UEdGraphPin

UEdGraphPin::UEdGraphPin(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
#if WITH_EDITORONLY_DATA
	bHidden = false;
	bNotConnectable = false;
	bAdvancedView = false;
#endif // WITH_EDITORONLY_DATA
}

void UEdGraphPin::MakeLinkTo(UEdGraphPin* ToPin)
{
	Modify();

	if (ToPin != NULL)
	{
		ToPin->Modify();

		// Make sure we don't already link to it
		if (!LinkedTo.Contains(ToPin))
		{
			UEdGraphNode* MyNode = GetOwningNode();

			// Check that the other pin does not link to us
			ensureMsg(!ToPin->LinkedTo.Contains(this), *GetLinkInfoString( LOCTEXT("MakeLinkTo", "MakeLinkTo").ToString(), LOCTEXT("IsLinked", "is linked with pin").ToString(), ToPin));			    
			ensureMsg(MyNode->GetOuter() == ToPin->GetOwningNode()->GetOuter(), *GetLinkInfoString( LOCTEXT("MakeLinkTo", "MakeLinkTo").ToString(), LOCTEXT("OuterMismatch", "has a different outer than pin").ToString(), ToPin)); // Ensure both pins belong to the same graph

			// Add to both lists
			LinkedTo.Add(ToPin);
			ToPin->LinkedTo.Add(this);
		}
	}
}

void UEdGraphPin::BreakLinkTo(UEdGraphPin* ToPin)
{
	Modify();

	if (ToPin != NULL)
	{
		ToPin->Modify();

		// If we do indeed link to the passed in pin...
		if (LinkedTo.Contains(ToPin))
		{
			LinkedTo.Remove(ToPin);

			// Check that the other pin links to us
			ensureMsg(ToPin->LinkedTo.Contains(this), *GetLinkInfoString( LOCTEXT("BreakLinkTo", "BreakLinkTo").ToString(), LOCTEXT("NotLinked", "not reciprocally linked with pin").ToString(), ToPin) );
			ToPin->LinkedTo.Remove(this);
		}
		else
		{
			// Check that the other pin does not link to us
			ensureMsg(!ToPin->LinkedTo.Contains(this), *GetLinkInfoString( LOCTEXT("MakeLinkTo", "MakeLinkTo").ToString(), LOCTEXT("IsLinked", "is linked with pin").ToString(), ToPin));
		}
	}
}

void UEdGraphPin::BreakAllPinLinks()
{
	TArray<UEdGraphPin*> LinkedToCopy = LinkedTo;

	for (int32 LinkIdx = 0; LinkIdx < LinkedToCopy.Num(); LinkIdx++)
	{
		BreakLinkTo(LinkedToCopy[LinkIdx]);
	}
}


void UEdGraphPin::CopyPersistentDataFromOldPin(const UEdGraphPin& SourcePin)
{
	// The name matches already, doesn't get copied here
	// The PinType, Direction, and bNotConnectable are properties generated from the schema

	// Only move the default value if it was modified; inherit the new default value otherwise
	if (SourcePin.DefaultValue != SourcePin.AutogeneratedDefaultValue || SourcePin.DefaultObject != NULL)
	{
		DefaultObject = SourcePin.DefaultObject;
		DefaultValue = SourcePin.DefaultValue;
	}

	const bool bTextValueWasModified = (SourcePin.DefaultTextValue.ToString() != SourcePin.AutogeneratedDefaultValue);
	if (bTextValueWasModified)
	{
		DefaultTextValue = SourcePin.DefaultTextValue;
	}

	// Copy the links
	for (int32 LinkIndex = 0; LinkIndex < SourcePin.LinkedTo.Num(); ++LinkIndex)
	{
		UEdGraphPin* OtherPin = SourcePin.LinkedTo[LinkIndex];
		check(NULL != OtherPin);
		
		Modify();
		OtherPin->Modify();

		OtherPin->MakeLinkTo(this);
	}

	// If the source pin is split, then split the new one
	if (SourcePin.SubPins.Num() > 0)
	{
		GetSchema()->SplitPin(this);
	}

#if WITH_EDITORONLY_DATA
	// Copy advanced visibility property, since it can be changed by user
	bAdvancedView = SourcePin.bAdvancedView;
#endif // WITH_EDITORONLY_DATA
}

const class UEdGraphSchema* UEdGraphPin::GetSchema() const
{
#if WITH_EDITOR
	return GetOwningNode()->GetGraph()->GetSchema();
#else
	return NULL;
#endif	//WITH_EDITOR
}

FString UEdGraphPin::GetDefaultAsString() const
{
	if(DefaultObject != NULL)
	{
		return DefaultObject->GetFullName();
	}
	else
	{
		return DefaultValue;
	}
}

FText UEdGraphPin::GetDisplayName() const
{
	return FText::FromString(GetSchema()->GetPinDisplayName(this));
}

const FString UEdGraphPin::GetLinkInfoString( const FString& InFunctionName, const FString& InInfoData, const UEdGraphPin* InToPin ) const
{
#if WITH_EDITOR
	const FString FromPinName = PinName;
	const UEdGraphNode* FromPinNode = Cast<UEdGraphNode>(GetOuter());
	const FString FromPinNodeName = (FromPinNode != NULL) ? FromPinNode->GetNodeTitle(ENodeTitleType::ListView).ToString() : TEXT("Unknown");
	
	const FString ToPinName = InToPin->PinName;
	const UEdGraphNode* ToPinNode = Cast<UEdGraphNode>(InToPin->GetOuter());
	const FString ToPinNodeName = (ToPinNode != NULL) ? ToPinNode->GetNodeTitle(ENodeTitleType::ListView).ToString() : TEXT("Unknown");
	const FString LinkInfo = FString::Printf( TEXT("UEdGraphPin::%s Pin '%s' on node '%s' %s '%s' on node '%s'"), *InFunctionName, *ToPinName, *ToPinNodeName, *InInfoData, *FromPinName, *FromPinNodeName);
	return LinkInfo;
#else
	return FString();
#endif
}

FString FGraphDisplayInfo::GetNotesAsString() const
{
	FString Result;

	if (Notes.Num() > 0)
	{
		Result = TEXT("(");
		for (int32 NoteIndex = 0; NoteIndex < Notes.Num(); ++NoteIndex)
		{
			if (NoteIndex > 0)
			{
				Result += TEXT(", ");
			}
			Result += Notes[NoteIndex];
		}
		Result += TEXT(")");
	}

	return Result;
}

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
