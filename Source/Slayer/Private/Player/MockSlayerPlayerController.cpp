#include "Player/MockSlayerPlayerController.h"

#include "AbilitySystemComponent.h"
#include "Cameras/ComposableCameraCameraBase.h"
#include "Character/MockBossCharacterBase.h"
#include "Core/ComposableCameraParameterBlock.h"
#include "Core/ComposableCameraPlayerCameraManager.h"
#include "DataAssets/ComposableCameraTransitionDataAsset.h"
#include "DataAssets/ComposableCameraTypeAsset.h"
#include "Game/MockCrushChunk.h"
#include "UObject/ConstructorHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Game/MockBattleGameMode.h"
#include "Game/MockBattleGameState.h"
#include "Game/MockSlayerSubsystems.h"
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

	// 最初は必ず自由カメラから始める
	// （ロックオンカメラは距離を持たず、直前のカメラの位置を引き継ぐ作りなので）
	if (!bCameraActivated && FreeCamera != nullptr)
	{
		CameraManager->ActivateNewCameraFromTypeAsset(FreeCamera, nullptr, FComposableCameraActivateParams(), FComposableCameraParameterBlock());
		bCameraActivated = true;
		CameraTarget = nullptr;
		// ロックオンは次のフレームで（自由カメラの位置が一度計算されてから引き継ぐ）
		return;
	}

	UComposableCameraTypeAsset* CameraAsset = (Target != nullptr && LockOnCamera != nullptr) ? LockOnCamera.Get() : FreeCamera.Get();
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
}

void AMockSlayerPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

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
	const UMockBattleSessionSubsystem* Session = GetGameInstance()->GetSubsystem<UMockBattleSessionSubsystem>();
	const AMockBossCharacterBase* SlayerPawn = GetSlayerPawn();

	UMockBossDefinition* Preferred = nullptr;
	if (Session != nullptr && Session->LastResult.bCaptured)
	{
		Preferred = Session->LastResult.Boss;
	}
	else if (SlayerPawn != nullptr)
	{
		Preferred = SlayerPawn->GetDefinition();
	}

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
	const AMockBattleGameMode* GameMode = GetWorld()->GetAuthGameMode<AMockBattleGameMode>();
	if (!IsInResult() || GameMode == nullptr)
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
