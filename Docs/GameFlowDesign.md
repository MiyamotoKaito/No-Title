# ボス獲得型ボスバトル 設計書（ゲームフロー）

Nov 6, 2026 · @Miyamoto

タイトルからボスを選んで戦い、塊を砕いてリザルトを見て、またボス選択に戻るまでの流れと、それをUE5でどう組むかをまとめる。

## 全体フロー

&#91;embedded content: 画面・フェーズ遷移 · メニュー側3画面、バトル側6フェーズ\]

メニュー側はレベルとWidget、バトル側は1つのレベルの中でGameModeのフェーズを進める。下段の英字はEBattlePhaseの値。

## 各画面・フェーズの役割

画面は「メニュー側」と「バトル側」の2つに分かれ、バトル側の中身はフェーズで切り替える。

| # | 画面・フェーズ | やること | 次へ進む条件 | 主な担当 |
| --- | --- | --- | --- | --- |
| 1 | タイトル | ゲーム開始、セーブデータ読み込み | 決定ボタン | WBP\_Title |
| 2 | ボス選択画面 | 挑むボスを選ぶ。推奨レベルを表示 | ボス決定 | WBP\_BossSelect |
| 3 | キャラクター選択画面 | 獲得済みのキャラ（ボス）から操作キャラを選ぶ。レベル表示 | キャラ決定 | WBP\_CharacterSelect |
| 4 | 戦闘（レベル読み込み） | 選んだボスのアリーナを開き、ボスとプレイヤーキャラをSpawn・Possess | 読み込み完了 | ABattleGameMode |
| 5 | 戦闘開始のシーケンス | ボスの登場演出（Level Sequence）。この間は操作不可 | シーケンス終了 | ABattleGameMode / Level Sequence |
| 6 | Battle | ボス戦本体。ボスはBT＋GA、プレイヤーは入力＋GA | ボスのHPが0 | ABattleGameMode / GAS |
| 7 | 戦闘終了 | ボスの撃破演出、入力停止 | 演出終了 | ABattleGameMode |
| 8 | Bossの塊出現 | 塊Actorを出し、制限時間のカウント開始 | 塊を砕く or 時間切れ | ACrushChunk / ABattleGameMode |
| 9 | リザルト画面 | 獲得の成否、経験値、レベルアップを表示しセーブ | 決定ボタン | WBP\_Result |
| → | ボス選択画面へ戻る | メニュー側のレベルを開き直す | — | — |

## UE上の構成

レベルは「タイトル」「メニュー」「バトル（ボスごとのアリーナ）」の3種類に分け、バトル内の流れはGameModeが持つフェーズの状態で管理する。

### レベル分け

| レベル | 含む画面 | GameMode |
| --- | --- | --- |
| L\_Title | タイトル | ATitleGameMode |
| L\_Menu | ボス選択、キャラクター選択（同じレベル内でWidgetを切り替え） | AMenuGameMode |
| L\_Arena\_<ボス名> | 戦闘〜リザルト | ABattleGameMode |

ボス選択とキャラ選択は行き来が多いので、レベルを分けずにWidgetの切り替えにする。リザルトもバトルのレベル上に重ねて出し、決定でL\_Menuを開く。

### バトル内のフェーズ管理

```cpp
UENUM(BlueprintType)
enum class EBattlePhase : uint8
{
    Intro,    // 戦闘開始のシーケンス
    Battle,   // ボス戦本体
    BossDown, // 戦闘終了の演出
    Crush,    // 塊を砕く
    Result    // リザルト
};
```

- ABattleGameMode が現在のフェーズを持ち、`SetPhase()` だけで切り替える
- フェーズ変更はデリゲート（OnPhaseChanged）で通知し、UI・カメラ・入力の有効化はそれを受けて各自で切り替える
- 現在のフェーズはGameStateにも持たせておくと、レイドでマルチにした時にクライアントへそのまま複製できる

### フェーズを進めるきっかけ

| 遷移 | きっかけ |
| --- | --- |
| Intro → Battle | Level Sequenceの再生終了イベント |
| Battle → BossDown | ボスのHP Attributeが0になった時（GASのAttribute変更通知、またはGameplayEvent） |
| BossDown → Crush | 撃破演出の終了 |
| Crush → Result | 塊のHPが0、またはタイマー終了 |
| Result → L\_Menu | リザルト画面で決定 |

## ブレイクシステム

ボスはブレイク値を持ち、攻撃で削りきるとひるんで一定時間「攻撃し放題」になる。Battleフェーズの中の状態として、GASのAttributeとタグで実装する。

### 状態の流れ

1. 通常：攻撃を当てるたびにブレイク値が減る
2. ブレイク値が0：GE\_Breakを適用し、タグ `State.Break` を付ける
3. ブレイク中：ボスは行動停止・ひるみモーション。プレイヤーは攻撃し放題
4. 効果時間が終わる：タグが外れ、ブレイク値を最大まで回復して通常に戻る

### 実装

| 要素 | 内容 |
| --- | --- |
| Attribute | BossのAttributeSetに `Break` と `MaxBreak` を追加 |
| ブレイクダメージ | ダメージ用のExecutionCalculationでHealthと同時にBreakも減らす。技ごとのブレイク量はSetByCaller（またはGAごとの値）で渡す |
| ブレイク発生 | AttributeSetの `PostGameplayEffectExecute` でBreakが0になったらGameplayEventを送り、GA\_Breakを発動 |
| GA\_Break | 実行中のボスの技をキャンセルし、ひるみモンタージュを再生。GE\_Break（Duration）で `State.Break` を付与 |
| 技の封印 | ボスの攻撃GAの `ActivationBlockedTags` に `State.Break` を入れ、ブレイク中は発動できなくする |
| BT | ルートにGameplay Tag条件のDecorator（Observer Abort）を置き、`State.Break` 中は行動を止める |
| 回復 | GE\_Breakの終了時にBreakをMaxBreakに戻す |
| UI | ボスのHPバーの下にブレイクゲージを表示 |

ブレイクはタグで管理するので、GAとBTの両方が同じ `State.Break` を見るだけで止まり、個別に止める処理は要らない。

### ブレイクの未決定事項

- [ ] ブレイク時間の長さ（秒数はGEのDurationで調整できるようにする）
- [ ] ブレイク中のダメージ倍率を上げるか
- [ ] ブレイクするたびにMaxBreakを増やすか（連続ブレイクのしすぎ防止）
- [ ] プレイヤーがボスを操作している時も、プレイヤー側にブレイク値を持たせるか

## データの受け渡し

レベルを開き直すとGameModeやWidgetは消えるので、画面をまたぐデータはGameInstanceSubsystemに持たせる。

| クラス | 持つデータ | 寿命 |
| --- | --- | --- |
| UBattleSessionSubsystem | 選んだボス（UBossDefinition）、選んだキャラ（UBossDefinition）、直前のバトル結果 | 1回のバトルの間（ボス選択で上書き） |
| UProgressSubsystem | 獲得済みボスの一覧、キャラごとの経験値・レベル | ゲーム起動中ずっと |
| UBossCaptureSaveGame（USaveGame） | UProgressSubsystemの中身 | ディスクに保存 |

### 流れ

1. タイトル：UProgressSubsystemがSaveGameを読み込む
2. ボス選択：UBattleSessionSubsystemに選んだボスを書く
3. キャラ選択：UProgressSubsystemから獲得済みキャラを並べ、選んだキャラを書く
4. 戦闘：ABattleGameModeがUBattleSessionSubsystemを読んで、アリーナにボスとキャラをSpawnする
5. リザルト：結果をUBattleSessionSubsystemに書き、獲得したボスと経験値をUProgressSubsystemに反映してセーブ

どちらのSubsystemもGameModeやWidgetから参照するだけにして、画面同士が直接データを渡し合わない形にする。

## 未決定事項

- [ ] プレイヤーが倒れた時の流れ（リトライ、敗北リザルト、ボス選択に戻るなど）
- [ ] 最初のキャラクター：まだボスを1体も獲得していない時に何で戦うか
- [ ] 塊を時間内に砕けなかった時も経験値を得られるか
- [ ] 戦闘中のポーズ・リタイアの有無
- [ ] レイドボスへの入口（ボス選択画面に並べるか、別メニューにするか）
