#include "KeepersChildActorHost.h"

#include "Components/ChildActorComponent.h"
#include "UObject/UnrealType.h"

namespace KeepersChildActors
{
	static const FString ComponentPrefix = TEXT("ChildActor_");

	static void SetObjectProperty(UObject* Container, FObjectPropertyBase* Property, UObject* Value)
	{
		Property->SetObjectPropertyValue(Property->ContainerPtrToValuePtr<void>(Container), Value);
	}

	/** Points empty object variables on the child that can hold the host back at the host. */
	static void LinkBackToHost(AActor* Child, AActor* Host)
	{
		for (TFieldIterator<FObjectPropertyBase> It(Child->GetClass()); It; ++It)
		{
			FObjectPropertyBase* Property = *It;
			// Only Blueprint variables: engine properties such as Owner or Instigator are left alone.
			if (Property->GetOwnerClass() == nullptr || Property->GetOwnerClass()->ClassGeneratedBy == nullptr)
			{
				continue;
			}
			if (!Host->IsA(Property->PropertyClass))
			{
				continue;
			}
			if (Property->GetObjectPropertyValue_InContainer(Child) == nullptr)
			{
				SetObjectProperty(Child, Property, Host);
			}
		}
	}
}

void AKeepersChildActorHost::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	RefreshChildActorReferences();
}

void AKeepersChildActorHost::RefreshChildActorReferences()
{
	TInlineComponentArray<UChildActorComponent*> ChildComponents(this);
	for (UChildActorComponent* Component : ChildComponents)
	{
		AActor* Child = Component->GetChildActor();
		if (!Child)
		{
			continue;
		}

		const FString ComponentName = Component->GetName();
		if (ComponentName.StartsWith(KeepersChildActors::ComponentPrefix))
		{
			const FName VariableName(*ComponentName.RightChop(KeepersChildActors::ComponentPrefix.Len()));
			FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(GetClass(), VariableName);
			if (Property && Child->IsA(Property->PropertyClass))
			{
				KeepersChildActors::SetObjectProperty(this, Property, Child);
			}
		}

		KeepersChildActors::LinkBackToHost(Child, this);
	}
}
