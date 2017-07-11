// Copyright 2017, Institute for Artificial Intelligence - University of Bremen
// Author: Andrei Haidu (http://haidu.eu)

#include "Hand.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "HandOrientationParser.h"
#include "Engine/Engine.h"

// Sets default values
AHand::AHand()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Fixation grasp parameters	
	bFixationGraspEnabled = true;
	bTwoHandsFixationGraspEnabled = true;
	bGraspHeld = false;
	OneHandFixationMaximumMass = 5.f;
	OneHandFixationMaximumLength = 50.f;
	TwoHandsFixationMaximumMass = 15.f;
	TwoHandsFixationMaximumLength = 120.f;

	// Set attachement collision component
	FixationGraspArea = CreateDefaultSubobject<USphereComponent>(TEXT("FixationGraspArea"));
	FixationGraspArea->SetupAttachment(GetRootComponent());
	FixationGraspArea->InitSphereRadius(4.f);

	// Set default as left hand
	HandType = EHandType::Left;

	// Set skeletal mesh default physics related values
	USkeletalMeshComponent* const SkelComp = GetSkeletalMeshComponent();
	SkelComp->SetSimulatePhysics(true);
	SkelComp->SetEnableGravity(false);
	SkelComp->SetCollisionProfileName(TEXT("BlockAll"));
	SkelComp->bGenerateOverlapEvents = true;

	// Angular drive default values
	AngularDriveMode = EAngularDriveMode::SLERP;
	Spring = 9000.0f;
	Damping = 1000.0f;
	ForceLimit = 0.0f;

	// Set fingers and their bone names default values
	AHand::SetupHandDefaultValues(HandType);

	// Set skeletal default values
	//AHand::SetupSkeletalDefaultValues(GetSkeletalMeshComponent());

	GraspPtr = MakeShareable(new Grasp());
}

// Called when the game starts or when spawned
void AHand::BeginPlay()
{
	Super::BeginPlay();

	// Disable tick as default
	SetActorTickEnabled(true);

	// Bind overlap events
	FixationGraspArea->OnComponentBeginOverlap.AddDynamic(this, &AHand::OnFixationGraspAreaBeginOverlap);
	FixationGraspArea->OnComponentEndOverlap.AddDynamic(this, &AHand::OnFixationGraspAreaEndOverlap);

	// Setup the values for controlling the hand fingers
	AHand::SetupAngularDriveValues(AngularDriveMode);

}

// Called every frame, used for motion control
void AHand::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	//if (TwoHandsGraspableObject)
	//{
	//	UE_LOG(LogTemp, Warning, TEXT("%s : TwoHandsGraspableObject : %s"), *GetName(), *TwoHandsGraspableObject->GetName());
	//}

	//if (!AHand::IsTwoHandGraspStillValid())
	//{
	//	AHand::DetachFixationGrasp();
	//}

}

// Update default values if properties have been changed in the editor
#if WITH_EDITOR
void AHand::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	// Call the base class version  
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Get the name of the property that was changed  
	FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	// If hand type has been changed
	if ((PropertyName == GET_MEMBER_NAME_CHECKED(AHand, HandType)))
	{
		AHand::SetupHandDefaultValues(HandType);
	}

	// If the skeletal mesh has been changed
	if ((PropertyName == GET_MEMBER_NAME_CHECKED(AHand, GetSkeletalMeshComponent())))
	{
		//AHand::SetupSkeletalDefaultValues(GetSkeletalMeshComponent());
	}

	UE_LOG(LogTemp, Warning, TEXT("Selected property name: %s"), *PropertyName.ToString());
}
#endif  

// Check if the object in reach is one-, two-hand(s), or not graspable
void AHand::OnFixationGraspAreaBeginOverlap(class UPrimitiveComponent* HitComp, class AActor* OtherActor,
	class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult)
{
	// Check if object is graspable
	const uint8 GraspType = CheckObjectGraspableType(OtherActor);

	if (GraspType == ONE_HAND_GRASPABLE)
	{
		OneHandGraspableObjects.Emplace(Cast<AStaticMeshActor>(OtherActor));
	}
	else if (GraspType == TWO_HANDS_GRASPABLE)
	{
		TwoHandsGraspableObject = Cast<AStaticMeshActor>(OtherActor);
	}
}

// Object out or grasping reach, remove as possible grasp object
void AHand::OnFixationGraspAreaEndOverlap(class UPrimitiveComponent* HitComp, class AActor* OtherActor,
	class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	// If present, remove from the graspable objects
	OneHandGraspableObjects.Remove(Cast<AStaticMeshActor>(OtherActor));

	if (TwoHandsGraspableObject && OtherActor == TwoHandsGraspableObject)
	{
		TwoHandsGraspableObject = nullptr;
	}
}

// Update the grasp pose
void AHand::UpdateGrasp(const float Goal)
{
		if (!OneHandGraspedObject)
		{
			for (const auto& ConstrMapItr : Thumb.FingerPartToConstraint)
			{
				ConstrMapItr.Value->SetAngularOrientationTarget(FQuat(FRotator(0.f, 0.f, Goal * 100.f)));
			}
			for (const auto& ConstrMapItr : Index.FingerPartToConstraint)
			{
				ConstrMapItr.Value->SetAngularOrientationTarget(FQuat(FRotator(0.f, 0.f, Goal * 100.f)));
			}
			for (const auto& ConstrMapItr : Middle.FingerPartToConstraint)
			{
				ConstrMapItr.Value->SetAngularOrientationTarget(FQuat(FRotator(0.f, 0.f, Goal * 100.f)));
			}
			for (const auto& ConstrMapItr : Ring.FingerPartToConstraint)
			{
				ConstrMapItr.Value->SetAngularOrientationTarget(FQuat(FRotator(0.f, 0.f, Goal * 100.f)));
			}
			for (const auto& ConstrMapItr : Pinky.FingerPartToConstraint)
			{
				ConstrMapItr.Value->SetAngularOrientationTarget(FQuat(FRotator(0.f, 0.f, Goal * 100.f)));
			}
		}
		else if (!bGraspHeld)
		{
			AHand::MaintainFingerPositions();
		}
}

//
void AHand::UpdateGrasp2(const float Alpha)
{
	GraspPtr->UpdateGrasp(Alpha, this);
}

// Switch the grasp pose
void AHand::SwitchGrasp()
{
	//TODO: Dynamically Switching, not hardcoded

	//IHandOrientationReadable* HandOrientationReadable = Cast<IHandOrientationReadable>(HandOrientationParser);
	if (GraspPtr.IsValid())
	{
		GraspPtr->SwitchGrasp(this);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Grasp shared pointer is not valid!"));
	}
}

// Fixation grasp via attachment of the object to the hand
bool AHand::TryOneHandFixationGrasp()
{
	// If no current grasp is active and there is at least one graspable object
	if ((!OneHandGraspedObject) && (OneHandGraspableObjects.Num() > 0))
	{
		// Get the object to be grasped from the pool of objects
		OneHandGraspedObject = OneHandGraspableObjects.Pop();
	
		// TODO bug report, overlaps flicker when object is attached to hand, this prevents moving objects frome one hand to another
		//if (OneHandGraspedObject->GetAttachParentActor() && OneHandGraspedObject->GetAttachParentActor()->IsA(AHand::StaticClass()))
		//{
		//	// Detach from other hand
		//	AHand* OtherHand = Cast<AHand>(OneHandGraspedObject->GetAttachParentActor());
		//	OtherHand->TryDetachFixationGrasp();
		//}

		// Disable physics on the object and attach it to the hand
		OneHandGraspedObject->GetStaticMeshComponent()->SetSimulatePhysics(false);
		/*OneHandGraspedObject->AttachToComponent(GetRootComponent(), FAttachmentTransformRules(
			EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, true));*/
		OneHandGraspedObject->AttachToActor(this, FAttachmentTransformRules(
			EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, true));
		// Disable overlap checks for the fixation grasp area during active grasping
		FixationGraspArea->bGenerateOverlapEvents = false;
		// Successful grasp
		return true;
	}
	return false;
}

// Fixation grasp of two hands attachment
bool AHand::TryTwoHandsFixationGrasp()
{
	//SetActorTickEnabled(true);

	if (TwoHandsGraspableObject)
	{
		UE_LOG(LogTemp, Error, TEXT(" ** TEST ** AHand:TwoHandsGraspableObject: %s (from this: %s)"),
			*TwoHandsGraspableObject->GetName(), *GetName());
	}
	if (OtherHand->GetTwoHandsGraspableObject())
	{
		UE_LOG(LogTemp, Error, TEXT(" ** TEST ** AHand:TwoHandsGraspableObject: %s (from other: %s)"),
			*OtherHand->GetTwoHandsGraspableObject()->GetName(), *OtherHand->GetName());
	}
	if (TwoHandsGraspableObject == OtherHand->GetTwoHandsGraspableObject())
	{
		UE_LOG(LogTemp, Error, TEXT(" ** TEST ** AHand: EQUAL"));
	}

	// Check if the other hand can also grasp the same object
	if (OtherHand && 
		TwoHandsGraspableObject &&
		TwoHandsGraspableObject == OtherHand->GetTwoHandsGraspableObject())
	{

		//UE_LOG(LogTemp, Warning, TEXT("AHand:TwoHandsGraspableObject: %s (from this: %s)  TwoHandsGraspableObject: %s (from other: %s)"),
		//	*TwoHandsGraspedObject->GetName(), *GetName(), *OtherHand->GetTwoHandsGraspableObject()->GetName(), *OtherHand->GetName());

		// Set the grasped object, and clear the graspable one
		TwoHandsGraspedObject = TwoHandsGraspableObject;
		TwoHandsGraspableObject = nullptr;

		// Disable physics on the object and attach it to the hand
		TwoHandsGraspedObject->GetStaticMeshComponent()->SetSimulatePhysics(false);
		//TwoHandsGraspedObject->AttachToComponent(GetRootComponent(), FAttachmentTransformRules(
		//	EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, true));
		TwoHandsGraspedObject->AttachToActor(this, FAttachmentTransformRules(
			EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, true));
		UE_LOG(LogTemp, Warning, TEXT("AHand: TwoHand Attached %s from %s"), *TwoHandsGraspedObject->GetName(), *GetName());


		// Disable overlaps of the fixation grasp area during the active grasp
		FixationGraspArea->bGenerateOverlapEvents = false;

		// Set other hands grasp as well
		UE_LOG(LogTemp, Error, TEXT(" ** TEST ** next step is to call AHand::TwoHandsFixationGraspFromOther() -- %s"), *GetName());
		OtherHand->TwoHandsFixationGraspFromOther();

		return true;
	}
	return false;
}

// Fixation grasp of two hands attachment (triggered by other hand)
bool AHand::TwoHandsFixationGraspFromOther()
{
	UE_LOG(LogTemp, Error, TEXT(" ** TEST ** calling AHand::TwoHandsFixationGraspFromOther() in %s"), *GetName());
	//SetActorTickEnabled(true);
	if (true/*TwoHandsGraspableObject*/)
	{
		// Set the grasped object, and clear the graspable one
		TwoHandsGraspedObject = TwoHandsGraspableObject;
		TwoHandsGraspableObject = nullptr;

		// Disable physics on the object and attach it to the hand
		//TwoHandsGraspedObject->AttachToComponent(GetRootComponent(), FAttachmentTransformRules(
		//	EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, true));
		AttachToActor(OtherHand, FAttachmentTransformRules(
			EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, true));
		UE_LOG(LogTemp, Error, TEXT("AHand: TwoHand(OTHER) Attached %s from %s"), *GetName(), *OtherHand->GetName());


		// Disable overlaps of the fixation grasp area during the active grasp
		FixationGraspArea->bGenerateOverlapEvents = false;

		return true;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Other NOT SET TwoHandsGraspableObject  %s"), *GetName());
	}
	return false;
}

// Detach fixation grasp from hand(s)
bool AHand::DetachFixationGrasp()
{
	if (OneHandGraspedObject)
	{
		// Detach object from hand
		OneHandGraspedObject->GetStaticMeshComponent()->DetachFromComponent(FDetachmentTransformRules(
			EDetachmentRule::KeepWorld, EDetachmentRule::KeepWorld, EDetachmentRule::KeepWorld, true));
		UE_LOG(LogTemp, Warning, TEXT("AHand: Detached %s from %s"), *OneHandGraspedObject->GetName(), *GetName());

		// Enable physics with and apply current hand velocity, clear pointer to object
		OneHandGraspedObject->GetStaticMeshComponent()->SetSimulatePhysics(true);
		OneHandGraspedObject->GetStaticMeshComponent()->SetPhysicsLinearVelocity(GetVelocity());
		OneHandGraspedObject = nullptr;
		bGraspHeld = false;

		// Re-enable overlaps for the fixation grasp area
		FixationGraspArea->bGenerateOverlapEvents = true;
		return true;
	}
	else if (TwoHandsGraspedObject && OtherHand)
	{
		//SetActorTickEnabled(false);
		// Detach object from hand
		TwoHandsGraspedObject->GetStaticMeshComponent()->DetachFromComponent(FDetachmentTransformRules(
			EDetachmentRule::KeepWorld, EDetachmentRule::KeepWorld, EDetachmentRule::KeepWorld, true));
		UE_LOG(LogTemp, Warning, TEXT("AHand: TwoHandDetachment %s from %s"), *TwoHandsGraspedObject->GetName(), *GetName());

		// Enable physics with and apply current hand velocity, clear pointer to object
		TwoHandsGraspedObject->GetStaticMeshComponent()->SetSimulatePhysics(true);
		TwoHandsGraspedObject->GetStaticMeshComponent()->SetPhysicsLinearVelocity(GetVelocity());
		TwoHandsGraspedObject = nullptr;

		// Trigger detachment on other hand as well
		OtherHand->DetachTwoHandFixationGraspFromOther();

		// Re-enable overlaps for the fixation grasp area
		FixationGraspArea->bGenerateOverlapEvents = true;
		return true;
	}
	return false;
}

// Detach fixation grasp from hand (triggered by the other hand)
bool AHand::DetachTwoHandFixationGraspFromOther()
{
	if (TwoHandsGraspedObject)
	{
		// Detach object from hand
		TwoHandsGraspedObject->GetStaticMeshComponent()->DetachFromComponent(FDetachmentTransformRules(
			EDetachmentRule::KeepWorld, EDetachmentRule::KeepWorld, EDetachmentRule::KeepWorld, true));
		UE_LOG(LogTemp, Warning, TEXT("AHand:(OTHER) TwoHandDetachment %s from %s"), *TwoHandsGraspedObject->GetName(), *GetName());
		// Clear pointer
		TwoHandsGraspedObject = nullptr;

		// Re-enable overlaps for the fixation grasp area
		FixationGraspArea->bGenerateOverlapEvents = true;
		return true;
	}
	return false;
}

// Check if the two hand grasp is still valid (the hands have not moved away from each other)
bool AHand::IsTwoHandGraspStillValid()
{
	return true;
}

// Set pointer to other hand, used for two hand fixation grasp
void AHand::SetOtherHand(AHand* InOtherHand)
{
	OtherHand = InOtherHand;
	UE_LOG(LogTemp, Error, TEXT("AHand: %s has pointer to other hand: %s!"), *GetName(), *OtherHand->GetName());

}

// Check how the object graspable
uint8 AHand::CheckObjectGraspableType(AActor* InActor)
{
	// Check if the static mesh actor can be grasped
	AStaticMeshActor* const SMActor = Cast<AStaticMeshActor>(InActor);
	if (SMActor)
	{
		// Get the static mesh component
		UStaticMeshComponent* const SMComp = SMActor->GetStaticMeshComponent();
		if (SMComp)
		{
			// Check that actor is movable, has a static mesh component, has physics on,
			// and has one hand graspable dimensions
			if (SMActor->IsRootComponentMovable() &&
				SMComp->IsSimulatingPhysics() &&
				SMComp->GetMass() < OneHandFixationMaximumMass &&
				SMActor->GetComponentsBoundingBox().GetSize().Size() < OneHandFixationMaximumLength)
			{
				return ONE_HAND_GRASPABLE;
			}
			// check for two hand graspable dimensions
			else if(SMComp->GetMass() < TwoHandsFixationMaximumMass &&
				SMActor->GetComponentsBoundingBox().GetSize().Size() < TwoHandsFixationMaximumLength)
			{
				return TWO_HANDS_GRASPABLE;
			}
		}
	}
	// Actor cannot be attached
	return NOT_GRASPABLE;
}

// Hold grasp in the current position
void AHand::MaintainFingerPositions()
{
	//for (const auto& ConstrMapItr : Thumb.FingerPartToConstraint)
	//{
	//	ConstrMapItr.Value->SetAngularOrientationTarget(FQuat(FRotator(
	//		ConstrMapItr.Value->GetCurrentSwing2(),
	//		ConstrMapItr.Value->GetCurrentSwing1(),
	//		ConstrMapItr.Value->GetCurrentTwist())));
	//}
	//for (const auto& ConstrMapItr : Index.FingerPartToConstraint)
	//{
	//	ConstrMapItr.Value->SetAngularOrientationTarget(FQuat(FRotator(
	//		ConstrMapItr.Value->GetCurrentSwing2(),
	//		ConstrMapItr.Value->GetCurrentSwing1(),
	//		ConstrMapItr.Value->GetCurrentTwist())));
	//}
	//for (const auto& ConstrMapItr : Middle.FingerPartToConstraint)
	//{
	//	ConstrMapItr.Value->SetAngularOrientationTarget(FQuat(FRotator(
	//		ConstrMapItr.Value->GetCurrentSwing2(),
	//		ConstrMapItr.Value->GetCurrentSwing1(),
	//		ConstrMapItr.Value->GetCurrentTwist())));
	//}
	//for (const auto& ConstrMapItr : Ring.FingerPartToConstraint)
	//{
	//	ConstrMapItr.Value->SetAngularOrientationTarget(FQuat(FRotator(
	//		ConstrMapItr.Value->GetCurrentSwing2(),
	//		ConstrMapItr.Value->GetCurrentSwing1(),
	//		ConstrMapItr.Value->GetCurrentTwist())));
	//}
	//for (const auto& ConstrMapItr : Pinky.FingerPartToConstraint)
	//{
	//	ConstrMapItr.Value->SetAngularOrientationTarget(FQuat(FRotator(
	//		ConstrMapItr.Value->GetCurrentSwing2(),
	//		ConstrMapItr.Value->GetCurrentSwing1(),
	//		ConstrMapItr.Value->GetCurrentTwist())));
	//}

	bGraspHeld = true;
}

// Setup hand default values
void AHand::SetupHandDefaultValues(EHandType InHandType)
{
	if (InHandType == EHandType::Left)
	{
		Thumb.FingerType = EFingerType::Thumb;
		Thumb.FingerPartToBoneName.Add(EFingerPart::Proximal, "thumb_01_l");
		Thumb.FingerPartToBoneName.Add(EFingerPart::Intermediate, "thumb_02_l");
		Thumb.FingerPartToBoneName.Add(EFingerPart::Distal, "thumb_03_l");

		Index.FingerType = EFingerType::Index;
		Index.FingerPartToBoneName.Add(EFingerPart::Proximal, "index_01_l");
		Index.FingerPartToBoneName.Add(EFingerPart::Intermediate, "index_02_l");
		Index.FingerPartToBoneName.Add(EFingerPart::Distal, "index_03_l");

		Middle.FingerType = EFingerType::Middle;
		Middle.FingerPartToBoneName.Add(EFingerPart::Proximal, "middle_01_l");
		Middle.FingerPartToBoneName.Add(EFingerPart::Intermediate, "middle_02_l");
		Middle.FingerPartToBoneName.Add(EFingerPart::Distal, "middle_03_l");

		Ring.FingerType = EFingerType::Ring;
		Ring.FingerPartToBoneName.Add(EFingerPart::Proximal, "ring_01_l");
		Ring.FingerPartToBoneName.Add(EFingerPart::Intermediate, "ring_02_l");
		Ring.FingerPartToBoneName.Add(EFingerPart::Distal, "ring_03_l");

		Pinky.FingerType = EFingerType::Pinky;
		Pinky.FingerPartToBoneName.Add(EFingerPart::Proximal, "pinky_01_l");
		Pinky.FingerPartToBoneName.Add(EFingerPart::Intermediate, "pinky_02_l");
		Pinky.FingerPartToBoneName.Add(EFingerPart::Distal, "pinky_03_l");
	}
	else if (InHandType == EHandType::Right)
	{
		Thumb.FingerType = EFingerType::Thumb;
		Thumb.FingerPartToBoneName.Add(EFingerPart::Proximal, "thumb_01_r");
		Thumb.FingerPartToBoneName.Add(EFingerPart::Intermediate, "thumb_02_r");
		Thumb.FingerPartToBoneName.Add(EFingerPart::Distal, "thumb_03_r");

		Index.FingerType = EFingerType::Index;
		Index.FingerPartToBoneName.Add(EFingerPart::Proximal, "index_01_r");
		Index.FingerPartToBoneName.Add(EFingerPart::Intermediate, "index_02_r");
		Index.FingerPartToBoneName.Add(EFingerPart::Distal, "index_03_r");

		Middle.FingerType = EFingerType::Middle;
		Middle.FingerPartToBoneName.Add(EFingerPart::Proximal, "middle_01_r");
		Middle.FingerPartToBoneName.Add(EFingerPart::Intermediate, "middle_02_r");
		Middle.FingerPartToBoneName.Add(EFingerPart::Distal, "middle_03_r");

		Ring.FingerType = EFingerType::Ring;
		Ring.FingerPartToBoneName.Add(EFingerPart::Proximal, "ring_01_r");
		Ring.FingerPartToBoneName.Add(EFingerPart::Intermediate, "ring_02_r");
		Ring.FingerPartToBoneName.Add(EFingerPart::Distal, "ring_03_r");

		Pinky.FingerType = EFingerType::Pinky;
		Pinky.FingerPartToBoneName.Add(EFingerPart::Proximal, "pinky_01_r");
		Pinky.FingerPartToBoneName.Add(EFingerPart::Intermediate, "pinky_02_r");
		Pinky.FingerPartToBoneName.Add(EFingerPart::Distal, "pinky_03_r");
	}
}

// Setup skeletal mesh default values
void AHand::SetupSkeletalDefaultValues(USkeletalMeshComponent* InSkeletalMeshComponent)
{
	if (InSkeletalMeshComponent->GetPhysicsAsset())
	{
		// Hand joint velocity drive
		InSkeletalMeshComponent->SetAllMotorsAngularPositionDrive(true, true);

		// Set drive parameters
		InSkeletalMeshComponent->SetAllMotorsAngularDriveParams(Spring, Damping, ForceLimit);

		UE_LOG(LogTemp, Log, TEXT("AHand: SkeletalMeshComponent's angular motors set!"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AHand: SkeletalMeshComponent's has no PhysicsAsset set!"));
	}
}

// Setup fingers angular drive values
void AHand::SetupAngularDriveValues(EAngularDriveMode::Type DriveMode)
{
	USkeletalMeshComponent* const SkelMeshComp = GetSkeletalMeshComponent();
	if (Thumb.SetFingerPartsConstraints(SkelMeshComp->Constraints))
	{
		Thumb.SetFingerDriveMode(DriveMode, Spring, Damping, ForceLimit);
	}
	if (Index.SetFingerPartsConstraints(SkelMeshComp->Constraints))
	{
		Index.SetFingerDriveMode(DriveMode, Spring, Damping, ForceLimit);
	}
	if (Middle.SetFingerPartsConstraints(SkelMeshComp->Constraints))
	{
		Middle.SetFingerDriveMode(DriveMode, Spring, Damping, ForceLimit);
	}
	if (Ring.SetFingerPartsConstraints(SkelMeshComp->Constraints))
	{
		Ring.SetFingerDriveMode(DriveMode, Spring, Damping, ForceLimit);
	}
	if (Pinky.SetFingerPartsConstraints(SkelMeshComp->Constraints))
	{
		Pinky.SetFingerDriveMode(DriveMode, Spring, Damping, ForceLimit);
	}
}
