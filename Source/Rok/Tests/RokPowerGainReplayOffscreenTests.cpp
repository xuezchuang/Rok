#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Misc/SecureHash.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "../RokPowerGainWidget.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/WidgetRenderer.h"
#include "Rendering/DrawElements.h"
#include "RenderingThread.h"
#include "RHIGlobals.h"
#include "RHITypes.h"
#include "ShaderCompiler.h"
#include "ImageUtils.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

// UE 5.8 WindowsPlatformFile.cpp uses GetFileAttributesW and
// GetFinalPathNameByHandle for the same directory/reparse boundary.
#if PLATFORM_WINDOWS
struct FReplayPathAttributes
{
	uint32 Flags;
	uint32 Error;
};
using FReplayAttributeReader = FReplayPathAttributes (*)(const FString&);

static FReplayPathAttributes ReplayReadAttributes(const FString& Path)
{
	FString Native = Path;
	FPaths::MakePlatformFilename(Native);
	const DWORD Flags = GetFileAttributesW(*Native);
	return {Flags, Flags == INVALID_FILE_ATTRIBUTES ? GetLastError() : ERROR_SUCCESS};
}

static bool ReplayNoReparseAncestor(const FString& Candidate, FReplayAttributeReader ReadAttributes)
{
	FString Cursor = Candidate;
	FPaths::NormalizeDirectoryName(Cursor);
	if (Cursor.Len() < 3 || Cursor[1] != TEXT(':') || Cursor[2] != TEXT('/')) return false;
	for (;;)
	{
		const FReplayPathAttributes Info = ReadAttributes(Cursor);
		if (Info.Flags != INVALID_FILE_ATTRIBUTES)
		{
			if ((Info.Flags & FILE_ATTRIBUTE_REPARSE_POINT) != 0) return false;
		}
		else if (Info.Error != ERROR_FILE_NOT_FOUND && Info.Error != ERROR_PATH_NOT_FOUND)
		{
			return false;
		}
		if (Cursor.Len() == 3) break; // The drive root itself was checked.
		const FString Parent = FPaths::GetPath(Cursor);
		if (Parent.IsEmpty() || Parent.Equals(Cursor, ESearchCase::IgnoreCase)) return false;
		Cursor = Parent.Len() == 2 && Parent[1] == TEXT(':') ? Parent + TEXT("/") : Parent;
		FPaths::NormalizeDirectoryName(Cursor);
	}
	return true;
}

static bool ReplayPhysicalDirectory(const FString& Directory, FString& Physical)
{
	FString Native = Directory;
	FPaths::MakePlatformFilename(Native);
	HANDLE Handle = CreateFileW(*Native, FILE_READ_ATTRIBUTES,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS, nullptr);
	if (Handle == INVALID_HANDLE_VALUE) return false;
	const DWORD Required = GetFinalPathNameByHandleW(Handle, nullptr, 0, FILE_NAME_NORMALIZED);
	TArray<WCHAR> Buffer;
	if (Required > 0 && Required < 32768) Buffer.SetNum(Required + 1);
	const DWORD Written = Buffer.IsEmpty() ? 0 : GetFinalPathNameByHandleW(
		Handle, Buffer.GetData(), Buffer.Num(), FILE_NAME_NORMALIZED);
	const bool bClosed = CloseHandle(Handle) != 0;
	if (!bClosed || Written == 0 || Written >= uint32(Buffer.Num())) return false;
	Physical = FString(Buffer.GetData());
	if (!Physical.StartsWith(TEXT("\\\\?\\"), ESearchCase::CaseSensitive)
		|| Physical.StartsWith(TEXT("\\\\?\\UNC\\"), ESearchCase::CaseSensitive)) return false;
	Physical.RightChopInline(4);
	Physical.ReplaceInline(TEXT("\\"), TEXT("/"));
	FPaths::NormalizeDirectoryName(Physical);
	return true;
}

static bool ReplayConfinedResultDirectory(const FString& SavedRoot,
	const FString& ResultParent, const FString& Batch)
{
	if (!ReplayNoReparseAncestor(Batch, ReplayReadAttributes)
		|| !ReplayNoReparseAncestor(ResultParent, ReplayReadAttributes)) return false;
	FString SavedPhysical, ParentPhysical, BatchPhysical;
	return ReplayPhysicalDirectory(SavedRoot, SavedPhysical)
		&& ReplayPhysicalDirectory(ResultParent, ParentPhysical)
		&& ReplayPhysicalDirectory(Batch, BatchPhysical)
		&& FPaths::IsUnderDirectory(ParentPhysical, SavedPhysical)
		&& FPaths::GetPath(BatchPhysical).Equals(ParentPhysical, ESearchCase::IgnoreCase);
}

static FReplayPathAttributes ReplayInjectedReparse(const FString& Path)
{
	return {Path.EndsWith(TEXT("/Results"), ESearchCase::IgnoreCase)
		? uint32(FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)
		: uint32(FILE_ATTRIBUTE_DIRECTORY), ERROR_SUCCESS};
}
static FReplayPathAttributes ReplayInjectedOrdinary(const FString&)
{
	return {FILE_ATTRIBUTE_DIRECTORY, ERROR_SUCCESS};
}
static FReplayPathAttributes ReplayInjectedUnreadable(const FString& Path)
{
	return Path.EndsWith(TEXT("/Results"), ESearchCase::IgnoreCase)
		? FReplayPathAttributes{INVALID_FILE_ATTRIBUTES, ERROR_ACCESS_DENIED}
		: FReplayPathAttributes{FILE_ATTRIBUTE_DIRECTORY, ERROR_SUCCESS};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokPowerReplayReparsePathGateTest,
	"Rok.Ui.Animation.PowerGain.ResultPathRejectsReparseAncestor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokPowerReplayReparsePathGateTest::RunTest(const FString&)
{
	TestFalse(TEXT("Injected Results junction is rejected without creating one"),
		ReplayNoReparseAncestor(TEXT("D:/ueDemo/Rok/Saved/PowerGain/Results/Fresh"), ReplayInjectedReparse));
	TestFalse(TEXT("Unreadable ancestor fails closed"),
		ReplayNoReparseAncestor(TEXT("D:/ueDemo/Rok/Saved/PowerGain/Results/Fresh"), ReplayInjectedUnreadable));
	TestTrue(TEXT("Ordinary ancestors are permitted"),
		ReplayNoReparseAncestor(TEXT("D:/ueDemo/Rok/Saved/PowerGain/Results/Fresh"), ReplayInjectedOrdinary));
	return true;
}
#endif

// This background is the Unity probe's synthetic test pattern, not a city HUD.
class SRokPowerReplayBands final : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SRokPowerReplayBands) {} SLATE_END_ARGS()
	void Construct(const FArguments&) {}
	bool bColored = false;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(640, 360); }
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
		FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle&, bool) const override
	{
		const FLinearColor Gray(.035f, .035f, .035f, 1);
		const FLinearColor Palette[] = {
			FLinearColor(.08f, .19f, .27f, 1), FLinearColor(.29f, .16f, .06f, 1), FLinearColor(.12f, .31f, .17f, 1)};
		const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
		for (int32 Y = 0; Y < 6; ++Y)
			for (int32 X = 0; X < 4; ++X)
				FSlateDrawElement::MakeBox(Elements, Layer,
					Geometry.ToPaintGeometry(FVector2f(160, 60), FSlateLayoutTransform(FVector2f(160.0f * X, 60.0f * Y))),
					White, ESlateDrawEffect::None, bColored ? Palette[(X + 5 - Y) % 3] : Gray);
		return Layer;
	}
};

// JSON numbers must stay finite; preserve abnormal values as diagnostic strings.
static TSharedPtr<FJsonValue> ReplaySafeNumber(double Value)
{
	if (FMath::IsFinite(Value)) return MakeShared<FJsonValueNumber>(Value);
	return MakeShared<FJsonValueString>(FMath::IsNaN(Value) ? TEXT("NaN")
		: Value < 0 ? TEXT("-Infinity") : TEXT("+Infinity"));
}

// The reviewed closure pins the exact fresh Unity input, native report and 112 images.
static bool ReplaySha1Matches(const FString& Path, const FString& Expected)
{
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *Path) || Bytes.IsEmpty()
        || uint64(Bytes.Num()) > MAX_uint32) return false;
    return FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num()).ToString()
        .Equals(Expected, ESearchCase::IgnoreCase);
}

// GetParticles exposes frame-local slots, not stable particle identities.
struct FReplayParticleAgeAudit
{
	bool bExactCount = false;
	bool bNoPendingBirth = false;
	bool bFiniteAges = true;
	bool bRawIndexAligned = false;
	bool bSortedAligned = false;
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();

	FReplayParticleAgeAudit(const TArray<TSharedPtr<FJsonValue>>& SourceRemaining,
		const TArray<float>& UEAges, const TArray<bool>& PendingBirth)
	{
		bExactCount = SourceRemaining.Num() == UEAges.Num() && PendingBirth.Num() == UEAges.Num();
		bNoPendingBirth = !PendingBirth.Contains(true);
		TArray<float> SourceAges;
		TArray<TSharedPtr<FJsonValue>> SourceRemainingValues, SourceAgeValues, UEAgeValues;
		TArray<TSharedPtr<FJsonValue>> RawDifferences, SortedDifferences;
		TArray<TSharedPtr<FJsonValue>> InvalidIndices;
		for (int32 Index = 0; Index < SourceRemaining.Num(); ++Index)
		{
			const TSharedPtr<FJsonValue>& Remaining = SourceRemaining[Index];
			double Raw = 0;
			const bool bNumber = Remaining.IsValid() && Remaining->Type == EJson::Number
				&& Remaining->TryGetNumber(Raw);
			const float AsFloat = float(Raw);
			const float Age = 1.f - AsFloat;
			const bool bValid = bNumber && FMath::IsFinite(Raw)
				&& FMath::IsFinite(AsFloat) && FMath::IsFinite(Age);
			bFiniteAges &= bValid;
			if (!bValid) InvalidIndices.Add(MakeShared<FJsonValueString>(
				FString::Printf(TEXT("sourceRemaining[%d]"), Index)));
			if (bNumber) SourceRemainingValues.Add(ReplaySafeNumber(Raw));
			else SourceRemainingValues.Add(MakeShared<FJsonValueString>(TEXT("invalid JSON number")));
			SourceAges.Add(Age);
			if (bNumber) SourceAgeValues.Add(ReplaySafeNumber(Age));
			else SourceAgeValues.Add(MakeShared<FJsonValueString>(TEXT("invalid derived age")));
		}
		for (int32 Index = 0; Index < UEAges.Num(); ++Index)
		{
			const float Age = UEAges[Index];
			if (!FMath::IsFinite(Age))
			{
				bFiniteAges = false;
				InvalidIndices.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("ueAge[%d]"), Index)));
			}
			UEAgeValues.Add(ReplaySafeNumber(Age));
		}
		Json->SetArrayField(TEXT("sourceRemainingRaw"), MoveTemp(SourceRemainingValues));
		Json->SetArrayField(TEXT("sourceAgesRaw"), MoveTemp(SourceAgeValues));
		Json->SetArrayField(TEXT("ueAgesRaw"), MoveTemp(UEAgeValues));
		Json->SetArrayField(TEXT("invalidIndices"), MoveTemp(InvalidIndices));
		Json->SetStringField(TEXT("invalidState"), !bExactCount ? TEXT("particle count")
			: !bNoPendingBirth ? TEXT("particle pending birth")
			: !bFiniteAges ? TEXT("particle non-finite age or remaining") : TEXT(""));
		bRawIndexAligned = bExactCount && bNoPendingBirth && bFiniteAges;
		bSortedAligned = bRawIndexAligned;
		float RawMaximum = 0.f, SortedMaximum = 0.f;
		TArray<TSharedPtr<FJsonValue>> SortedSourceValues, SortedUEValues;
		if (bRawIndexAligned)
		{
			TArray<float> SortedSource = SourceAges, SortedUE = UEAges;
			SortedSource.Sort();
			SortedUE.Sort();
			for (float Age : SortedSource) SortedSourceValues.Add(ReplaySafeNumber(Age));
			for (float Age : SortedUE) SortedUEValues.Add(ReplaySafeNumber(Age));
			for (int32 Index = 0; Index < UEAges.Num(); ++Index)
			{
				const float Raw = FMath::Abs(UEAges[Index] - SourceAges[Index]);
				const float Sorted = FMath::Abs(SortedUE[Index] - SortedSource[Index]);
				RawMaximum = FMath::Max(RawMaximum, Raw);
				SortedMaximum = FMath::Max(SortedMaximum, Sorted);
				RawDifferences.Add(MakeShared<FJsonValueNumber>(Raw));
				SortedDifferences.Add(MakeShared<FJsonValueNumber>(Sorted));
				bRawIndexAligned &= FMath::IsNearlyEqual(UEAges[Index], SourceAges[Index], .001f);
				bSortedAligned &= FMath::IsNearlyEqual(SortedUE[Index], SortedSource[Index], .001f);
			}
		}
		Json->SetBoolField(TEXT("exactCount"), bExactCount);
		Json->SetBoolField(TEXT("noPendingBirth"), bNoPendingBirth);
		Json->SetBoolField(TEXT("finiteAges"), bFiniteAges);
		Json->SetArrayField(TEXT("rawIndexAbsDifferences"), MoveTemp(RawDifferences));
		Json->SetNumberField(TEXT("rawIndexMaxAbsDifference"), RawMaximum);
		Json->SetArrayField(TEXT("sourceAgesSorted"), MoveTemp(SortedSourceValues));
		Json->SetArrayField(TEXT("ueAgesSorted"), MoveTemp(SortedUEValues));
		Json->SetArrayField(TEXT("sortedAbsDifferences"), MoveTemp(SortedDifferences));
		Json->SetNumberField(TEXT("sortedMaxAbsDifference"), SortedMaximum);
		Json->SetBoolField(TEXT("rawIndexAligned"), bRawIndexAligned);
		Json->SetBoolField(TEXT("sortedAligned"), bSortedAligned);
		Json->SetBoolField(TEXT("orderOnlyDifference"),
			bExactCount && bNoPendingBirth && bFiniteAges && !bRawIndexAligned && bSortedAligned);
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokPowerGainReplayOffscreenTest,
	"Rok.Ui.Animation.PowerGain.ReplayOffscreenRHIClockV7",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokPowerGainReplayOffscreenTest::RunTest(const FString&)
{
	// Deliberately opt in: ordinary editor automation must not silently certify visual parity.
	if (!FParse::Param(FCommandLine::Get(), TEXT("RokPowerReplayV7Clock")))
	{
		AddInfo(TEXT("Replay offscreen clock diagnostic requires -RokPowerReplayV7Clock"));
		return true;
	}
	if (!FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly")))
	{ AddError(TEXT("V7 clock audit requires -RokLocalOnly")); return false; }
	if (!FParse::Param(FCommandLine::Get(), TEXT("RenderOffScreen"))
		|| !FSlateApplication::IsInitialized()
		|| !FSlateApplication::Get().IsRenderingOffScreen())
	{ AddError(TEXT("V7 clock audit requires actual Slate -RenderOffScreen")); return false; }
	if (GUsingNullRHI || !FApp::CanEverRender() || !FSlateApplication::IsInitialized()
		|| FSlateApplication::Get().GetRenderer() == nullptr)
	{
		AddError(TEXT("Real RHI and initialized Slate renderer are required"));
		return false;
	}

	FString ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPaths::NormalizeDirectoryName(ProjectRoot);
	if (!ProjectRoot.Equals(TEXT("D:/ueDemo/Rok/Saved/UIB1"), ESearchCase::IgnoreCase))
	{ AddError(TEXT("V7 clock audit requires the isolated UIB1 project")); return false; }
	FString SourceRoot;
	FString RequestedRoot;
	const bool bRootFlagPresent = FCString::Strifind(FCommandLine::Get(), TEXT("RokPowerReplaySourceRoot")) != nullptr;
	const bool bRootParsed = FParse::Value(FCommandLine::Get(), TEXT("RokPowerReplaySourceRoot="), RequestedRoot);
	if (!bRootFlagPresent || !bRootParsed || RequestedRoot.IsEmpty())
	{ AddError(TEXT("V7 requires explicit pinned -RokPowerReplaySourceRoot")); return false; }
	SourceRoot = FPaths::ConvertRelativePathToFull(RequestedRoot);
	FPaths::NormalizeDirectoryName(SourceRoot);
	if (!SourceRoot.Equals(TEXT("D:/ueDemo/Rok"), ESearchCase::IgnoreCase))
	{ AddError(TEXT("RokPowerReplaySourceRoot must identify the pinned read-only live source root")); return false; }

	const FString ClosurePath = SourceRoot / TEXT("Saved/PendingPowerReplayFirstFrameClockAnalysisV1/source_closure.json");
	if (!ReplaySha1Matches(ClosurePath,
		TEXT("e62ae198523fcbe1790badb0e76b84784b377993")))
	{
		AddError(TEXT("Reviewed V7 source clock closure SHA-1 differs"));
		return false;
	}
	FString ClosureJson;
	TSharedPtr<FJsonObject> Closure;
	if (!FFileHelper::LoadFileToString(ClosureJson, *ClosurePath)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ClosureJson), Closure) || !Closure.IsValid()
		|| Closure->GetStringField(TEXT("schema")) != TEXT("power_replay_v7_first_frame_clock_source_closure")
		|| Closure->GetStringField(TEXT("replayInputSha256"))
			!= TEXT("695724174d78ce43c85408b54b8b8750745632680834d482d8f2bb553bacb2a5")
		|| Closure->GetStringField(TEXT("nativeReportSha256"))
			!= TEXT("4564a2df2e6cda9e18db8db7170e972922320c8e300aa902002148198a51d15a")
		|| Closure->GetStringField(TEXT("v6RootReceiptSha256"))
			!= TEXT("3a3781cd1de45ca2406ff2008429b3f407064d5d2baaaaffb90c8151e66f6a1e")
		|| Closure->GetStringField(TEXT("v6IndependentReviewSha256"))
			!= TEXT("86c22cdb096ec5a910808d3bcebcdcaf446a1a23867fe62c99bbcd9ac176a48d")
		|| Closure->GetStringField(TEXT("v6IndependentReadySha256"))
			!= TEXT("823a925e7769349d2fe694135cc843772882bdd752a6ad86a65cca9b97fa245e")
		|| Closure->GetStringField(TEXT("previousV6StateSha256"))
			!= TEXT("212c81a4b70f5027a141fa6fe0d29cb457217fb2908d793b2f0b37c66069ea9a")
		|| Closure->GetStringField(TEXT("v5RootReceiptSha256"))
			!= TEXT("053cae91ac56a36862e5b4c4e60535e211fbccb8f22751e82ea0ed9d18e6fb80")
		|| Closure->GetStringField(TEXT("v5IndependentReviewSha256"))
			!= TEXT("8b6fb5d4bdc7de244f73df18dc0a999ba12deed582530ea35603902fb7a0ced4")
		|| Closure->GetStringField(TEXT("v5RootReceiptSha1"))
			!= TEXT("3c06bd04e8b40ceba7ef82b81fbc0826b5631b8b")
		|| Closure->GetStringField(TEXT("v5IndependentReviewSha1"))
			!= TEXT("014a7975d45bd86588397bd13003f6adfa4871cc"))
	{
		AddError(TEXT("Reviewed V7 fresh source closure missing or redirected"));
		return false;
	}
	if (Closure->GetStringField(TEXT("replayInputSha1")) != TEXT("481af2f56f9455d40e6d9ce144399e8ed759740c")
		|| Closure->GetStringField(TEXT("nativeReportSha1")) != TEXT("a53cb4f8edb65a0453de03db21b604e525686775")
		|| Closure->GetStringField(TEXT("previousV6StateSha1")) != TEXT("6c03883033ef7a71ec5e4689f5bbeb26d0d62570"))
    { AddError(TEXT("Reviewed V7 Core SHA-1 binding changed")); return false; }
    const FString ResultRelative = Closure->GetStringField(TEXT("resultRelativePath"));
	if (ResultRelative != TEXT("Saved/RokUnityMigration/RootPowerUnityHistory_20261011T040923203212Z/Results/20261011T040924111Z-259ad73d72e04a589ca9e603725aae82"))
	{
		AddError(TEXT("V7 source result directory differs from the reviewed V6 fresh Play output"));
		return false;
	}
	const FString SourceFolder = SourceRoot / ResultRelative;
	const FString InputPath = SourceFolder / TEXT("replay_input.json");
	const FString Fixture = SourceFolder / TEXT("native_replay_visual.json");
	if (!ReplaySha1Matches(InputPath, Closure->GetStringField(TEXT("replayInputSha1")))
		|| !ReplaySha1Matches(Fixture, Closure->GetStringField(TEXT("nativeReportSha1"))))
	{
		AddError(TEXT("Fresh replay input or native report SHA-1 differs from closure"));
		return false;
	}
	const FString PreviousV6State = SourceRoot / Closure->GetStringField(TEXT("previousV6StateRelativePath"));
	if (!ReplaySha1Matches(PreviousV6State, Closure->GetStringField(TEXT("previousV6StateSha1"))))
	{ AddError(TEXT("Pinned V6 actual state baseline differs")); return false; }
	const FString RootV6Receipt = SourceRoot / Closure->GetStringField(TEXT("v6RootReceiptRelativePath"));
	const FString V6IndependentReview = SourceRoot / Closure->GetStringField(TEXT("v6IndependentReviewRelativePath"));
	const FString V6IndependentReady = SourceRoot / Closure->GetStringField(TEXT("v6IndependentReadyRelativePath"));
	if (!ReplaySha1Matches(RootV6Receipt, Closure->GetStringField(TEXT("v6RootReceiptSha1")))
		|| !ReplaySha1Matches(V6IndependentReview, Closure->GetStringField(TEXT("v6IndependentReviewSha1")))
		|| !ReplaySha1Matches(V6IndependentReady, Closure->GetStringField(TEXT("v6IndependentReadySha1"))))
	{ AddError(TEXT("Pinned V6 actual receipt or independent review differs")); return false; }
	const FString RootV5Receipt = SourceRoot / Closure->GetStringField(TEXT("v5RootReceiptRelativePath"));
	const FString V5IndependentReview = SourceRoot / Closure->GetStringField(TEXT("v5IndependentReviewRelativePath"));
	if (!ReplaySha1Matches(RootV5Receipt, Closure->GetStringField(TEXT("v5RootReceiptSha1")))
		|| !ReplaySha1Matches(V5IndependentReview, Closure->GetStringField(TEXT("v5IndependentReviewSha1"))))
	{ AddError(TEXT("Pinned V5 actual receipt or independent review differs")); return false; }
	FString InputJson, SourceJson;
	TSharedPtr<FJsonObject> Input, Source;
	if (!FFileHelper::LoadFileToString(InputJson, *InputPath)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(InputJson), Input) || !Input.IsValid()
		|| !FFileHelper::LoadFileToString(SourceJson, *Fixture)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(SourceJson), Source) || !Source.IsValid())
	{
		AddError(TEXT("Reviewed fresh Unity input or native report missing or invalid"));
		return false;
	}
	if (Input->GetStringField(TEXT("schema")) != TEXT("power_replay_fresh_play_input_v6")
		|| Input->GetStringField(TEXT("source_sha256")) != Closure->GetStringField(TEXT("nativeReportSha256"))
		|| Input->GetBoolField(TEXT("exact_particle_birth_accepted"))
		|| Input->GetArrayField(TEXT("runs")).Num() != 14
		|| Source->GetStringField(TEXT("unityVersion")) != TEXT("2019.4.28f1c1")
		|| Source->GetIntegerField(TEXT("randomSeed")) != 10010
		|| Source->GetArrayField(TEXT("failures")).Num() != 0
		|| Source->GetArrayField(TEXT("frames")).Num() != 4523
		|| Source->GetArrayField(TEXT("captures")).Num() != 14
		|| Source->GetArrayField(TEXT("history")).IsEmpty())
	{
		AddError(TEXT("Fresh Unity source shape or first-history contract changed"));
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>& NativeFrames = Source->GetArrayField(TEXT("frames"));
	const TArray<TSharedPtr<FJsonValue>>& History = Source->GetArrayField(TEXT("history"));
	const TArray<TSharedPtr<FJsonValue>>& Captures = Source->GetArrayField(TEXT("captures"));
	const TArray<TSharedPtr<FJsonValue>>& InputRuns = Input->GetArrayField(TEXT("runs"));
	TArray<TSharedPtr<FJsonValue>> Frames;
	Frames.Reserve(NativeFrames.Num());
	int32 NativeCursor = 0;
	int32 SourcePairs = 0;
	for (int32 Run = 0; Run < 14; ++Run)
	{
		const TSharedPtr<FJsonObject> RunInput = InputRuns[Run]->AsObject();
		if (!RunInput.IsValid() || RunInput->GetIntegerField(TEXT("run")) != Run
			|| RunInput->GetBoolField(TEXT("particleExactBirthAccepted")))
		{ AddError(FString::Printf(TEXT("Fresh run identity/birth contract invalid %d"), Run)); return false; }
		const TArray<TSharedPtr<FJsonValue>>& Ops = RunInput->GetArrayField(TEXT("operations"));
		if (Ops.Num() < 5 || Ops[0]->AsObject()->GetStringField(TEXT("kind")) != TEXT("freshPlay"))
		{ AddError(FString::Printf(TEXT("Fresh Play origin absent %d"), Run)); return false; }
		const TSharedPtr<FJsonObject> Fresh = Ops[0]->AsObject();
		const TSharedPtr<FJsonValue> AfterInstantiate = Fresh->TryGetField(TEXT("afterInstantiate"));
		if (!AfterInstantiate.IsValid() || !AfterInstantiate->AsObject().IsValid()
			|| AfterInstantiate->AsObject()->GetStringField(TEXT("operation"))
				!= TEXT("after source Instantiate before first natural Update")
			|| Fresh->GetIntegerField(TEXT("frame")) != AfterInstantiate->AsObject()->GetIntegerField(TEXT("frame"))
			|| !FMath::IsFinite(Fresh->GetNumberField(TEXT("unityTime")))
			|| Fresh->GetNumberField(TEXT("unityTime")) != AfterInstantiate->AsObject()->GetNumberField(TEXT("unityTime")))
		{ AddError(FString::Printf(TEXT("Pre-Play creation origin invalid %d"), Run)); return false; }
		int32 OriginIndex = -1, AfterIndex = -1, LiveIndex = -1, StartIndex = -1;
		int32 UpdateIndex = -1, NormalIndex = -1, OriginCount = 0, AfterCount = 0;
		const TSharedPtr<FJsonValue>* HistoryAfter = nullptr;
		for (int32 H = 0; H < History.Num(); ++H)
		{
			const TSharedPtr<FJsonObject> Observation = History[H]->AsObject();
			if (!Observation.IsValid() || Observation->GetIntegerField(TEXT("run")) != Run) continue;
			const FString Phase = Observation->GetStringField(TEXT("operation"));
			if (Phase == TEXT("before source Instantiate / fresh Play origin")) { OriginIndex = H; ++OriginCount; }
			else if (Phase == TEXT("after source Instantiate before first natural Update"))
				{ AfterIndex = H; HistoryAfter = &History[H]; ++AfterCount; }
			else if (Phase == TEXT("AutoDestory.live first observed after source Start")) LiveIndex = H;
			else if (Phase == TEXT("runner Start after source Start")) StartIndex = H;
			else if (Phase == TEXT("natural Update after source Update") && UpdateIndex < 0) UpdateIndex = H;
			else if (Phase == TEXT("normal LateUpdate") && NormalIndex < 0) NormalIndex = H;
		}
		if (OriginCount != 1 || AfterCount != 1 || AfterIndex <= OriginIndex
			|| LiveIndex <= AfterIndex
			|| UpdateIndex <= AfterIndex || NormalIndex <= UpdateIndex
			|| !HistoryAfter || !FJsonValue::CompareEqual(**HistoryAfter, *AfterInstantiate)
			|| FMath::Abs(History[LiveIndex]->AsObject()->GetNumberField(TEXT("autoDestroyLive"))
				- Fresh->GetNumberField(TEXT("unityTime"))) > .001
			|| (Run == 0 && !(AfterIndex < LiveIndex && LiveIndex < StartIndex && StartIndex < UpdateIndex)))
		{ AddError(FString::Printf(TEXT("Natural Awake/Start/Update history invalid %d"), Run)); return false; }
		int32 ReplayBefore = 0, ReplayAfter = 0;
		bool bFirstNormal = true;
		for (int32 O = 1; O < Ops.Num(); ++O)
		{
			const TSharedPtr<FJsonObject> Op = Ops[O]->AsObject();
			if (!Op.IsValid() || NativeCursor >= NativeFrames.Num())
			{ AddError(TEXT("Fresh operation exceeds native frames")); return false; }
			const FString Kind = Op->GetStringField(TEXT("kind"));
			const FString ExpectedOperation = Kind == TEXT("Advance") ? TEXT("normal LateUpdate")
				: Kind == TEXT("sourceBeforeReplay") ? TEXT("before source replay")
				: Kind == TEXT("PlayReplay") ? TEXT("after source replay; animator pending real tick") : TEXT("");
			const TSharedPtr<FJsonValue> State = Op->TryGetField(TEXT("sourceState"));
			const TSharedPtr<FJsonObject> Native = NativeFrames[NativeCursor]->AsObject();
			if (ExpectedOperation.IsEmpty() || !State.IsValid() || !State->AsObject().IsValid()
				|| !Native.IsValid() || Native->GetIntegerField(TEXT("run")) != Run
				|| Native->GetStringField(TEXT("operation")) != ExpectedOperation
				|| !FJsonValue::CompareEqual(*State, *NativeFrames[NativeCursor]))
			{ AddError(FString::Printf(TEXT("Fresh/native full frame mapping differs %d/%d"), Run, O)); return false; }
			if (Kind == TEXT("Advance"))
			{
				if (bFirstNormal)
				{
					bFirstNormal = false;
					const int32 FirstFrame = Native->GetIntegerField(TEXT("frame"));
					const int32 OriginFrame = AfterInstantiate->AsObject()->GetIntegerField(TEXT("frame"));
					if (FirstFrame > OriginFrame + 1
						|| FirstFrame != History[UpdateIndex]->AsObject()->GetIntegerField(TEXT("frame"))
						|| FirstFrame != History[NormalIndex]->AsObject()->GetIntegerField(TEXT("frame"))
						|| !FMath::IsFinite(Native->GetNumberField(TEXT("delta")))
						|| Native->GetNumberField(TEXT("delta")) <= 0
						|| Op->GetNumberField(TEXT("delta")) != Native->GetNumberField(TEXT("delta")))
					{ AddError(FString::Printf(TEXT("First natural Advance skipped/changed %d"), Run)); return false; }
				}
			}
			else if (Kind == TEXT("sourceBeforeReplay")) ++ReplayBefore;
			else ++ReplayAfter;
			Frames.Add(NativeFrames[NativeCursor++]);
		}
		if (bFirstNormal || ReplayBefore != 2 || ReplayAfter != 2)
		{ AddError(FString::Printf(TEXT("Fresh run replay count invalid %d"), Run)); return false; }
		SourcePairs += ReplayAfter;
	}
	if (NativeCursor != NativeFrames.Num() || Frames.Num() != NativeFrames.Num() || SourcePairs != 28)
	{ AddError(TEXT("Fresh input does not cover every native frame and replay pair")); return false; }

	const TArray<TSharedPtr<FJsonValue>>& ImagePins = Closure->GetArrayField(TEXT("images"));
	const TCHAR* Layers[] = { TEXT("mesh"), TEXT("particles"), TEXT("combined"), TEXT("background") };
	if (ImagePins.Num() != 112)
	{ AddError(TEXT("Fresh closure must pin all 112 native images")); return false; }
	for (int32 Run = 0; Run < 14; ++Run)
	{
		const TSharedPtr<FJsonObject> Capture = Captures[Run]->AsObject();
		if (!Capture.IsValid() || Capture->GetObjectField(TEXT("state"))->GetIntegerField(TEXT("run")) != Run
			|| Capture->GetArrayField(TEXT("images")).Num() != 8)
		{ AddError(FString::Printf(TEXT("Invalid fresh capture/layer inventory %d"), Run)); return false; }
		for (int32 Colored = 0; Colored < 2; ++Colored)
			for (int32 Layer = 0; Layer < 4; ++Layer)
			{
				const int32 ImageIndex = Colored * 4 + Layer;
				const int32 PinIndex = Run * 8 + ImageIndex;
				const FString Name = FString::Printf(TEXT("%02d_%s_%s.png"), Run,
					Colored ? TEXT("colored") : TEXT("gray"), Layers[Layer]);
				const TSharedPtr<FJsonObject> Pin = ImagePins[PinIndex]->AsObject();
				const FString Expected = SourceFolder / Name;
				FString Recorded = Capture->GetArrayField(TEXT("images"))[ImageIndex]->AsString();
				Recorded.ReplaceInline(TEXT("\\"), TEXT("/"));
				FString NormalizedExpected = Expected;
				NormalizedExpected.ReplaceInline(TEXT("\\"), TEXT("/"));
				if (!Pin.IsValid() || Pin->GetStringField(TEXT("name")) != Name
					|| Pin->GetIntegerField(TEXT("bytes")) != IFileManager::Get().FileSize(*Expected)
					|| !Recorded.Equals(NormalizedExpected, ESearchCase::IgnoreCase)
					|| !ReplaySha1Matches(Expected, Pin->GetStringField(TEXT("sha1"))))
				{
					AddError(FString::Printf(TEXT("Missing/changed fresh Unity image %d/%d"), Run, ImageIndex));
					return false;
				}
			}
	}

	FString SavedRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	FPaths::NormalizeDirectoryName(SavedRoot);
	if (!SavedRoot.Equals(ProjectRoot / TEXT("Saved"), ESearchCase::IgnoreCase))
	{ AddError(TEXT("V7 results must remain in the isolated project's Saved directory")); return false; }
	FString ResultParent = SavedRoot / TEXT("RokUnityMigration/PowerGain/ReplayOffscreenV7/Results");
	FPaths::NormalizeDirectoryName(ResultParent);
	FString RequestedBatch;
	if (!FParse::Value(FCommandLine::Get(), TEXT("RokPowerReplayV7ResultDir="), RequestedBatch)
		|| RequestedBatch.IsEmpty() || FPaths::IsRelative(RequestedBatch))
	{ AddError(TEXT("V7 requires an explicit absolute fresh Shadow Saved result directory")); return false; }
	FString Batch = FPaths::ConvertRelativePathToFull(RequestedBatch);
	FPaths::NormalizeDirectoryName(Batch);
	if (!FPaths::IsUnderDirectory(Batch, ResultParent)
		|| !FPaths::GetPath(Batch).Equals(ResultParent, ESearchCase::IgnoreCase))
	{ AddError(TEXT("V7 result directory must be one fresh direct child under isolated Shadow Saved")); return false; }
	#if PLATFORM_WINDOWS
	if (!ReplayNoReparseAncestor(SavedRoot, ReplayReadAttributes)
		|| !ReplayNoReparseAncestor(ResultParent, ReplayReadAttributes)
		|| !ReplayNoReparseAncestor(Batch, ReplayReadAttributes))
	{ AddError(TEXT("V8 output ancestor is a reparse point or has an unreadable attribute")); return false; }
	#else
	{ AddError(TEXT("V8 output confinement requires Windows attribute checks")); return false; }
	#endif
	if (IFileManager::Get().DirectoryExists(*Batch) || !IFileManager::Get().MakeDirectory(*Batch, true))
	{
		AddError(TEXT("Cannot create unique offscreen result directory"));
		return false;
	}
	#if PLATFORM_WINDOWS
	if (!ReplayConfinedResultDirectory(SavedRoot, ResultParent, Batch))
	{ AddError(TEXT("V8 created output directory is not physically inside Shadow Saved")); return false; }
	#endif
	AddInfo(FString::Printf(TEXT("Diagnostic output: %s"), *Batch));

	FWidgetRenderer Renderer(false, true);
	if (!Renderer.GetSlateRenderer())
	{
		AddError(TEXT("FWidgetRenderer did not create a Slate RHI renderer"));
		return false;
	}
	UTextureRenderTarget2D* Target = FWidgetRenderer::CreateTargetFor(FVector2D(640, 360), TF_Nearest, false);
	if (!Target || !Target->GameThread_GetRenderTargetResource())
	{
		AddError(TEXT("Offscreen RHI render target unavailable"));
		return false;
	}
	Target->AddToRoot();
	TArray<TSharedPtr<FJsonValue>> Results;
	int32 Cursor = 0;
	int32 TotalReplayPairs = 0;
	bool bSuccess = true;
	int32 MismatchRuns = 0;
	for (int32 Run = 0; Run < 14; ++Run)
	{
		const int32 RunStartCursor = Cursor;
		const TSharedPtr<FJsonObject> Unity = Captures[Run]->AsObject()->GetObjectField(TEXT("state"));
		const int32 CaptureFrame = Unity->GetIntegerField(TEXT("frame"));
		const double FreshOriginClock = InputRuns[Run]->AsObject()->GetArrayField(TEXT("operations"))[0]
			->AsObject()->GetNumberField(TEXT("unityTime"));
		URokPowerGainWidget* Effect = NewObject<URokPowerGainWidget>();
		Effect->AddToRoot();
		const TSharedRef<SRokPowerReplayBands> Bands = SNew(SRokPowerReplayBands);
		const TSharedRef<SOverlay> Composition = SNew(SOverlay)
			+ SOverlay::Slot()[Bands]
			+ SOverlay::Slot()[Effect->TakeWidget()];
		if (!Effect->PlayAtClock(FreshOriginClock))
		{
			AddError(FString::Printf(TEXT("Real source widget PlayAtClock failed at run %d"), Run));
			Effect->RemoveFromRoot(); bSuccess = false; break;
		}
		int32 LastFrame = -1, ReplayCount = 0;
		bool bCaptured = false, bRunValid = true, bInvalidParticleAgeSeen = false;
		double LastClockInput = FreshOriginClock;
		TArray<TSharedPtr<FJsonValue>> ClockInputs;
		auto AdvanceSourceClock = [&](const TSharedPtr<FJsonObject>& Frame) -> bool
		{
			const double Absolute = Frame->GetNumberField(TEXT("unityTime"));
			const double ParticleDelta = Frame->GetNumberField(TEXT("delta"));
			if (!FMath::IsFinite(Absolute) || !FMath::IsFinite(ParticleDelta)
				|| ParticleDelta <= 0 || Absolute < LastClockInput
				|| !Effect->AdvanceAtClock(float(ParticleDelta), Absolute)) return false;
			TSharedRef<FJsonObject> InputClock = MakeShared<FJsonObject>();
			InputClock->SetNumberField(TEXT("frame"), Frame->GetNumberField(TEXT("frame")));
			InputClock->SetNumberField(TEXT("absoluteScaledClock"), Absolute);
			InputClock->SetNumberField(TEXT("clockDelta"), Absolute - LastClockInput);
			InputClock->SetNumberField(TEXT("particleFrameDelta"), ParticleDelta);
			ClockInputs.Add(MakeShared<FJsonValueObject>(InputClock));
			LastClockInput = Absolute;
			return true;
		};
		TArray<TSharedPtr<FJsonValue>> ReplayEvents;
		TSharedPtr<FJsonObject> PendingNextNormal;
		TSharedPtr<FJsonObject> PendingSecondNormal;
		TSharedRef<FJsonObject> FirstRecordedFrame = MakeShared<FJsonObject>();
		bool bInitialPhaseAligned = true;
		bool bReplayPhaseAligned = true;
		auto MaterialU = [&]() { return Effect->Materials[0]->K2_GetScalarParameterValue(TEXT("UTiling")); };
		for (; Cursor < Frames.Num(); ++Cursor)
		{
			const TSharedPtr<FJsonObject> Frame = Frames[Cursor]->AsObject();
			const int32 FrameRun = Frame->GetIntegerField(TEXT("run"));
			if (FrameRun != Run) break;
			const int32 FrameId = Frame->GetIntegerField(TEXT("frame"));
			const FString Operation = Frame->GetStringField(TEXT("operation"));
			if (Operation == TEXT("normal LateUpdate"))
			{
				const float Delta = Frame->GetNumberField(TEXT("delta"));
				if (FrameId <= LastFrame || Delta <= 0 || Delta > 1 || !FMath::IsFinite(Delta))
				{ bRunValid = false; break; }
				if (!AdvanceSourceClock(Frame)) { bRunValid = false; break; }
				if (PendingSecondNormal.IsValid())
				{
					PendingSecondNormal->SetNumberField(TEXT("secondNormalFrame"), FrameId);
					PendingSecondNormal->SetNumberField(TEXT("sourceSecondNormalized"), Frame->GetNumberField(TEXT("animatorNormalized")));
					PendingSecondNormal->SetNumberField(TEXT("sourceSecondUTiling"), Frame->GetNumberField(TEXT("uTiling")));
					PendingSecondNormal->SetArrayField(TEXT("sourceSecondParticleRemaining"), Frame->GetArrayField(TEXT("remaining")));
					PendingSecondNormal->SetNumberField(TEXT("ueSecondMeshTime"), Effect->GetPlaybackTime());
					const float UeSecondU = MaterialU();
					PendingSecondNormal->SetNumberField(TEXT("ueSecondMaterialUTiling"), UeSecondU);
					const bool bSecondUAligned = FMath::IsNearlyEqual(UeSecondU,
						float(Frame->GetNumberField(TEXT("uTiling"))), .0001f);
					PendingSecondNormal->SetBoolField(TEXT("secondNormalUAligned"), bSecondUAligned);
					bReplayPhaseAligned &= bSecondUAligned;
					TArray<float> SecondAges; TArray<bool> SecondPending;
					Effect->GetParticleStateForAudit(SecondAges, SecondPending);
					TArray<TSharedPtr<FJsonValue>> SecondParticles;
					for (int32 P = 0; P < SecondAges.Num(); ++P)
					{
						TSharedRef<FJsonObject> Particle = MakeShared<FJsonObject>();
						bInvalidParticleAgeSeen |= !FMath::IsFinite(SecondAges[P]);
						Particle->SetField(TEXT("age"), ReplaySafeNumber(SecondAges[P]));
						Particle->SetBoolField(TEXT("pendingBirth"), SecondPending[P]);
						SecondParticles.Add(MakeShared<FJsonValueObject>(Particle));
					}
					PendingSecondNormal->SetArrayField(TEXT("ueSecondParticles"), MoveTemp(SecondParticles));
					PendingSecondNormal.Reset();
				}
				if (LastFrame < 0)
				{
					const float SourceMeshSeconds = .85f * float(Frame->GetNumberField(TEXT("animatorNormalized")));
					TArray<float> FirstAges; TArray<bool> FirstPending;
					Effect->GetParticleStateForAudit(FirstAges, FirstPending);
					const TArray<TSharedPtr<FJsonValue>>& FirstRemaining = Frame->GetArrayField(TEXT("remaining"));
					FirstRecordedFrame->SetNumberField(TEXT("frame"), FrameId);
					FirstRecordedFrame->SetNumberField(TEXT("sourceElapsed"), Frame->GetNumberField(TEXT("elapsed")));
					FirstRecordedFrame->SetNumberField(TEXT("sourceDelta"), Delta);
					FirstRecordedFrame->SetNumberField(TEXT("freshOriginAbsoluteClock"), FreshOriginClock);
					FirstRecordedFrame->SetNumberField(TEXT("sourceAbsoluteClock"), Frame->GetNumberField(TEXT("unityTime")));
					FirstRecordedFrame->SetNumberField(TEXT("sourceClockDeltaFromOrigin"),
						Frame->GetNumberField(TEXT("unityTime")) - FreshOriginClock);
					FirstRecordedFrame->SetNumberField(TEXT("sourceMeshSecondsFromNormalized"), SourceMeshSeconds);
					FirstRecordedFrame->SetNumberField(TEXT("ueMeshTime"), Effect->GetPlaybackTime());
					FirstRecordedFrame->SetNumberField(TEXT("ueMaterialUTiling"), MaterialU());
					FirstRecordedFrame->SetArrayField(TEXT("sourceParticleRemaining"), FirstRemaining);
					TArray<TSharedPtr<FJsonValue>> FirstParticles;
					for (int32 P = 0; P < FirstAges.Num(); ++P)
					{
						TSharedRef<FJsonObject> Particle = MakeShared<FJsonObject>();
						bInvalidParticleAgeSeen |= !FMath::IsFinite(FirstAges[P]);
						Particle->SetField(TEXT("age"), ReplaySafeNumber(FirstAges[P]));
						Particle->SetBoolField(TEXT("pendingBirth"), FirstPending[P]);
						FirstParticles.Add(MakeShared<FJsonValueObject>(Particle));
					}
					FirstRecordedFrame->SetArrayField(TEXT("ueParticles"), MoveTemp(FirstParticles));
					const FReplayParticleAgeAudit FirstAgeAudit(FirstRemaining, FirstAges, FirstPending);
					bInvalidParticleAgeSeen |= !FirstAgeAudit.bFiniteAges;
					FirstRecordedFrame->SetObjectField(TEXT("particleAgeAudit"), FirstAgeAudit.Json);
					bInitialPhaseAligned = FMath::IsNearlyEqual(SourceMeshSeconds, Effect->GetPlaybackTime(), .001f)
						&& FMath::IsNearlyEqual(MaterialU(), float(Frame->GetNumberField(TEXT("uTiling"))), .0001f)
						&& FirstAgeAudit.bSortedAligned;
				}
				LastFrame = FrameId;
				if (PendingNextNormal.IsValid())
				{
					PendingNextNormal->SetNumberField(TEXT("nextNormalFrame"), FrameId);
					PendingNextNormal->SetNumberField(TEXT("sourceNextNormalized"), Frame->GetNumberField(TEXT("animatorNormalized")));
					PendingNextNormal->SetNumberField(TEXT("sourceNextUTiling"), Frame->GetNumberField(TEXT("uTiling")));
					PendingNextNormal->SetArrayField(TEXT("sourceNextParticleRemaining"), Frame->GetArrayField(TEXT("remaining")));
					PendingNextNormal->SetNumberField(TEXT("ueNextMeshTime"), Effect->GetPlaybackTime());
					const float UeNextU = MaterialU();
					PendingNextNormal->SetNumberField(TEXT("ueNextMaterialUTiling"), UeNextU);
					const bool bNextUAligned = FMath::IsNearlyEqual(UeNextU,
						float(Frame->GetNumberField(TEXT("uTiling"))), .0001f);
					PendingNextNormal->SetBoolField(TEXT("nextNormalUAligned"), bNextUAligned);
					bReplayPhaseAligned &= bNextUAligned;
					TArray<float> NextAges; TArray<bool> NextPending;
					Effect->GetParticleStateForAudit(NextAges, NextPending);
					TArray<TSharedPtr<FJsonValue>> NextParticles;
					for (int32 P = 0; P < NextAges.Num(); ++P)
					{
						TSharedRef<FJsonObject> Particle = MakeShared<FJsonObject>();
						bInvalidParticleAgeSeen |= !FMath::IsFinite(NextAges[P]);
						Particle->SetField(TEXT("age"), ReplaySafeNumber(NextAges[P]));
						Particle->SetBoolField(TEXT("pendingBirth"), NextPending[P]);
						NextParticles.Add(MakeShared<FJsonValueObject>(Particle));
					}
					PendingNextNormal->SetArrayField(TEXT("ueNextParticles"), MoveTemp(NextParticles));
					PendingSecondNormal = PendingNextNormal;
					PendingNextNormal.Reset();
				}
			}
			else if (Operation == TEXT("before source replay"))
			{
				if (FrameId != LastFrame || ReplayCount >= 2) { bRunValid = false; break; }
				const float BeforeMeshTime = Effect->GetPlaybackTime();
				const float BeforeMaterialU = MaterialU();
				const float BeforeRemaining = Effect->GetRemainingTime();
				TArray<float> BeforeAges; TArray<bool> BeforePending;
				Effect->GetParticleStateForAudit(BeforeAges, BeforePending);
				if (++Cursor >= Frames.Num() || Frames[Cursor]->AsObject()->GetIntegerField(TEXT("frame")) != FrameId
					|| Frames[Cursor]->AsObject()->GetStringField(TEXT("operation")) != TEXT("after source replay; animator pending real tick")
					|| !Effect->PlayAtClock(Frames[Cursor]->AsObject()->GetNumberField(TEXT("unityTime"))))
				{ bRunValid = false; break; }
				const TSharedPtr<FJsonObject> After = Frames[Cursor]->AsObject();
				LastClockInput = After->GetNumberField(TEXT("unityTime"));
				TSharedRef<FJsonObject> Event = MakeShared<FJsonObject>();
				Event->SetNumberField(TEXT("frame"), FrameId);
				Event->SetNumberField(TEXT("sourceBeforeNormalized"), Frame->GetNumberField(TEXT("animatorNormalized")));
				Event->SetNumberField(TEXT("sourceBeforeUTiling"), Frame->GetNumberField(TEXT("uTiling")));
				Event->SetArrayField(TEXT("sourceBeforeParticleRemaining"), Frame->GetArrayField(TEXT("remaining")));
				Event->SetNumberField(TEXT("ueBeforeMeshTime"), BeforeMeshTime);
				Event->SetNumberField(TEXT("ueBeforeMaterialUTiling"), BeforeMaterialU);
				const bool bBeforeUAligned = FMath::IsNearlyEqual(BeforeMaterialU,
					float(Frame->GetNumberField(TEXT("uTiling"))), .0001f);
				Event->SetBoolField(TEXT("beforeUAligned"), bBeforeUAligned);
				bReplayPhaseAligned &= bBeforeUAligned;
				Event->SetNumberField(TEXT("ueBeforeRemaining"), BeforeRemaining);
				TArray<TSharedPtr<FJsonValue>> BeforeParticles;
				for (int32 P = 0; P < BeforeAges.Num(); ++P)
				{
					TSharedRef<FJsonObject> Particle = MakeShared<FJsonObject>();
					bInvalidParticleAgeSeen |= !FMath::IsFinite(BeforeAges[P]);
					Particle->SetField(TEXT("age"), ReplaySafeNumber(BeforeAges[P]));
					Particle->SetBoolField(TEXT("pendingBirth"), BeforePending[P]);
					BeforeParticles.Add(MakeShared<FJsonValueObject>(Particle));
				}
				Event->SetArrayField(TEXT("ueBeforeParticles"), MoveTemp(BeforeParticles));
				Event->SetNumberField(TEXT("sourceElapsed"), After->GetNumberField(TEXT("elapsed")));
				Event->SetNumberField(TEXT("replayAbsoluteClock"), LastClockInput);
				Event->SetNumberField(TEXT("sourceRootLifetimeAfter"), After->GetNumberField(TEXT("rootLifetime")));
				Event->SetNumberField(TEXT("sourceUTilingAfter"), After->GetNumberField(TEXT("uTiling")));
				Event->SetArrayField(TEXT("sourceParticleRemainingAfter"), After->GetArrayField(TEXT("remaining")));
				Event->SetNumberField(TEXT("ueMeshTimeAfter"), Effect->GetPlaybackTime());
				const float UeAfterU = MaterialU();
				Event->SetNumberField(TEXT("ueMaterialUTilingAfter"), UeAfterU);
				const bool bSameFrameUAligned = FMath::IsNearlyEqual(UeAfterU,
					float(After->GetNumberField(TEXT("uTiling"))), .0001f);
				Event->SetBoolField(TEXT("sameFrameUAligned"), bSameFrameUAligned);
				bReplayPhaseAligned &= bSameFrameUAligned;
				Event->SetNumberField(TEXT("ueRemainingAfter"), Effect->GetRemainingTime());
				TArray<float> EventAges; TArray<bool> EventPending;
				Effect->GetParticleStateForAudit(EventAges, EventPending);
				TArray<TSharedPtr<FJsonValue>> EventParticles;
				for (int32 P = 0; P < EventAges.Num(); ++P)
				{
					TSharedRef<FJsonObject> Particle = MakeShared<FJsonObject>();
					bInvalidParticleAgeSeen |= !FMath::IsFinite(EventAges[P]);
					Particle->SetField(TEXT("age"), ReplaySafeNumber(EventAges[P]));
					Particle->SetBoolField(TEXT("pendingBirth"), EventPending[P]);
					EventParticles.Add(MakeShared<FJsonValueObject>(Particle));
				}
				Event->SetArrayField(TEXT("ueParticlesAfter"), MoveTemp(EventParticles));
				ReplayEvents.Add(MakeShared<FJsonValueObject>(Event));
				PendingNextNormal = Event;
				++ReplayCount;
			}
			else { bRunValid = false; break; }
			if (LastFrame == CaptureFrame && (Cursor + 1 == Frames.Num()
				|| Frames[Cursor + 1]->AsObject()->GetIntegerField(TEXT("frame")) != CaptureFrame))
			{
				bCaptured = true;
				break;
			}
		}
		if (!bRunValid || !bCaptured || LastFrame != CaptureFrame || ReplayCount > 2)
		{
			AddError(FString::Printf(TEXT("Fixture operation order/capture frame invalid at run %d"), Run));
			Effect->RemoveFromRoot(); bSuccess = false; break;
		}

		TArray<float> Ages; TArray<bool> Pending;
		Effect->GetParticleStateForAudit(Ages, Pending);
		TSharedRef<FJsonObject> State = MakeShared<FJsonObject>();
		State->SetNumberField(TEXT("run"), Run);
		State->SetNumberField(TEXT("sourceTarget"), Captures[Run]->AsObject()->GetNumberField(TEXT("target")));
		State->SetNumberField(TEXT("freshOriginAbsoluteClock"), FreshOriginClock);
		State->SetNumberField(TEXT("sourceElapsed"), Unity->GetNumberField(TEXT("elapsed")));
		State->SetNumberField(TEXT("sourceFrame"), CaptureFrame);
		State->SetStringField(TEXT("timelineAlignment"), bInitialPhaseAligned
			? TEXT("OBSERVED_ALIGNED_AT_FIRST_RECORDED_FRAME") : TEXT("UNALIGNED_FIRST_RECORDED_FRAME"));
		State->SetBoolField(TEXT("replayAnimatorPhaseAligned"), bReplayPhaseAligned);
		State->SetObjectField(TEXT("firstRecordedFrame"), FirstRecordedFrame);
		State->SetArrayField(TEXT("replayEvents"), MoveTemp(ReplayEvents));
		State->SetNumberField(TEXT("sourceMeshNormalized"), Unity->GetNumberField(TEXT("animatorNormalized")));
		State->SetNumberField(TEXT("sourceUTiling"), Unity->GetNumberField(TEXT("uTiling")));
		State->SetNumberField(TEXT("sourceParticleTime"), Unity->GetNumberField(TEXT("particleTime")));
		State->SetNumberField(TEXT("ueMeshTime"), Effect->GetPlaybackTime());
		State->SetNumberField(TEXT("ueMaterialUTiling"), MaterialU());
		State->SetNumberField(TEXT("ueCurveUTilingForDiagnosis"), URokPowerGainWidget::EvaluateUTiling(Effect->GetPlaybackTime()));
		State->SetNumberField(TEXT("ueRemaining"), Effect->GetRemainingTime());
		State->SetNumberField(TEXT("sourceRootLifetime"), Unity->GetNumberField(TEXT("rootLifetime")));
		State->SetArrayField(TEXT("sourceImages"), Captures[Run]->AsObject()->GetArrayField(TEXT("images")));
		State->SetBoolField(TEXT("sourceRootAlive"), Unity->GetBoolField(TEXT("rootAlive")));
		State->SetBoolField(TEXT("uePlaying"), Effect->IsPlaying());
		TArray<TSharedPtr<FJsonValue>> Particles;
		for (int32 P = 0; P < Ages.Num(); ++P)
		{
			TSharedRef<FJsonObject> Particle = MakeShared<FJsonObject>();
			bInvalidParticleAgeSeen |= !FMath::IsFinite(Ages[P]);
			Particle->SetField(TEXT("age"), ReplaySafeNumber(Ages[P]));
			Particle->SetBoolField(TEXT("pendingBirth"), Pending[P]);
			Particles.Add(MakeShared<FJsonValueObject>(Particle));
		}
		State->SetArrayField(TEXT("ueParticles"), MoveTemp(Particles));
		State->SetArrayField(TEXT("unityParticleRemaining"), Unity->GetArrayField(TEXT("remaining")));
		const TArray<TSharedPtr<FJsonValue>>& SourceRemaining = Unity->GetArrayField(TEXT("remaining"));
		const FReplayParticleAgeAudit CaptureAgeAudit(SourceRemaining, Ages, Pending);
		bInvalidParticleAgeSeen |= !CaptureAgeAudit.bFiniteAges;
		State->SetObjectField(TEXT("particleAgeAudit"), CaptureAgeAudit.Json);
		TArray<TSharedPtr<FJsonValue>> Differences;
		auto Diff = [&](const TCHAR* Text) { Differences.Add(MakeShared<FJsonValueString>(Text)); };
		if (!bInitialPhaseAligned) Diff(TEXT("first natural source mesh or particle phase differs"));
		if (!bReplayPhaseAligned) Diff(TEXT("same-frame or next-normal replay Animator U phase differs"));
		if (Effect->IsPlaying() != Unity->GetBoolField(TEXT("rootAlive"))) Diff(TEXT("root alive"));
		if (!FMath::IsNearlyEqual(Effect->GetRemainingTime(),
			FMath::Max(0.f, float(Unity->GetNumberField(TEXT("rootLifetime"))
				- Unity->GetNumberField(TEXT("elapsed")))), .001f)) Diff(TEXT("root remaining > 1 ms"));
		if (Unity->GetBoolField(TEXT("rootAlive"))
			&& !FMath::IsNearlyEqual(MaterialU(),
				float(Unity->GetNumberField(TEXT("uTiling"))), .0001f)) Diff(TEXT("mesh U tiling > 1e-4"));
		if (!CaptureAgeAudit.bExactCount) Diff(TEXT("particle count"));
		else if (!CaptureAgeAudit.bNoPendingBirth) Diff(TEXT("particle pending birth"));
		else if (!CaptureAgeAudit.bFiniteAges) Diff(TEXT("particle non-finite age or remaining"));
		else if (!CaptureAgeAudit.bSortedAligned) Diff(TEXT("particle age multiset > 1 ms"));
		if (!Differences.IsEmpty()) ++MismatchRuns;
		State->SetArrayField(TEXT("differences"), MoveTemp(Differences));
		TArray<TSharedPtr<FJsonValue>> Images;
		for (int32 Colored = 0; Colored < 2 && bRunValid; ++Colored)
		{
			Bands->bColored = Colored != 0;
			for (int32 Layer = 0; Layer < 4; ++Layer)
			{
				Effect->AuditPaintLayers = static_cast<uint8>(Layer == 0 ? 1 : Layer == 1 ? 2 : Layer == 2 ? 3 : 0);
				// First draw may submit a Slate material permutation. Root enforces an
				// external process timeout for this explicitly opted-in diagnostic.
				Renderer.DrawWidget(Target, Composition, FVector2D(640, 360), 0, false);
				FlushRenderingCommands();
				if (GShaderCompilingManager)
				{
					GShaderCompilingManager->FinishAllCompilation();
					if (GShaderCompilingManager->IsCompiling()) { bRunValid = false; break; }
				}
				Renderer.DrawWidget(Target, Composition, FVector2D(640, 360), 0, false);
				FlushRenderingCommands();
				if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling())
				{ bRunValid = false; break; }
				TArray<FColor> Pixels;
				if (!Target->GameThread_GetRenderTargetResource()->ReadPixels(
					Pixels, FReadSurfaceDataFlags(RCM_UNorm, CubeFace_MAX))
					|| Pixels.Num() != 640 * 360) { bRunValid = false; break; }
				TArray64<uint8> Png;
				FImageUtils::PNGCompressImageArray(640, 360,
					TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
				const FString Name = FString::Printf(TEXT("%02d_%s_%s.png"), Run,
					Colored ? TEXT("colored") : TEXT("gray"), Layers[Layer]);
				#if PLATFORM_WINDOWS
				if (!ReplayConfinedResultDirectory(SavedRoot, ResultParent, Batch)
					|| !ReplayNoReparseAncestor(Batch / Name, ReplayReadAttributes)
					|| IFileManager::Get().FileExists(*(Batch / Name)))
				{ bRunValid = false; break; }
				#endif
				if (Png.IsEmpty() || !FFileHelper::SaveArrayToFile(Png, *(Batch / Name)))
				{ bRunValid = false; break; }
				Images.Add(MakeShared<FJsonValueString>(Name));
			}
		}
		State->SetArrayField(TEXT("images"), MoveTemp(Images));
		// The 8 images and capture-state values above are frozen. Continue the same
		// real object through the rest of this independent source run for full replay coverage.
		TArray<TSharedPtr<FJsonValue>> FullReplayEvents = State->GetArrayField(TEXT("replayEvents"));
		TSharedPtr<FJsonObject> SourceTail;
		auto ParticleSnapshot = [&]()
		{
			TArray<float> SnapshotAges; TArray<bool> SnapshotPending;
			Effect->GetParticleStateForAudit(SnapshotAges, SnapshotPending);
			TArray<TSharedPtr<FJsonValue>> Snapshot;
			for (int32 P = 0; P < SnapshotAges.Num(); ++P)
			{
				TSharedRef<FJsonObject> Particle = MakeShared<FJsonObject>();
				bInvalidParticleAgeSeen |= !FMath::IsFinite(SnapshotAges[P]);
				Particle->SetField(TEXT("age"), ReplaySafeNumber(SnapshotAges[P]));
				Particle->SetBoolField(TEXT("pendingBirth"), SnapshotPending[P]);
				Snapshot.Add(MakeShared<FJsonValueObject>(Particle));
			}
			return Snapshot;
		};
		auto RecordFollowingNormal = [&](const TSharedPtr<FJsonObject>& Event,
			const TSharedPtr<FJsonObject>& Sample, bool bSecond)
		{
			const TCHAR* Prefix = bSecond ? TEXT("Second") : TEXT("Next");
			Event->SetNumberField(FString::Printf(TEXT("%sNormalFrame"), Prefix), Sample->GetNumberField(TEXT("frame")));
			Event->SetNumberField(FString::Printf(TEXT("source%sNormalized"), Prefix), Sample->GetNumberField(TEXT("animatorNormalized")));
			Event->SetNumberField(FString::Printf(TEXT("source%sUTiling"), Prefix), Sample->GetNumberField(TEXT("uTiling")));
			Event->SetArrayField(FString::Printf(TEXT("source%sParticleRemaining"), Prefix), Sample->GetArrayField(TEXT("remaining")));
			Event->SetNumberField(FString::Printf(TEXT("ue%sMeshTime"), Prefix), Effect->GetPlaybackTime());
			const float UeU = MaterialU();
			Event->SetNumberField(FString::Printf(TEXT("ue%sMaterialUTiling"), Prefix), UeU);
			Event->SetArrayField(FString::Printf(TEXT("ue%sParticles"), Prefix), ParticleSnapshot());
			const bool bAligned = FMath::IsNearlyEqual(UeU, float(Sample->GetNumberField(TEXT("uTiling"))), .0001f);
			Event->SetBoolField(bSecond ? TEXT("secondNormalUAligned") : TEXT("nextNormalUAligned"), bAligned);
			bReplayPhaseAligned &= bAligned;
		};
		for (++Cursor; bRunValid && Cursor < Frames.Num(); ++Cursor)
		{
			const TSharedPtr<FJsonObject> Frame = Frames[Cursor]->AsObject();
			if (Frame->GetIntegerField(TEXT("run")) != Run) break;
			const int32 FrameId = Frame->GetIntegerField(TEXT("frame"));
			const FString Operation = Frame->GetStringField(TEXT("operation"));
			if (Operation == TEXT("normal LateUpdate"))
			{
				const float Delta = Frame->GetNumberField(TEXT("delta"));
				if (FrameId <= LastFrame || Delta <= 0 || Delta > 1 || !FMath::IsFinite(Delta))
				{ bRunValid = false; break; }
				if (!AdvanceSourceClock(Frame)) { bRunValid = false; break; }
				LastFrame = FrameId;
				SourceTail = Frame;
				if (PendingSecondNormal.IsValid())
				{
					RecordFollowingNormal(PendingSecondNormal, Frame, true);
					PendingSecondNormal.Reset();
				}
				if (PendingNextNormal.IsValid())
				{
					RecordFollowingNormal(PendingNextNormal, Frame, false);
					PendingSecondNormal = PendingNextNormal;
					PendingNextNormal.Reset();
				}
			}
			else if (Operation == TEXT("before source replay"))
			{
				if (FrameId != LastFrame || ReplayCount >= 2 || Cursor + 1 >= Frames.Num())
				{ bRunValid = false; break; }
				const float BeforeMesh = Effect->GetPlaybackTime();
				const float BeforeU = MaterialU();
				const float BeforeRemaining = Effect->GetRemainingTime();
				const TArray<TSharedPtr<FJsonValue>> BeforeParticles = ParticleSnapshot();
				const TSharedPtr<FJsonObject> After = Frames[++Cursor]->AsObject();
				if (After->GetIntegerField(TEXT("run")) != Run || After->GetIntegerField(TEXT("frame")) != FrameId
					|| After->GetStringField(TEXT("operation")) != TEXT("after source replay; animator pending real tick")
					|| !Effect->PlayAtClock(After->GetNumberField(TEXT("unityTime"))))
				{ bRunValid = false; break; }
				LastClockInput = After->GetNumberField(TEXT("unityTime"));
				TSharedRef<FJsonObject> Event = MakeShared<FJsonObject>();
				Event->SetNumberField(TEXT("frame"), FrameId);
				Event->SetNumberField(TEXT("sourceElapsed"), After->GetNumberField(TEXT("elapsed")));
				Event->SetNumberField(TEXT("replayAbsoluteClock"), LastClockInput);
				Event->SetNumberField(TEXT("sourceBeforeNormalized"), Frame->GetNumberField(TEXT("animatorNormalized")));
				Event->SetNumberField(TEXT("sourceBeforeUTiling"), Frame->GetNumberField(TEXT("uTiling")));
				Event->SetArrayField(TEXT("sourceBeforeParticleRemaining"), Frame->GetArrayField(TEXT("remaining")));
				Event->SetNumberField(TEXT("ueBeforeMeshTime"), BeforeMesh);
				Event->SetNumberField(TEXT("ueBeforeMaterialUTiling"), BeforeU);
				Event->SetNumberField(TEXT("ueBeforeRemaining"), BeforeRemaining);
				Event->SetArrayField(TEXT("ueBeforeParticles"), BeforeParticles);
				Event->SetNumberField(TEXT("sourceRootLifetimeAfter"), After->GetNumberField(TEXT("rootLifetime")));
				Event->SetNumberField(TEXT("sourceUTilingAfter"), After->GetNumberField(TEXT("uTiling")));
				Event->SetArrayField(TEXT("sourceParticleRemainingAfter"), After->GetArrayField(TEXT("remaining")));
				Event->SetNumberField(TEXT("ueMeshTimeAfter"), Effect->GetPlaybackTime());
				const float UeAfter = MaterialU();
				Event->SetNumberField(TEXT("ueMaterialUTilingAfter"), UeAfter);
				Event->SetNumberField(TEXT("ueRemainingAfter"), Effect->GetRemainingTime());
				Event->SetArrayField(TEXT("ueParticlesAfter"), ParticleSnapshot());
				const bool bBeforeAligned = FMath::IsNearlyEqual(BeforeU,
					float(Frame->GetNumberField(TEXT("uTiling"))), .0001f);
				const bool bAfterAligned = FMath::IsNearlyEqual(UeAfter,
					float(After->GetNumberField(TEXT("uTiling"))), .0001f);
				Event->SetBoolField(TEXT("beforeUAligned"), bBeforeAligned);
				Event->SetBoolField(TEXT("sameFrameUAligned"), bAfterAligned);
				bReplayPhaseAligned &= bBeforeAligned && bAfterAligned;
				FullReplayEvents.Add(MakeShared<FJsonValueObject>(Event));
				PendingNextNormal = Event;
				++ReplayCount;
			}
			else { bRunValid = false; break; }
		}
		State->SetArrayField(TEXT("replayEvents"), MoveTemp(FullReplayEvents));
		State->SetArrayField(TEXT("clockInputs"), MoveTemp(ClockInputs));
		State->SetStringField(TEXT("invalidState"), bInvalidParticleAgeSeen
			? TEXT("particle non-finite age or remaining") : TEXT(""));
		State->SetNumberField(TEXT("fullRunRecordCount"), Cursor - RunStartCursor);
		State->SetNumberField(TEXT("fullRunReplayPairs"), ReplayCount);
		State->SetBoolField(TEXT("replayAnimatorPhaseAligned"), bReplayPhaseAligned);
		if (SourceTail.IsValid())
		{
			State->SetNumberField(TEXT("sourceTailFrame"), SourceTail->GetNumberField(TEXT("frame")));
			State->SetNumberField(TEXT("sourceTailElapsed"), SourceTail->GetNumberField(TEXT("elapsed")));
			State->SetBoolField(TEXT("sourceTailRootAlive"), SourceTail->GetBoolField(TEXT("rootAlive")));
			State->SetBoolField(TEXT("ueTailPlaying"), Effect->IsPlaying());
			State->SetNumberField(TEXT("ueTailRemaining"), Effect->GetRemainingTime());
		}
		if (ReplayCount != 2 || PendingNextNormal.IsValid() || PendingSecondNormal.IsValid()
			|| !SourceTail.IsValid() || SourceTail->GetNumberField(TEXT("elapsed")) < 3.08
			|| SourceTail->GetBoolField(TEXT("rootAlive")))
			bRunValid = false;
		else
		{
			TotalReplayPairs += ReplayCount;
			TArray<TSharedPtr<FJsonValue>> RunDifferences = State->GetArrayField(TEXT("differences"));
			const bool bHadDifferences = !RunDifferences.IsEmpty();
			if (!bReplayPhaseAligned)
				RunDifferences.Add(MakeShared<FJsonValueString>(TEXT("full-run replay Animator U phase differs")));
			if (bInvalidParticleAgeSeen)
				RunDifferences.Add(MakeShared<FJsonValueString>(TEXT("particle non-finite age or remaining")));
			if (SourceTail->GetBoolField(TEXT("rootAlive")) != Effect->IsPlaying())
				RunDifferences.Add(MakeShared<FJsonValueString>(TEXT("full-run root destruction tail differs")));
			if (!bHadDifferences && !RunDifferences.IsEmpty()) ++MismatchRuns;
			State->SetArrayField(TEXT("differences"), MoveTemp(RunDifferences));
		}
		Effect->RemoveFromRoot();
		if (!bRunValid)
		{
			AddError(FString::Printf(TEXT("RHI capture or full source replay failed at run %d"), Run));
			bSuccess = false; break;
		}
		Results.Add(MakeShared<FJsonValueObject>(State));
	}
	Target->RemoveFromRoot();
	TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetStringField(TEXT("schema"), TEXT("power_replay_v7_dual_clock_rhi.v1"));
	Report->SetStringField(TEXT("scope"), TEXT("Deterministic Unity absolute Time.time plus particle frame delta through real UE widget and RHI; no real HUD tick or visual acceptance"));
	Report->SetStringField(TEXT("clockContract"), TEXT("root/Animator: absolute scaled source clock; particle: recorded frame delta"));
	Report->SetStringField(TEXT("sourceFixture"), ResultRelative / TEXT("native_replay_visual.json"));
	Report->SetStringField(TEXT("freshReplayInput"), ResultRelative / TEXT("replay_input.json"));
	Report->SetStringField(TEXT("sourceClosureSha256"), TEXT("7f6296a614b5f7ed05a8e18e66fb830ca6975cd0656bd4a45fe72401b13fa818"));
	Report->SetStringField(TEXT("previousV6State"), Closure->GetStringField(TEXT("previousV6StateRelativePath")));
	Report->SetStringField(TEXT("previousV6StateSha256"), Closure->GetStringField(TEXT("previousV6StateSha256")));
	Report->SetStringField(TEXT("sourceRoot"), SourceRoot);
	Report->SetStringField(TEXT("projectRoot"), ProjectRoot);
	Report->SetStringField(TEXT("projectSavedRoot"), SavedRoot);
	Report->SetStringField(TEXT("resultDirectory"), Batch);
	Report->SetArrayField(TEXT("runs"), MoveTemp(Results));
	Report->SetBoolField(TEXT("captureComplete"), bSuccess && Report->GetArrayField(TEXT("runs")).Num() == 14);
	Report->SetNumberField(TEXT("stateMismatchRuns"), MismatchRuns);
	Report->SetNumberField(TEXT("fullRunReplayPairs"), TotalReplayPairs);
	FString Json;
	FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
	#if PLATFORM_WINDOWS
	if (!ReplayConfinedResultDirectory(SavedRoot, ResultParent, Batch)
		|| !ReplayNoReparseAncestor(Batch / TEXT("replay_state.json"), ReplayReadAttributes)
		|| IFileManager::Get().FileExists(*(Batch / TEXT("replay_state.json"))))
	{ AddError(TEXT("V8 replay state output path is redirected or already exists")); return false; }
	#endif
	if (!FFileHelper::SaveStringToFile(Json, *(Batch / TEXT("replay_state.json"))))
	{ AddError(TEXT("Cannot write replay state manifest")); return false; }
	if (MismatchRuns > 0)
		AddError(FString::Printf(TEXT("Unity/UE timeline state differs in %d of 14 runs; see replay_state.json"), MismatchRuns));
	if (TotalReplayPairs != 28) AddError(TEXT("Expected all 28 native replay pairs across 14 full runs"));
	return bSuccess && TotalReplayPairs == 28 && MismatchRuns == 0;
}
#endif
