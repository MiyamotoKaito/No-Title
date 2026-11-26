#include "Mock/Character/MockSlayerSimpleAnimInstance.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequenceBase.h"
#include "AnimationRuntime.h"
#include "Mock/Character/MockBossCharacterBase.h"
#include "Mock/Data/MockBossDefinition.h"

const FName UMockSlayerSimpleAnimInstance::SlotName(TEXT("DefaultSlot"));

/**
 * アニメの計算はワーカースレッドで行われるので、必要な値はプロキシにコピーしてから使う
 */
struct FMockSlayerSimpleAnimInstanceProxy : public FAnimInstanceProxy
{
	explicit FMockSlayerSimpleAnimInstanceProxy(UAnimInstance* InAnimInstance)
		: FAnimInstanceProxy(InAnimInstance)
	{
	}

	virtual void Initialize(UAnimInstance* InAnimInstance) override
	{
		FAnimInstanceProxy::Initialize(InAnimInstance);
		// スロットを登録しておく（モンタージュがこのスロットで再生できるように）
		RegisterSlotNodeWithAnimInstance(UMockSlayerSimpleAnimInstance::SlotName);
	}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override
	{
		FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
		const UMockSlayerSimpleAnimInstance* Instance = CastChecked<UMockSlayerSimpleAnimInstance>(InAnimInstance);
		Idle = Instance->IdleAnimation;
		Walk = Instance->WalkAnimation;
		Run = Instance->RunAnimation;
		WalkSpeed = FMath::Max(Instance->WalkAnimationSpeed, 1.f);
		RunSpeed = FMath::Max(Instance->RunAnimationSpeed, WalkSpeed + 1.f);
		Speed = Instance->Speed;
	}

	virtual void UpdateAnimationNode(const FAnimationUpdateContext& InContext) override
	{
		const float DeltaTime = InContext.GetDeltaTime();

		// 待機はそのまま、歩き・走りは同じ足運びになるよう割合（位相）で進める
		IdleTime = AdvanceLooping(Idle, IdleTime, DeltaTime);
		const float MoveLength = FMath::Lerp(GetLength(Walk), GetLength(Run), GetRunAlpha());
		if (MoveLength > 0.f)
		{
			// 歩きの速さより遅い時はゆっくり足を動かす
			const float Rate = FMath::Clamp(Speed / WalkSpeed, 0.3f, 1.5f);
			MovePhase = FMath::Fmod(MovePhase + DeltaTime * Rate / MoveLength, 1.f);
		}

		// モンタージュのスロットの重み
		GetSlotWeight(UMockSlayerSimpleAnimInstance::SlotName, SlotNodeWeight, SourceWeight, TotalNodeWeight);
		UpdateSlotNodeWeight(UMockSlayerSimpleAnimInstance::SlotName, SlotNodeWeight, 1.f);
	}

	virtual bool Evaluate(FPoseContext& Output) override
	{
		if (SlotNodeWeight <= ZERO_ANIMWEIGHT_THRESH)
		{
			EvaluateLocomotion(Output);
			return true;
		}

		FPoseContext SourcePose(Output);
		EvaluateLocomotion(SourcePose);
		const FAnimationPoseData SourceData(SourcePose);
		FAnimationPoseData OutputData(Output);
		SlotEvaluatePose(UMockSlayerSimpleAnimInstance::SlotName, SourceData, SourceWeight, OutputData, SlotNodeWeight, TotalNodeWeight);
		return true;
	}

private:
	static float GetLength(const UAnimSequenceBase* Animation)
	{
		return Animation ? Animation->GetPlayLength() : 0.f;
	}

	static float AdvanceLooping(const UAnimSequenceBase* Animation, float Time, float DeltaTime)
	{
		const float Length = GetLength(Animation);
		return Length > 0.f ? FMath::Fmod(Time + DeltaTime, Length) : 0.f;
	}

	// 0：歩き、1：走り
	float GetRunAlpha() const
	{
		return FMath::Clamp((Speed - WalkSpeed) / (RunSpeed - WalkSpeed), 0.f, 1.f);
	}

	static void GetPose(const UAnimSequenceBase* Animation, float Time, FPoseContext& Output)
	{
		if (Animation == nullptr)
		{
			Output.ResetToRefPose();
			return;
		}
		FAnimationPoseData PoseData(Output);
		Animation->GetAnimationPose(PoseData, FAnimExtractContext(static_cast<double>(Time), false, {}, true));
	}

	static void Blend(const FPoseContext& A, const FPoseContext& B, float Alpha, FPoseContext& Output)
	{
		const FAnimationPoseData DataA(const_cast<FPoseContext&>(A));
		const FAnimationPoseData DataB(const_cast<FPoseContext&>(B));
		FAnimationPoseData OutputData(Output);
		FAnimationRuntime::BlendTwoPosesTogether(DataA, DataB, 1.f - Alpha, OutputData);
	}

	void EvaluateLocomotion(FPoseContext& Output) const
	{
		const float RunAlpha = GetRunAlpha();

		// 歩きと走りを混ぜた「移動」のポーズ
		FPoseContext MovePose(Output);
		{
			FPoseContext WalkPose(Output);
			GetPose(Walk, MovePhase * GetLength(Walk), WalkPose);
			if (RunAlpha > 0.f && Run != nullptr)
			{
				FPoseContext RunPose(Output);
				GetPose(Run, MovePhase * GetLength(Run), RunPose);
				Blend(WalkPose, RunPose, RunAlpha, MovePose);
			}
			else
			{
				Blend(WalkPose, WalkPose, 0.f, MovePose);
			}
		}

		// 止まっている時は待機と混ぜる
		const float MoveAlpha = FMath::Clamp(Speed / (WalkSpeed * 0.5f), 0.f, 1.f);
		FPoseContext IdlePose(Output);
		GetPose(Idle, IdleTime, IdlePose);
		Blend(IdlePose, MovePose, MoveAlpha, Output);
	}

	TObjectPtr<UAnimSequenceBase> Idle;
	TObjectPtr<UAnimSequenceBase> Walk;
	TObjectPtr<UAnimSequenceBase> Run;
	float WalkSpeed = 200.f;
	float RunSpeed = 600.f;
	float Speed = 0.f;

	float IdleTime = 0.f;
	float MovePhase = 0.f;

	float SlotNodeWeight = 0.f;
	float SourceWeight = 1.f;
	float TotalNodeWeight = 0.f;
};

void UMockSlayerSimpleAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	const AMockBossCharacterBase* Character = Cast<AMockBossCharacterBase>(GetOwningActor());
	const UMockBossDefinition* Definition = Character ? Character->GetDefinition() : nullptr;
	if (Definition == nullptr)
	{
		return;
	}
	IdleAnimation = Definition->IdleAnimation;
	WalkAnimation = Definition->WalkAnimation;
	RunAnimation = Definition->RunAnimation;
	WalkAnimationSpeed = Definition->WalkAnimationSpeed;
	RunAnimationSpeed = Definition->RunAnimationSpeed;
}

void UMockSlayerSimpleAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// 体が大きいほど一歩が大きいので、等倍の時の速さに直して足運びを合わせる
	const AMockBossCharacterBase* Character = Cast<AMockBossCharacterBase>(GetOwningActor());
	if (Character == nullptr)
	{
		Speed = 0.f;
		return;
	}
	Speed = Character->GetVelocity().Size2D() / Character->GetBodyScale();
}

FAnimInstanceProxy* UMockSlayerSimpleAnimInstance::CreateAnimInstanceProxy()
{
	return new FMockSlayerSimpleAnimInstanceProxy(this);
}

void UMockSlayerSimpleAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}
