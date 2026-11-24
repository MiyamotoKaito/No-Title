#include "Game/MockBattleGameMode.h"

#include "AIController.h"
#include "Camera/CameraActor.h"
#include "Particles/ParticleSystem.h"
#include "Character/MockBossCharacterBase.h"
#include "Data/MockBossDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/MockCrushChunk.h"
#include "Game/MockSlayerSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Player/MockSlayerPlayerController.h"
#include "TimerManager.h"
#include "UI/MockSlayerHUD.h"

namespace
{
	// 開発用：経験値の倍率（進化の確認などに使う）
	TAutoConsoleVariable<float> CVarSlayerExpScale(TEXT("slayer.ExpScale"), 1.f, TEXT("経験値の倍率（開発用）"));
}

AMockBattleGameMode::AMockBattleGameMode()
{
	DefaultPawnClass = AMockBossCharacterBase::StaticClass();
	PlayerControllerClass = AMockSlayerPlayerController::StaticClass();
	GameStateClass = AMockBattleGameState::StaticClass();
	HUDClass = AMockSlayerHUD::StaticClass();
}

void AMockBattleGameMode::StartPlay()
{
	Super::StartPlay();

	// 初期キャラは最初から獲得済みにしておく
	if (UMockProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UMockProgressSubsystem>())
	{
		Progress->AddCapturedBoss(StarterCharacter, 1);
	}

	// プレイヤーの Spawn と Possess が終わってから始める
	GetWorldTimerManager().SetTimerForNextTick(this, &AMockBattleGameMode::BeginBattle);
}

APawn* AMockBattleGameMode::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	// 選んだキャラ（ボス）のポーンを出し、そのまま PlayerController が Possess する
	const FTransform SpawnTransform = StartSpot ? FTransform(StartSpot->GetActorRotation(), StartSpot->GetActorLocation()) : FTransform::Identity;

	AMockBossCharacterBase* Pawn = GetWorld()->SpawnActorDeferred<AMockBossCharacterBase>(AMockBossCharacterBase::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (Pawn == nullptr)
	{
		return nullptr;
	}
	UMockBossDefinition* PlayerDefinition = GetPlayerDefinition();
	Pawn->SetDefinition(PlayerDefinition);
	// キャラごとに育てたレベルで戦う
	if (const UMockProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UMockProgressSubsystem>())
	{
		Pawn->SetCharacterLevel(Progress->GetLevel(PlayerDefinition));
	}
	UGameplayStatics::FinishSpawningActor(Pawn, SpawnTransform);
	Pawn->OnDied.AddUObject(this, &AMockBattleGameMode::OnPlayerDied);
	return Pawn;
}

UMockBossDefinition* AMockBattleGameMode::GetPlayerDefinition() const
{
	const UMockBattleSessionSubsystem* Session = GetGameInstance()->GetSubsystem<UMockBattleSessionSubsystem>();
	if (Session != nullptr && Session->SelectedCharacter != nullptr)
	{
		return Session->SelectedCharacter;
	}
	return StarterCharacter;
}

AMockBattleGameState* AMockBattleGameMode::GetBattleGameState() const
{
	return GetGameState<AMockBattleGameState>();
}

void AMockBattleGameMode::SetPhase(EMockBattlePhase NewPhase, float Duration)
{
	GetWorldTimerManager().ClearTimer(PhaseTimer);
	if (AMockBattleGameState* State = GetBattleGameState())
	{
		State->SetPhase(NewPhase, Duration);
	}
	UE_LOG(LogTemp, Log, TEXT("[Battle] Phase -> %s"), *UEnum::GetValueAsString(NewPhase));
}

void AMockBattleGameMode::BeginBattle()
{
	SpawnBoss();

	AMockBattleGameState* State = GetBattleGameState();
	AMockBossCharacterBase* Boss = State ? State->Boss.Get() : nullptr;
	if (Boss == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] ボスを出せませんでした。BP_BattleGameMode の BossRoster を確認してください"));
		return;
	}

	// 登場演出（Intro）→ バトル開始
	float Duration = Boss->PlayIntro();
	Duration = Duration > 0.f ? FMath::Min(Duration, MaxIntroDuration) : IntroDuration;

	SetPhase(EMockBattlePhase::Intro, Duration);
	GetWorldTimerManager().SetTimer(PhaseTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		SetPhase(EMockBattlePhase::Battle);
	}), Duration, false);
}

void AMockBattleGameMode::SpawnBoss()
{
	UMockBattleSessionSubsystem* Session = GetGameInstance()->GetSubsystem<UMockBattleSessionSubsystem>();
	CurrentBossDefinition = (Session && Session->SelectedBoss) ? Session->SelectedBoss.Get() : ChooseNextBoss(GetPlayerDefinition());
	if (CurrentBossDefinition == nullptr)
	{
		return;
	}

	// プレイヤーの前方に、プレイヤーの方を向けて出す
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	FVector Origin = PlayerPawn ? PlayerPawn->GetActorLocation() : FVector::ZeroVector;
	FVector Forward = PlayerPawn ? PlayerPawn->GetActorForwardVector() : FVector::ForwardVector;
	Forward.Z = 0.f;
	Forward.Normalize();

	const FVector Location = Origin + Forward * BossSpawnDistance + FVector(0.f, 0.f, 50.f);
	const FTransform SpawnTransform((-Forward).Rotation(), Location);

	AMockBossCharacterBase* Boss = GetWorld()->SpawnActorDeferred<AMockBossCharacterBase>(AMockBossCharacterBase::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (Boss == nullptr)
	{
		return;
	}
	Boss->SetDefinition(CurrentBossDefinition);
	Boss->SetCharacterLevel(CurrentBossDefinition->BossLevel);
	UGameplayStatics::FinishSpawningActor(Boss, SpawnTransform);

	// 同じクラスを AIController が Possess すると「敵」になる
	Boss->SpawnDefaultController();
	Boss->OnDied.AddUObject(this, &AMockBattleGameMode::OnBossDied);

	if (AMockBattleGameState* State = GetBattleGameState())
	{
		State->Boss = Boss;
		State->BossDefinition = CurrentBossDefinition;
	}
}

void AMockBattleGameMode::OnBossDied(AMockBossCharacterBase* DeadBoss)
{
	AMockBattleGameState* State = GetBattleGameState();
	if (State == nullptr || State->GetPhase() != EMockBattlePhase::Battle)
	{
		return;
	}

	SetPhase(EMockBattlePhase::BossDown, BossDownDuration);
	GetWorldTimerManager().SetTimer(PhaseTimer, this, &AMockBattleGameMode::StartCrush, BossDownDuration, false);
}

void AMockBattleGameMode::OnPlayerDied(AMockBossCharacterBase* DeadPlayer)
{
	AMockBattleGameState* State = GetBattleGameState();
	// 獲得演出中・リザルト中に倒れても負けにしない
	if (State == nullptr || State->GetPhase() == EMockBattlePhase::Result || State->GetPhase() == EMockBattlePhase::Capture || State->GetPhase() == EMockBattlePhase::Evolution)
	{
		return;
	}
	FinishBattle(false, false);
}

void AMockBattleGameMode::StartCrush()
{
	AMockBattleGameState* State = GetBattleGameState();
	AMockBossCharacterBase* Boss = State ? State->Boss.Get() : nullptr;
	if (State == nullptr || Boss == nullptr || CurrentBossDefinition == nullptr)
	{
		FinishBattle(true, false);
		return;
	}

	// ボスが塊になる
	const FVector Location = Boss->GetActorLocation() + FVector(0.f, 0.f, 20.f);
	State->Boss = nullptr;
	Boss->Destroy();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AMockCrushChunk* Chunk = GetWorld()->SpawnActor<AMockCrushChunk>(AMockCrushChunk::StaticClass(), Location, FRotator::ZeroRotator, Params);
	if (Chunk == nullptr)
	{
		FinishBattle(true, false);
		return;
	}
	// 強いボスほど塊も硬い（砕く力＝レベルで縮める）
	const float ChunkHealth = CurrentBossDefinition->CrushChunkHealth * CurrentBossDefinition->GetLevelScale(CurrentBossDefinition->BossLevel);
	Chunk->InitializeChunk(CurrentBossDefinition, ChunkHealth);
	Chunk->OnBroken.AddUObject(this, &AMockBattleGameMode::OnChunkBroken);
	State->Chunk = Chunk;

	const float TimeLimit = FMath::Max(CurrentBossDefinition->CrushTimeLimit, 1.f);
	SetPhase(EMockBattlePhase::Crush, TimeLimit);
	GetWorldTimerManager().SetTimer(PhaseTimer, this, &AMockBattleGameMode::OnCrushTimeout, TimeLimit, false);
}

void AMockBattleGameMode::OnChunkBroken(AMockCrushChunk* Chunk)
{
	AMockBattleGameState* State = GetBattleGameState();
	if (State == nullptr || State->GetPhase() != EMockBattlePhase::Crush)
	{
		return;
	}
	// ダメージ処理の途中で塊を消さないように、次のフレームで演出を始める
	const FVector Location = Chunk->GetActorLocation();
	SetPhase(EMockBattlePhase::Capture);
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, Location]()
	{
		StartCaptureCinematic(Location);
	}));
}

void AMockBattleGameMode::StartCaptureCinematic(const FVector& Location)
{
	AMockBattleGameState* State = GetBattleGameState();
	if (State != nullptr && State->Chunk != nullptr)
	{
		State->Chunk->Destroy();
		State->Chunk = nullptr;
	}

	UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ChunkBreakEffect, Location, FRotator::ZeroRotator, FVector(2.f));

	// プレイヤーの方を向いてボスが現れる
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	const APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;
	FVector ToPlayer = PlayerPawn ? PlayerPawn->GetActorLocation() - Location : FVector::ForwardVector;
	ToPlayer.Z = 0.f;
	if (!ToPlayer.Normalize())
	{
		ToPlayer = FVector::ForwardVector;
	}

	const FTransform BossTransform(ToPlayer.Rotation(), Location + FVector(0.f, 0.f, 30.f));
	AMockBossCharacterBase* DisplayBoss = GetWorld()->SpawnActorDeferred<AMockBossCharacterBase>(AMockBossCharacterBase::StaticClass(), BossTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (DisplayBoss != nullptr)
	{
		DisplayBoss->SetDefinition(CurrentBossDefinition);
		DisplayBoss->SetCharacterLevel(CurrentBossDefinition ? CurrentBossDefinition->BossLevel : 1);
		UGameplayStatics::FinishSpawningActor(DisplayBoss, BossTransform);
		DisplayBoss->SetActorEnableCollision(false);
		DisplayBoss->PlayIntro();
		CaptureDisplayBoss = DisplayBoss;
	}
	UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), CaptureEffect, Location, FRotator::ZeroRotator);

	// ボスの斜め前から見るカメラに切り替える。主役はボスなので、プレイヤーは演出中だけ隠す
	if (PC != nullptr && PC->GetPawn() != nullptr)
	{
		PC->GetPawn()->SetActorHiddenInGame(true);
	}
	const FVector CameraDirection = ToPlayer.RotateAngleAxis(25.f, FVector::UpVector);
	ViewFromCamera(Location + CameraDirection * 360.f + FVector(0.f, 0.f, 70.f), Location + FVector(0.f, 0.f, 60.f), 0.8f);

	SetPhase(EMockBattlePhase::Capture, CaptureDuration);
	GetWorldTimerManager().SetTimer(PhaseTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		FinishBattle(true, true);
	}), CaptureDuration, false);
}

void AMockBattleGameMode::OnCrushTimeout()
{
	FinishBattle(true, false);
}

void AMockBattleGameMode::FinishBattle(bool bVictory, bool bCaptured)
{
	AMockBattleGameState* State = GetBattleGameState();
	if (State != nullptr && State->Chunk != nullptr)
	{
		State->Chunk->Destroy();
		State->Chunk = nullptr;
	}

	FMockSlayerBattleResult Result;
	Result.Boss = CurrentBossDefinition;
	Result.Character = GetPlayerDefinition();
	Result.bVictory = bVictory;
	Result.bCaptured = bCaptured;

	if (UMockProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UMockProgressSubsystem>())
	{
		// 獲得したボスは、戦った時のレベルのまま仲間になる
		const int32 BossLevel = CurrentBossDefinition ? CurrentBossDefinition->BossLevel : 1;
		if (bCaptured)
		{
			Result.bNewCapture = Progress->AddCapturedBoss(CurrentBossDefinition, BossLevel);
		}

		// 経験値：勝てば多め、獲得でボーナス、負けても少しは入る
		int32 Exp = bVictory ? VictoryExpBase + VictoryExpPerBossLevel * BossLevel : DefeatExpBase + DefeatExpPerBossLevel * BossLevel;
		if (bCaptured)
		{
			Exp += CaptureExpBonus;
		}
		// 開発用：slayer.ExpScale で経験値を増やせる（進化の確認用）
		Exp = FMath::RoundToInt(Exp * FMath::Max(CVarSlayerExpScale.GetValueOnGameThread(), 0.f));
		Result.ExpGained = Exp;
		Result.OldLevel = Progress->GetLevel(Result.Character);
		Progress->AddExp(Result.Character, Exp);
		Result.NewLevel = Progress->GetLevel(Result.Character);

		// 形態が変わるレベルに達したら進化
		if (Result.Character != nullptr && Result.Character->GetFormIndex(Result.NewLevel) > Result.Character->GetFormIndex(Result.OldLevel))
		{
			Result.bEvolved = true;
			Result.OldFormName = Result.Character->GetFormName(Result.OldLevel);
			Result.NewFormName = Result.Character->GetFormName(Result.NewLevel);
		}
	}

	if (UMockBattleSessionSubsystem* Session = GetGameInstance()->GetSubsystem<UMockBattleSessionSubsystem>())
	{
		Session->LastResult = Result;
	}

	// 進化した時は、リザルトの前に進化演出を入れる（倒れている時は演出なしでリザルトに表示だけ）
	const AMockBossCharacterBase* PlayerPawn = Cast<AMockBossCharacterBase>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (Result.bEvolved && PlayerPawn != nullptr && !PlayerPawn->IsDead())
	{
		StartEvolutionCinematic(Result.NewLevel);
		return;
	}

	SetPhase(EMockBattlePhase::Result);
}

void AMockBattleGameMode::StartEvolutionCinematic(int32 NewLevel)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	AMockBossCharacterBase* PlayerPawn = PC ? Cast<AMockBossCharacterBase>(PC->GetPawn()) : nullptr;
	if (PlayerPawn == nullptr)
	{
		SetPhase(EMockBattlePhase::Result);
		return;
	}

	// 獲得演出のボスはもう出番が終わり。主役は自分のキャラ
	if (CaptureDisplayBoss != nullptr)
	{
		CaptureDisplayBoss->Destroy();
		CaptureDisplayBoss = nullptr;
	}
	PlayerPawn->SetActorHiddenInGame(false);
	PlayerPawn->GetCharacterMovement()->StopMovementImmediately();

	// 正面から見るカメラ
	const FVector Location = PlayerPawn->GetActorLocation();
	const FVector Forward = PlayerPawn->GetActorForwardVector().GetSafeNormal2D();
	ViewFromCamera(Location + Forward.RotateAngleAxis(-20.f, FVector::UpVector) * 340.f + FVector(0.f, 0.f, 40.f), Location + FVector(0.f, 0.f, 30.f), 0.7f);

	SetPhase(EMockBattlePhase::Evolution, EvolutionDuration);

	// カメラが寄ってから姿が変わる
	TWeakObjectPtr<AMockBossCharacterBase> WeakPawn = PlayerPawn;
	FTimerHandle EvolveTimer;
	GetWorldTimerManager().SetTimer(EvolveTimer, FTimerDelegate::CreateWeakLambda(this, [this, WeakPawn, NewLevel]()
	{
		if (AMockBossCharacterBase* Pawn = WeakPawn.Get())
		{
			Pawn->Evolve(NewLevel, EvolutionEffect);
		}
	}), 1.f, false);

	GetWorldTimerManager().SetTimer(PhaseTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		SetPhase(EMockBattlePhase::Result);
	}), EvolutionDuration, false);
}

void AMockBattleGameMode::ViewFromCamera(const FVector& CameraLocation, const FVector& Focus, float BlendTime)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC == nullptr)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACameraActor* CameraActor = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), CameraLocation, (Focus - CameraLocation).Rotation(), Params);
	if (CameraActor == nullptr)
	{
		return;
	}
	PC->SetViewTargetWithBlend(CameraActor, BlendTime, VTBlend_Cubic);

	// 前の演出カメラはブレンドが終わってから消す
	if (AActor* OldCamera = CaptureCamera)
	{
		OldCamera->SetLifeSpan(BlendTime + 0.1f);
	}
	CaptureCamera = CameraActor;
}

void AMockBattleGameMode::RequestNextBattle(UMockBossDefinition* CharacterChoice)
{
	UMockBattleSessionSubsystem* Session = GetGameInstance()->GetSubsystem<UMockBattleSessionSubsystem>();
	if (Session == nullptr)
	{
		return;
	}

	Session->SelectedCharacter = CharacterChoice ? CharacterChoice : GetPlayerDefinition();

	// 負けた時は同じボスに再挑戦。勝った時は次のボスへ
	if (Session->LastResult.bVictory || Session->SelectedBoss == nullptr)
	{
		Session->SelectedBoss = ChooseNextBoss(Session->SelectedCharacter);
	}
	// 自分と同じボスとは戦わない
	if (Session->SelectedBoss == Session->SelectedCharacter)
	{
		Session->SelectedBoss = ChooseNextBoss(Session->SelectedCharacter);
	}

	const FString LevelName = UGameplayStatics::GetCurrentLevelName(this, true);
	UGameplayStatics::OpenLevel(this, FName(*LevelName));
}

TArray<UMockBossDefinition*> AMockBattleGameMode::GetSelectableCharacters() const
{
	TArray<UMockBossDefinition*> Result;
	if (const UMockProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UMockProgressSubsystem>())
	{
		for (const TObjectPtr<UMockBossDefinition>& Boss : Progress->GetCapturedBosses())
		{
			if (Boss != nullptr)
			{
				Result.Add(Boss);
			}
		}
	}
	return Result;
}

UMockBossDefinition* AMockBattleGameMode::ChooseNextBoss(const UMockBossDefinition* Exclude) const
{
	const UMockProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UMockProgressSubsystem>();

	// まだ獲得していないボスを、一覧の順に
	for (const TObjectPtr<UMockBossDefinition>& Boss : BossRoster)
	{
		if (Boss != nullptr && Boss.Get() != Exclude && (Progress == nullptr || !Progress->IsCaptured(Boss)))
		{
			return Boss;
		}
	}

	// 全員獲得済みならランダム
	TArray<UMockBossDefinition*> Candidates;
	for (const TObjectPtr<UMockBossDefinition>& Boss : BossRoster)
	{
		if (Boss != nullptr && Boss.Get() != Exclude)
		{
			Candidates.Add(Boss);
		}
	}
	return Candidates.Num() > 0 ? Candidates[FMath::RandRange(0, Candidates.Num() - 1)] : nullptr;
}
