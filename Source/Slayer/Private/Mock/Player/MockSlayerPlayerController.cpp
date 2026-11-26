#include "Mock/Player/MockSlayerPlayerController.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Mock/AbilitySystem/MockSlayerAttributeSet.h"
#include "Mock/Data/MockBossDefinition.h"
#include "TimerManager.h"
#include "Cameras/ComposableCameraCameraBase.h"
#include "Mock/Character/MockBossCharacterBase.h"
#include "Core/ComposableCameraParameterBlock.h"
#include "Core/ComposableCameraPlayerCameraManager.h"
#include "DataAssets/ComposableCameraTransitionDataAsset.h"
#include "DataAssets/ComposableCameraTypeAsset.h"
#include "Mock/Game/MockCrushChunk.h"
#include "UObject/ConstructorHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Mock/Game/MockBattleGameMode.h"
#include "Mock/Game/MockBattleGameState.h"
#include "Mock/Game/MockSlayerSubsystems.h"
#include "Engine/GameInstance.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "SlayerGameplayTags.h"

namespace
{
	// 開発用：コンソールで変更できる
	TAutoConsoleVariable<int32> CVarSlayerAutoPlay(TEXT("slayer.AutoPlay"), 0, TEXT("1 でプレイヤー側も自動で戦う（ループの動作確認用）"));
	TAutoConsoleVariable<int32> CVarSlayerGod(TEXT("slayer.God"), 0, TEXT("1 でプレイヤーを無敵にする"));
	TAutoConsoleVariable<int32> CVarSlayerChangeFull(TEXT("slayer.ChangeFull"), 0, TEXT("1 で変身ゲージを常に満タンにする"));

	UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType ValueType)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, Name);
		Action->ValueType = ValueType;
		return Action;
	}

	// WASD を2D移動にするためのモディファイア
	void AddSwizzle(UObject* Outer, FEnhancedActionKeyMapping& Mapping)
	{
		Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(Outer));
	}

	void AddNegate(UObject* Outer, FEnhancedActionKeyMapping& Mapping, bool bX = true, bool bY = true)
	{
		UInputModifierNegate* Negate = NewObject<UInputModifierNegate>(Outer);
		Negate->bX = bX;
		Negate->bY = bY;
		Negate->bZ = false;
		Mapping.Modifiers.Add(Negate);
	}
}

AMockSlayerPlayerController::AMockSlayerPlayerController()
{
	bShowMouseCursor = false;

	// カメラは Composable Camera System に任せる
	PlayerCameraManagerClass = AComposableCameraPlayerCameraManager::StaticClass();

	// プロジェクト用に複製して距離などを調整したカメラ定義（Tools/Python/build_alpha_assets.py が作る）
	static ConstructorHelpers::FObjectFinder<UComposableCameraTypeAsset> FreeCameraAsset(TEXT("/Game/Slayer/Camera/CAM_MockSlayer_Free.CAM_MockSlayer_Free"));
	static ConstructorHelpers::FObjectFinder<UComposableCameraTypeAsset> LockOnCameraAsset(TEXT("/Game/Slayer/Camera/CAM_MockSlayer_LockOn.CAM_MockSlayer_LockOn"));
	static ConstructorHelpers::FObjectFinder<UComposableCameraTransitionDataAsset> SwitchTransitionAsset(TEXT("/ComposableCameraSystem/CameraPresets/Preset_Transitions/CameraTransaition_Cylindrical_OneSecond.CameraTransaition_Cylindrical_OneSecond"));
	FreeCamera = FreeCameraAsset.Object;
	LockOnCamera = LockOnCameraAsset.Object;

	// 体の大きいキャラ用（引きを大きくしたもの）
	static ConstructorHelpers::FObjectFinder<UComposableCameraTypeAsset> FreeCameraLargeAsset(TEXT("/Game/Slayer/Camera/CAM_MockSlayer_FreeLarge.CAM_MockSlayer_FreeLarge"));
	static ConstructorHelpers::FObjectFinder<UComposableCameraTypeAsset> LockOnCameraLargeAsset(TEXT("/Game/Slayer/Camera/CAM_MockSlayer_LockOnLarge.CAM_MockSlayer_LockOnLarge"));
	FreeCameraLarge = FreeCameraLargeAsset.Object;
	LockOnCameraLarge = LockOnCameraLargeAsset.Object;
	TargetSwitchTransition = SwitchTransitionAsset.Object;

	// 視点操作の入力アクション。カメラ（ControlRotate ノード）もこれを読むので、アセットで共有する
	static ConstructorHelpers::FObjectFinder<UInputAction> LookActionAsset(TEXT("/Game/Slayer/Input/IA_MockSlayer_Look.IA_MockSlayer_Look"));
	LookActionTemplate = LookActionAsset.Object;
}

void AMockSlayerPlayerController::OnToggleLockOn()
{
	if (!IsGameplayInputAllowed())
	{
		return;
	}
	bLockOnEnabled = !bLockOnEnabled;
}

void AMockSlayerPlayerController::UpdateCamera()
{
	// 獲得・進化の演出とリザルトでは、GameMode が用意した演出カメラを使う
	const AMockBattleGameState* State = GetWorld()->GetGameState<AMockBattleGameState>();
	if (State != nullptr)
	{
		const EMockBattlePhase Phase = State->GetPhase();
		if (Phase == EMockBattlePhase::Capture || Phase == EMockBattlePhase::Evolution || Phase == EMockBattlePhase::Result)
		{
			return;
		}
	}

	// 狙う相手：ロックオン中だけ。砕くフェーズは塊、それ以外は生きているボス
	// （ロックオンしていない時は自由カメラで、マウスで視点を回せる）
	AActor* Target = nullptr;
	if (State != nullptr && bLockOnEnabled)
	{
		if (State->Chunk != nullptr && !State->Chunk->IsBroken())
		{
			Target = State->Chunk;
		}
		else if (State->Boss != nullptr && !State->Boss->IsDead())
		{
			Target = State->Boss;
		}
	}

	if (bCameraActivated && CameraTarget.Get() == Target)
	{
		return;
	}
	ActivateCameraFor(Target);
}

void AMockSlayerPlayerController::ActivateCameraFor(AActor* Target)
{
	AComposableCameraPlayerCameraManager* CameraManager = Cast<AComposableCameraPlayerCameraManager>(PlayerCameraManager);
	if (CameraManager == nullptr || GetPawn() == nullptr)
	{
		return;
	}

	// 体の大きいキャラを操作している時は、引きの大きいカメラにする
	const AMockBossCharacterBase* SlayerPawn = GetSlayerPawn();
	const bool bLarge = SlayerPawn != nullptr && SlayerPawn->GetBodyScale() >= LargeBodyScale;
	UComposableCameraTypeAsset* Free = (bLarge && FreeCameraLarge != nullptr) ? FreeCameraLarge.Get() : FreeCamera.Get();
	UComposableCameraTypeAsset* LockOn = (bLarge && LockOnCameraLarge != nullptr) ? LockOnCameraLarge.Get() : LockOnCamera.Get();

	// 最初は必ず自由カメラから始める
	// （ロックオンカメラは距離を持たず、直前のカメラの位置を引き継ぐ作りなので）
	if (!bCameraActivated && Free != nullptr)
	{
		CameraManager->ActivateNewCameraFromTypeAsset(Free, nullptr, FComposableCameraActivateParams(), FComposableCameraParameterBlock());
		bCameraActivated = true;
		CameraTarget = nullptr;
		// ロックオンは次のフレームで（自由カメラの位置が一度計算されてから引き継ぐ）
		return;
	}

	UComposableCameraTypeAsset* CameraAsset = (Target != nullptr && LockOn != nullptr) ? LockOn : Free;
	if (CameraAsset == nullptr)
	{
		return;
	}

	FComposableCameraParameterBlock Parameters;
	if (Target != nullptr)
	{
		Parameters.SetActor(LockOnTargetParameter, Target);
	}

	CameraManager->ActivateNewCameraFromTypeAsset(CameraAsset, TargetSwitchTransition, FComposableCameraActivateParams(), Parameters);
	CameraTarget = Target;
	bCameraActivated = true;
}

void AMockSlayerPlayerController::BeginPlay()
{
	Super::BeginPlay();

	SetInputMode(FInputModeGameOnly());
	AddMappingContext();
}

void AMockSlayerPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// キャラを乗り換えた時に IMC を入れ直す（キャラ別の IMC を作った時はここで差し替える）
	AddMappingContext();

	// カメラは新しいポーンで付け直す（体の大きさでカメラが変わるため）
	bCameraActivated = false;
	CameraTarget = nullptr;
	// 無敵（開発用）も新しいポーンに付け直す
	bGodApplied = false;

	// 与えたダメージで変身ゲージを溜める
	if (AMockBossCharacterBase* PreviousPawn = DamageBoundPawn.Get())
	{
		PreviousPawn->OnDamageDealt.Remove(DamageDealtHandle);
	}
	DamageBoundPawn = Cast<AMockBossCharacterBase>(InPawn);
	if (AMockBossCharacterBase* NewPawn = DamageBoundPawn.Get())
	{
		DamageDealtHandle = NewPawn->OnDamageDealt.AddUObject(this, &AMockSlayerPlayerController::OnPawnDealtDamage);
	}
}

void AMockSlayerPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	UpdateBossChange(DeltaTime);
	UpdateCamera();
	UpdateDebugCheats(DeltaTime);

	// リザルトに入った時、最初の選択を決める（獲得したボスがいればそれ、いなければ今のキャラ）
	if (bSelectionInitialized || !IsInResult())
	{
		return;
	}
	const AMockBattleGameMode* GameMode = GetWorld()->GetAuthGameMode<AMockBattleGameMode>();
	if (GameMode == nullptr)
	{
		return;
	}

	const TArray<UMockBossDefinition*> Characters = GameMode->GetSelectableCharacters();
	const AMockBossCharacterBase* SlayerPawn = GetSlayerPawn();

	// 今の操作キャラを選んだ状態から始める
	UMockBossDefinition* Preferred = SlayerPawn ? SlayerPawn->GetDefinition() : nullptr;

	SelectedCharacterIndex = FMath::Max(Characters.IndexOfByKey(Preferred), 0);
	bSelectionInitialized = true;
}

void AMockSlayerPlayerController::UpdateDebugCheats(float DeltaTime)
{
	AMockBossCharacterBase* SlayerPawn = GetSlayerPawn();
	UAbilitySystemComponent* ASC = SlayerPawn ? SlayerPawn->GetAbilitySystemComponent() : nullptr;

	// slayer.God：プレイヤーを無敵にする
	if (ASC != nullptr)
	{
		const bool bGod = CVarSlayerGod.GetValueOnGameThread() > 0;
		if (bGod && !bGodApplied)
		{
			ASC->AddLooseGameplayTag(SlayerTags::State_Invulnerable);
			bGodApplied = true;
		}
		else if (!bGod && bGodApplied)
		{
			ASC->RemoveLooseGameplayTag(SlayerTags::State_Invulnerable);
			bGodApplied = false;
		}
	}

	// slayer.AutoPlay：プレイヤー側も自動で戦う（ループ全体の動作確認用）
	if (CVarSlayerAutoPlay.GetValueOnGameThread() <= 0)
	{
		return;
	}

	AutoPlayTimer -= DeltaTime;

	// リザルトでは少し待ってから次のバトルへ
	if (IsInResult())
	{
		if (AutoPlayTimer <= -3.f)
		{
			AutoPlayTimer = 0.f;
			OnConfirm();
		}
		return;
	}

	if (SlayerPawn == nullptr || SlayerPawn->IsDead() || !IsGameplayInputAllowed())
	{
		AutoPlayTimer = 0.f;
		return;
	}

	const AActor* Target = SlayerPawn->FindCombatTarget(10000.f);
	if (Target == nullptr)
	{
		return;
	}

	const float Distance = FVector::Dist2D(SlayerPawn->GetActorLocation(), Target->GetActorLocation());
	if (Distance > 230.f && !SlayerPawn->IsAttacking())
	{
		FVector Direction = Target->GetActorLocation() - SlayerPawn->GetActorLocation();
		Direction.Z = 0.f;
		SlayerPawn->AddMovementInput(Direction.GetSafeNormal(), 1.f);
		return;
	}

	// 近ければスキル → 通常攻撃の順で押す
	if (AutoPlayTimer > 0.f)
	{
		return;
	}
	AutoPlayTimer = 0.3f;
	// 変身できるなら変身する（どれになるかはランダム）
	if (IsChangeReady() && !IsBossChanged())
	{
		const int32 Count = GetChangeCandidates().Num();
		if (Count > 0)
		{
			TryBossChange(FMath::RandRange(0, Count - 1));
			return;
		}
	}
	// スキル・必殺技はコンボの途中でも押す（コンボを中断して出る）
	if (SlayerPawn->PressAbilityInput(SlayerTags::Input_Skill2) || SlayerPawn->PressAbilityInput(SlayerTags::Input_Skill1))
	{
		return;
	}
	SlayerPawn->PressAbilityInput(SlayerTags::Input_Attack);
	SlayerPawn->ReleaseAbilityInput(SlayerTags::Input_Attack);
}

void AMockSlayerPlayerController::CreateDefaultInput()
{
	if (MappingContext != nullptr)
	{
		return;
	}

	if (DefaultMappingContext != nullptr)
	{
		MappingContext = DefaultMappingContext;
	}
	else
	{
		MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_SlayerDefault"));
	}

	MoveAction = MakeAction(this, TEXT("IA_Move"), EInputActionValueType::Axis2D);
	// 視点操作はカメラ（ControlRotate ノード）が読むので、同じアセットを使う（無ければ作る）
	LookAction = LookActionTemplate != nullptr ? LookActionTemplate.Get() : MakeAction(this, TEXT("IA_Look"), EInputActionValueType::Axis2D);
	LockOnAction = MakeAction(this, TEXT("IA_LockOn"), EInputActionValueType::Boolean);
	ChangeAction = MakeAction(this, TEXT("IA_BossChange"), EInputActionValueType::Boolean);
	ConfirmAction = MakeAction(this, TEXT("IA_Confirm"), EInputActionValueType::Boolean);
	SelectPrevAction = MakeAction(this, TEXT("IA_SelectPrev"), EInputActionValueType::Boolean);
	SelectNextAction = MakeAction(this, TEXT("IA_SelectNext"), EInputActionValueType::Boolean);

	AbilityActions.Add(SlayerTags::Input_Attack, MakeAction(this, TEXT("IA_Attack"), EInputActionValueType::Boolean));
	AbilityActions.Add(SlayerTags::Input_Skill1, MakeAction(this, TEXT("IA_Skill1"), EInputActionValueType::Boolean));
	AbilityActions.Add(SlayerTags::Input_Skill2, MakeAction(this, TEXT("IA_Skill2"), EInputActionValueType::Boolean));
	AbilityActions.Add(SlayerTags::Input_Dodge, MakeAction(this, TEXT("IA_Dodge"), EInputActionValueType::Boolean));

	const TCHAR* ChooseNames[] = { TEXT("IA_Choose1"), TEXT("IA_Choose2"), TEXT("IA_Choose3"), TEXT("IA_Choose4") };
	for (const TCHAR* Name : ChooseNames)
	{
		ChooseActions.Add(MakeAction(this, Name, EInputActionValueType::Boolean));
	}

	// アセットの IMC を使う時は、キー割り当てもアセット側に任せる
	if (DefaultMappingContext != nullptr)
	{
		return;
	}

	UInputMappingContext* IMC = MappingContext;

	// 移動：W/S を Y、A/D を X に
	AddSwizzle(IMC, IMC->MapKey(MoveAction, EKeys::W));
	{
		FEnhancedActionKeyMapping& Mapping = IMC->MapKey(MoveAction, EKeys::S);
		AddSwizzle(IMC, Mapping);
		AddNegate(IMC, Mapping);
	}
	AddNegate(IMC, IMC->MapKey(MoveAction, EKeys::A));
	IMC->MapKey(MoveAction, EKeys::D);
	IMC->MapKey(MoveAction, EKeys::Gamepad_Left2D);

	// カメラ：マウスは上で上を向くように Y を反転
	AddNegate(IMC, IMC->MapKey(LookAction, EKeys::Mouse2D), false, true);
	IMC->MapKey(LookAction, EKeys::Gamepad_Right2D);

	// ロックオンの切り替え
	IMC->MapKey(LockOnAction, EKeys::Tab);
	IMC->MapKey(LockOnAction, EKeys::MiddleMouseButton);
	IMC->MapKey(LockOnAction, EKeys::Gamepad_RightThumbstick);

	// ボスチェンジ（数字キーは変身先の指定。リザルトではキャラ選択）
	IMC->MapKey(ChangeAction, EKeys::F);
	IMC->MapKey(ChangeAction, EKeys::Gamepad_LeftShoulder);

	// 技
	IMC->MapKey(AbilityActions[SlayerTags::Input_Attack], EKeys::LeftMouseButton);
	IMC->MapKey(AbilityActions[SlayerTags::Input_Attack], EKeys::Gamepad_FaceButton_Left);
	IMC->MapKey(AbilityActions[SlayerTags::Input_Skill1], EKeys::RightMouseButton);
	IMC->MapKey(AbilityActions[SlayerTags::Input_Skill1], EKeys::Gamepad_FaceButton_Top);
	IMC->MapKey(AbilityActions[SlayerTags::Input_Skill2], EKeys::Q);
	IMC->MapKey(AbilityActions[SlayerTags::Input_Skill2], EKeys::Gamepad_RightShoulder);
	IMC->MapKey(AbilityActions[SlayerTags::Input_Dodge], EKeys::SpaceBar);
	IMC->MapKey(AbilityActions[SlayerTags::Input_Dodge], EKeys::Gamepad_FaceButton_Bottom);

	// リザルト画面
	IMC->MapKey(ConfirmAction, EKeys::Enter);
	IMC->MapKey(ConfirmAction, EKeys::Gamepad_Special_Right);
	IMC->MapKey(SelectPrevAction, EKeys::Left);
	IMC->MapKey(SelectPrevAction, EKeys::Gamepad_DPad_Left);
	IMC->MapKey(SelectNextAction, EKeys::Right);
	IMC->MapKey(SelectNextAction, EKeys::Gamepad_DPad_Right);
	const FKey NumberKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four };
	for (int32 Index = 0; Index < ChooseActions.Num(); ++Index)
	{
		IMC->MapKey(ChooseActions[Index], NumberKeys[Index]);
	}
}

void AMockSlayerPlayerController::AddMappingContext()
{
	CreateDefaultInput();

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (Subsystem == nullptr || MappingContext == nullptr)
	{
		return;
	}
	Subsystem->RemoveMappingContext(MappingContext);
	Subsystem->AddMappingContext(MappingContext, 0);
}

void AMockSlayerPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	CreateDefaultInput();

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	if (Input == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[Input] EnhancedInputComponent ではありません。Project Settings > Input の Default Input Component Class を確認してください"));
		return;
	}

	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMockSlayerPlayerController::OnMove);
	// 視点操作はカメラ（Composable Camera の ControlRotate ノード）がポーン側で直接読むので、ここでは束ねない
	Input->BindAction(LockOnAction, ETriggerEvent::Started, this, &AMockSlayerPlayerController::OnToggleLockOn);
	Input->BindAction(ChangeAction, ETriggerEvent::Started, this, &AMockSlayerPlayerController::OnChangePressed);

	for (const TPair<FGameplayTag, TObjectPtr<UInputAction>>& Pair : AbilityActions)
	{
		Input->BindAction(Pair.Value, ETriggerEvent::Started, this, &AMockSlayerPlayerController::OnAbilityPressed, Pair.Key);
		Input->BindAction(Pair.Value, ETriggerEvent::Completed, this, &AMockSlayerPlayerController::OnAbilityReleased, Pair.Key);
	}

	Input->BindAction(ConfirmAction, ETriggerEvent::Started, this, &AMockSlayerPlayerController::OnConfirm);
	Input->BindAction(SelectPrevAction, ETriggerEvent::Started, this, &AMockSlayerPlayerController::OnSelectStep, -1);
	Input->BindAction(SelectNextAction, ETriggerEvent::Started, this, &AMockSlayerPlayerController::OnSelectStep, 1);
	for (int32 Index = 0; Index < ChooseActions.Num(); ++Index)
	{
		Input->BindAction(ChooseActions[Index], ETriggerEvent::Started, this, &AMockSlayerPlayerController::OnChooseCharacter, Index);
	}
}

AMockBossCharacterBase* AMockSlayerPlayerController::GetSlayerPawn() const
{
	return Cast<AMockBossCharacterBase>(GetPawn());
}

bool AMockSlayerPlayerController::IsGameplayInputAllowed() const
{
	const AMockBattleGameState* State = GetWorld()->GetGameState<AMockBattleGameState>();
	// GameState が無いテスト用レベルでは常に操作できる
	return State == nullptr || State->IsPlayerInputAllowed();
}

void AMockSlayerPlayerController::OnMove(const FInputActionValue& Value)
{
	AMockBossCharacterBase* SlayerPawn = GetSlayerPawn();
	if (SlayerPawn == nullptr || !IsGameplayInputAllowed())
	{
		return;
	}

	// 画面に映っているカメラの向きを基準に動く
	const FVector2D Axis = Value.Get<FVector2D>();
	const float CameraYaw = PlayerCameraManager != nullptr ? PlayerCameraManager->GetCameraRotation().Yaw : GetControlRotation().Yaw;
	const FRotator YawRotation(0.f, CameraYaw, 0.f);
	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	SlayerPawn->AddMovementInput(Forward, Axis.Y);
	SlayerPawn->AddMovementInput(Right, Axis.X);
}

void AMockSlayerPlayerController::OnAbilityPressed(FGameplayTag InputTag)
{
	AMockBossCharacterBase* SlayerPawn = GetSlayerPawn();
	if (SlayerPawn != nullptr && IsGameplayInputAllowed())
	{
		SlayerPawn->PressAbilityInput(InputTag);
	}
}

void AMockSlayerPlayerController::OnAbilityReleased(FGameplayTag InputTag)
{
	if (AMockBossCharacterBase* SlayerPawn = GetSlayerPawn())
	{
		SlayerPawn->ReleaseAbilityInput(InputTag);
	}
}

bool AMockSlayerPlayerController::IsInResult() const
{
	const AMockBattleGameState* State = GetWorld()->GetGameState<AMockBattleGameState>();
	return State != nullptr && State->GetPhase() == EMockBattlePhase::Result;
}

void AMockSlayerPlayerController::OnChooseCharacter(int32 Index)
{
	// バトル中の数字キーは変身先の指定
	if (!IsInResult())
	{
		TryBossChange(Index);
		return;
	}

	const AMockBattleGameMode* GameMode = GetWorld()->GetAuthGameMode<AMockBattleGameMode>();
	if (GameMode == nullptr)
	{
		return;
	}
	if (GameMode->GetSelectableCharacters().IsValidIndex(Index))
	{
		SelectedCharacterIndex = Index;
		bSelectionInitialized = true;
	}
}

void AMockSlayerPlayerController::OnSelectStep(int32 Step)
{
	const AMockBattleGameMode* GameMode = GetWorld()->GetAuthGameMode<AMockBattleGameMode>();
	if (!IsInResult() || GameMode == nullptr)
	{
		return;
	}
	const int32 Count = GameMode->GetSelectableCharacters().Num();
	if (Count <= 0)
	{
		return;
	}
	SelectedCharacterIndex = (SelectedCharacterIndex + Step + Count) % Count;
	bSelectionInitialized = true;
}

void AMockSlayerPlayerController::OnConfirm()
{
	AMockBattleGameMode* GameMode = GetWorld()->GetAuthGameMode<AMockBattleGameMode>();
	if (!IsInResult() || GameMode == nullptr)
	{
		return;
	}

	const TArray<UMockBossDefinition*> Characters = GameMode->GetSelectableCharacters();
	UMockBossDefinition* Choice = Characters.IsValidIndex(SelectedCharacterIndex) ? Characters[SelectedCharacterIndex] : nullptr;
	GameMode->RequestNextBattle(Choice);
}

// ---------------------------------------------------------------------
// ボスチェンジ（変身）
// ---------------------------------------------------------------------

float AMockSlayerPlayerController::GetChangeChargePercent() const
{
	return MaxChangeCharge > 0.f ? FMath::Clamp(ChangeCharge / MaxChangeCharge, 0.f, 1.f) : 0.f;
}

bool AMockSlayerPlayerController::IsChangeReady() const
{
	return !IsBossChanged() && ChangeCharge >= MaxChangeCharge;
}

float AMockSlayerPlayerController::GetChangeRemainingPercent() const
{
	return (IsBossChanged() && ChangeDuration > 0.f) ? FMath::Clamp(ChangeRemaining / ChangeDuration, 0.f, 1.f) : 0.f;
}

TArray<UMockBossDefinition*> AMockSlayerPlayerController::GetChangeCandidates() const
{
	TArray<UMockBossDefinition*> Result;
	const UMockProgressSubsystem* Progress = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMockProgressSubsystem>() : nullptr;
	if (Progress == nullptr)
	{
		return Result;
	}

	// もとのキャラ（変身中なら眠っている方）には変身しない
	const AMockBossCharacterBase* Base = IsBossChanged() ? BasePawn.Get() : GetSlayerPawn();
	const UMockBossDefinition* BaseDefinition = Base ? Base->GetDefinition() : nullptr;
	for (const TObjectPtr<UMockBossDefinition>& Boss : Progress->GetCapturedBosses())
	{
		if (Boss != nullptr && Boss.Get() != BaseDefinition)
		{
			Result.Add(Boss);
		}
	}
	return Result;
}

bool AMockSlayerPlayerController::GetChangeBanner(FText& OutName, int32& OutLevel, float& OutAge) const
{
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}
	OutAge = static_cast<float>(World->GetRealTimeSeconds() - ChangeBannerStartRealTime);
	if (OutAge < 0.f || OutAge > ChangeBannerDuration)
	{
		return false;
	}
	OutName = ChangeBannerName;
	OutLevel = ChangeBannerLevel;
	return true;
}

void AMockSlayerPlayerController::OnChangePressed()
{
	// 変身中に押すと元に戻る。変身していなければ1番目のキャラに変身
	if (IsBossChanged())
	{
		if (IsGameplayInputAllowed())
		{
			EndBossChange(true);
		}
		return;
	}
	TryBossChange(0);
}

void AMockSlayerPlayerController::TryBossChange(int32 CandidateIndex)
{
	if (!IsGameplayInputAllowed() || !IsChangeReady())
	{
		return;
	}
	const AMockBossCharacterBase* Current = GetSlayerPawn();
	if (Current == nullptr || Current->IsDead())
	{
		return;
	}
	const TArray<UMockBossDefinition*> Candidates = GetChangeCandidates();
	if (!Candidates.IsValidIndex(CandidateIndex))
	{
		return;
	}
	BeginBossChange(Candidates[CandidateIndex]);
}

void AMockSlayerPlayerController::BeginBossChange(UMockBossDefinition* Target)
{
	AMockBossCharacterBase* Base = GetSlayerPawn();
	UWorld* World = GetWorld();
	if (Base == nullptr || Target == nullptr || World == nullptr)
	{
		return;
	}

	// 変身した姿を、今いる場所に出す（体が大きいほど足元が埋まらないように上げる）
	const float NewHalfHeight = Target->CapsuleHalfHeight * FMath::Max(Target->BodyScale, 0.1f);
	const float OldHalfHeight = Base->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Location = Base->GetActorLocation() + FVector(0.f, 0.f, NewHalfHeight - OldHalfHeight + 5.f);
	const FTransform SpawnTransform(FRotator(0.f, Base->GetActorRotation().Yaw, 0.f), Location);

	AMockBossCharacterBase* NewPawn = World->SpawnActorDeferred<AMockBossCharacterBase>(AMockBossCharacterBase::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (NewPawn == nullptr)
	{
		return;
	}
	NewPawn->SetDefinition(Target);
	// 獲得したボスは、そのボスとして育てたレベルで出てくる
	const UMockProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UMockProgressSubsystem>();
	NewPawn->SetCharacterLevel(Progress ? Progress->GetLevel(Target) : Target->BossLevel);
	UGameplayStatics::FinishSpawningActor(NewPawn, SpawnTransform);

	// もとのキャラの体力と必殺技ゲージを覚えておき、眠らせる
	if (const UMockSlayerAttributeSet* BaseAttributes = Base->GetAttributeSet())
	{
		SavedBaseHealth = BaseAttributes->GetHealth();
		SavedBaseUltimate = BaseAttributes->GetUltimateCharge();
	}
	Base->SetDormant(true);
	BasePawn = Base;

	// ポーンごと乗り換える（Possess で ASC の初期化・技の付与・ステータスの設定が行われる）
	Possess(NewPawn);
	ChangedPawn = NewPawn;
	NewPawn->OnDied.AddUObject(this, &AMockSlayerPlayerController::OnChangedPawnDied);
	UsedChangeForms.AddUnique(Target);

	// 変身した直後から、そのボスの必殺技を撃てる（自分を苦しめた技をすぐ使える）
	if (UAbilitySystemComponent* ASC = NewPawn->GetAbilitySystemComponent())
	{
		ASC->SetNumericAttributeBase(UMockSlayerAttributeSet::GetUltimateChargeAttribute(), NewPawn->GetAttributeSet()->GetMaxUltimateCharge());
	}

	ChangeCharge = 0.f;
	ChangeRemaining = ChangeDuration;

	// ---- 演出 ----
	// 時間をほぼ止めて、変身した姿と名乗りを見せる
	NewPawn->StartSlowMotion(0.12f, 1.2f);
	NewPawn->SpawnEffectAttached(Target->UltimateWarmupEffect, NAME_None, 1.5f);
	NewPawn->SpawnEffectAtLocation(Target->ImpactEffect, NewPawn->GetActorLocation() - FVector(0.f, 0.f, NewHalfHeight * 0.8f), FRotator::ZeroRotator, 3.f * NewPawn->GetBodyScale());
	if (const FMockSlayerVoiceSet* Voices = NewPawn->GetVoices())
	{
		NewPawn->PlayVoice(Voices->IntroBoast, AMockBossCharacterBase::EVoicePriority::Important);
	}

	// 登場モーションで構える。長すぎるので途中で止めて操作に戻す
	NewPawn->PlayIntro();
	TWeakObjectPtr<AMockBossCharacterBase> WeakPawn = NewPawn;
	GetWorldTimerManager().SetTimer(ChangeIntroStopTimer, FTimerDelegate::CreateWeakLambda(this, [WeakPawn]()
	{
		AMockBossCharacterBase* Pawn = WeakPawn.Get();
		UAnimInstance* AnimInstance = (Pawn && Pawn->GetMesh()) ? Pawn->GetMesh()->GetAnimInstance() : nullptr;
		const UMockBossDefinition* Definition = Pawn ? Pawn->GetDefinition() : nullptr;
		if (AnimInstance != nullptr && Definition != nullptr && Definition->IntroMontage != nullptr)
		{
			AnimInstance->Montage_Stop(0.25f, Definition->IntroMontage);
		}
	}), 0.9f, false);

	PlayChangeShockwave(NewPawn);

	ChangeBannerName = NewPawn->GetFormName();
	ChangeBannerLevel = NewPawn->GetCharacterLevel();
	ChangeBannerStartRealTime = World->GetRealTimeSeconds();

	UE_LOG(LogTemp, Log, TEXT("[BossChange] %s -> %s (Lv.%d)"), *GetNameSafe(Base->GetDefinition()), *GetNameSafe(Target), NewPawn->GetCharacterLevel());
}

void AMockSlayerPlayerController::PlayChangeShockwave(AMockBossCharacterBase* Center)
{
	const AMockBattleGameState* State = GetWorld()->GetGameState<AMockBattleGameState>();
	AMockBossCharacterBase* Enemy = State ? State->Boss.Get() : nullptr;
	if (Center == nullptr || Enemy == nullptr || Enemy->IsDead())
	{
		return;
	}

	// 変身の衝撃で、近くにいるボスを吹き飛ばす（大きい体ほど遠くまで届く）
	FVector Away = Enemy->GetActorLocation() - Center->GetActorLocation();
	Away.Z = 0.f;
	const float Radius = ChangeShockwaveRadius * Center->GetBodyScale() + Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius();
	if (Away.Size() > Radius)
	{
		return;
	}
	Away = Away.GetSafeNormal(UE_SMALL_NUMBER, Center->GetActorForwardVector());
	Enemy->LaunchCharacter(Away * ChangeShockwaveStrength + FVector(0.f, 0.f, 350.f), true, true);
}

void AMockSlayerPlayerController::EndBossChange(bool bWithEffect)
{
	AMockBossCharacterBase* Changed = ChangedPawn;
	AMockBossCharacterBase* Base = BasePawn;
	GetWorldTimerManager().ClearTimer(ChangeRevertTimer);
	GetWorldTimerManager().ClearTimer(ChangeIntroStopTimer);
	ChangedPawn = nullptr;
	BasePawn = nullptr;
	ChangeRemaining = 0.f;
	if (Changed == nullptr || Base == nullptr)
	{
		return;
	}

	// 変身していた場所に、もとのキャラを戻す
	const float BaseHalfHeight = Base->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float ChangedHalfHeight = Changed->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Location = Changed->GetActorLocation() - FVector(0.f, 0.f, ChangedHalfHeight - BaseHalfHeight);
	Base->SetActorLocationAndRotation(Location, FRotator(0.f, Changed->GetActorRotation().Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	Base->SetDormant(false);

	Changed->OnDied.RemoveAll(this);
	Possess(Base);

	// Possess でステータスが初期化されるので、変身前の体力と必殺技ゲージに戻す
	if (UAbilitySystemComponent* ASC = Base->GetAbilitySystemComponent())
	{
		ASC->SetNumericAttributeBase(UMockSlayerAttributeSet::GetHealthAttribute(), FMath::Max(SavedBaseHealth, 1.f));
		ASC->SetNumericAttributeBase(UMockSlayerAttributeSet::GetUltimateChargeAttribute(), SavedBaseUltimate);
	}

	if (bWithEffect && Base->GetDefinition() != nullptr)
	{
		Base->SpawnEffectAttached(Base->GetDefinition()->UltimateWarmupEffect, NAME_None, 1.2f);
		if (Changed->GetDefinition() != nullptr)
		{
			Changed->SpawnEffectAtLocation(Changed->GetDefinition()->ImpactEffect, Changed->GetActorLocation(), FRotator::ZeroRotator, 2.f * Changed->GetBodyScale());
		}
	}
	Changed->Destroy();

	UE_LOG(LogTemp, Log, TEXT("[BossChange] 解除 -> %s"), *GetNameSafe(Base->GetDefinition()));
}

void AMockSlayerPlayerController::OnChangedPawnDied(AMockBossCharacterBase* DeadPawn)
{
	// 変身した姿が倒れても負けではない。少し倒れる姿を見せてから元のキャラに戻る
	if (DeadPawn == nullptr || DeadPawn != ChangedPawn)
	{
		return;
	}
	GetWorldTimerManager().SetTimer(ChangeRevertTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		EndBossChange(true);
	}), 0.8f, false);
}

void AMockSlayerPlayerController::OnPawnDealtDamage(AMockBossCharacterBase* Source, float Damage)
{
	// 変身中は溜まらない
	if (IsBossChanged() || Damage <= 0.f)
	{
		return;
	}
	const AMockBattleGameState* State = GetWorld()->GetGameState<AMockBattleGameState>();
	const AMockBossCharacterBase* Boss = State ? State->Boss.Get() : nullptr;
	const UMockSlayerAttributeSet* BossAttributes = Boss ? Boss->GetAttributeSet() : nullptr;
	if (BossAttributes == nullptr || BossAttributes->GetMaxHealth() <= 0.f || ChangeFullDamageRatio <= 0.f)
	{
		return;
	}
	const float Ratio = Damage / (BossAttributes->GetMaxHealth() * ChangeFullDamageRatio);
	ChangeCharge = FMath::Min(ChangeCharge + Ratio * MaxChangeCharge, MaxChangeCharge);
}

void AMockSlayerPlayerController::UpdateBossChange(float DeltaTime)
{
	const AMockBattleGameState* State = GetWorld()->GetGameState<AMockBattleGameState>();
	const EMockBattlePhase Phase = State ? State->GetPhase() : EMockBattlePhase::Battle;

	if (IsBossChanged())
	{
		// 変身していられる時間が尽きたら戻る（倒れて戻る途中なら任せる）
		if (ChangedPawn->IsDead())
		{
			return;
		}
		ChangeRemaining -= DeltaTime;
		if (ChangeRemaining <= 0.f)
		{
			EndBossChange(true);
		}
		return;
	}

	if (CVarSlayerChangeFull.GetValueOnGameThread() > 0)
	{
		ChangeCharge = MaxChangeCharge;
		return;
	}
	if (Phase == EMockBattlePhase::Battle)
	{
		ChangeCharge = FMath::Min(ChangeCharge + ChangeChargePerSecond * DeltaTime, MaxChangeCharge);
	}
}
