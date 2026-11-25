// 倒したボスが変わる「塊」。制限時間内に砕けばそのボスを獲得できる

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"
#include "MockCrushChunk.generated.h"

class UAbilitySystemComponent;
class UMockBossDefinition;
class UCapsuleComponent;
class UMockSlayerAttributeSet;
class UStaticMeshComponent;
class AMockCrushChunk;
struct FOnAttributeChangeData;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnMockCrushChunkBroken, AMockCrushChunk* /*Chunk*/);

/**
 * HP を持つ砕く対象
 * 自分の ASC と AttributeSet を持つので、キャラの技（ダメージ GE）がそのまま当たる
 * ダメージ計算は Actor.CrushChunk タグを見て、攻撃側の CrushPower を使う
 * 制限時間は AMockBattleGameMode 側で管理する
 */
UCLASS()
class SLAYER_API AMockCrushChunk : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AMockCrushChunk();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// Spawn 後に呼ぶ：どのボスの塊か、HP はいくつか
	void InitializeChunk(UMockBossDefinition* InBoss, float InHealth);

	UMockBossDefinition* GetBoss() const { return Boss; }
	bool IsBroken() const { return bBroken; }
	float GetHealthPercent() const;

	FOnMockCrushChunkBroken OnBroken;

protected:
	virtual void BeginPlay() override;

	// 攻撃の当たり判定（Pawn として判定に入る）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chunk")
	TObjectPtr<UCapsuleComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chunk")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chunk")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chunk")
	TObjectPtr<UMockSlayerAttributeSet> AttributeSet;

	UPROPERTY(Transient)
	TObjectPtr<UMockBossDefinition> Boss;

	// HP が減るほど小さくなる（最小の大きさ）
	UPROPERTY(EditDefaultsOnly, Category = "Chunk")
	float MinScale = 0.4f;

private:
	void OnHealthChanged(const FOnAttributeChangeData& Data);

	FVector InitialScale = FVector::OneVector;
	bool bBroken = false;
};
