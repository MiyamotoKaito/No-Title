#include "Mock/Game/MockCrushChunk.h"

#include "AbilitySystemComponent.h"
#include "Mock/AbilitySystem/MockSlayerAttributeSet.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "SlayerGameplayTags.h"
#include "UObject/ConstructorHelpers.h"

AMockCrushChunk::AMockCrushChunk()
{
	PrimaryActorTick.bCanEverTick = false;

	Collision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Collision"));
	Collision->InitCapsuleSize(90.f, 110.f);
	Collision->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	// カメラが塊に押し込まれないようにする
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	RootComponent = Collision;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeRotation(FRotator(35.f, 20.f, 45.f));
	Mesh->SetRelativeScale3D(FVector(1.4f));

	// 仮の見た目：エンジン標準の立方体を傾けて結晶っぽく置く
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UMockSlayerAttributeSet>(TEXT("AttributeSet"));

	bReplicates = true;
}

void AMockCrushChunk::BeginPlay()
{
	Super::BeginPlay();

	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	// ダメージ計算で「塊」と分かるようにする
	AbilitySystemComponent->AddLooseGameplayTag(SlayerTags::Actor_CrushChunk);
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UMockSlayerAttributeSet::GetHealthAttribute()).AddUObject(this, &AMockCrushChunk::OnHealthChanged);

	InitialScale = Mesh->GetRelativeScale3D();
}

UAbilitySystemComponent* AMockCrushChunk::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AMockCrushChunk::InitializeChunk(UMockBossDefinition* InBoss, float InHealth)
{
	Boss = InBoss;
	const float Health = FMath::Max(InHealth, 1.f);
	AbilitySystemComponent->SetNumericAttributeBase(UMockSlayerAttributeSet::GetMaxHealthAttribute(), Health);
	AbilitySystemComponent->SetNumericAttributeBase(UMockSlayerAttributeSet::GetHealthAttribute(), Health);
}

float AMockCrushChunk::GetHealthPercent() const
{
	const float MaxHealth = AttributeSet->GetMaxHealth();
	return MaxHealth > 0.f ? AttributeSet->GetHealth() / MaxHealth : 0.f;
}

void AMockCrushChunk::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	if (bBroken)
	{
		return;
	}

	// 削れるほど小さくなる
	const float Percent = GetHealthPercent();
	Mesh->SetRelativeScale3D(InitialScale * FMath::Lerp(MinScale, 1.f, Percent));

	if (Data.NewValue > 0.f)
	{
		return;
	}

	bBroken = true;
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetVisibility(false);
	OnBroken.Broadcast(this);
}
