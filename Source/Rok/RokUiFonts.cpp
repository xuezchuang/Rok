#include "RokUiFonts.h"

#include "Fonts/CompositeFont.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"

TSharedPtr<const FCompositeFont> RokUiFonts::GetEngineDefaultComposite()
{
	return FCoreStyle::GetDefaultFont();
}

TSharedPtr<const FCompositeFont> RokUiFonts::GetUnitySourceComposite()
{
#if PLATFORM_WINDOWS
	// Unity HUD prefabs use Unity's built-in Arial (fileID 10102).
	// Unity 2019 uses Microsoft YaHei for the Chinese fallback.
	static const TSharedPtr<const FCompositeFont> SourceFont = []() -> TSharedPtr<const FCompositeFont>
	{
		const FString Fonts = FPaths::Combine(FPlatformMisc::GetEnvironmentVariable(TEXT("WINDIR")), TEXT("Fonts"));
		const FString Arial = Fonts / TEXT("arial.ttf");
		const FString ArialBold = Fonts / TEXT("arialbd.ttf");
		const FString YaHei = Fonts / TEXT("msyh.ttc");
		const FString YaHeiBold = Fonts / TEXT("msyhbd.ttc");
		IFileManager& Files = IFileManager::Get();
		if (!Files.FileExists(*Arial) || !Files.FileExists(*ArialBold)
			|| !Files.FileExists(*YaHei) || !Files.FileExists(*YaHeiBold))
		{
			return nullptr;
		}

		constexpr EFontHinting Hinting = EFontHinting::Default;
		constexpr EFontLoadingPolicy Loading = EFontLoadingPolicy::LazyLoad;
		auto Composite = MakeShared<FStandaloneCompositeFont>(TEXT("Regular"), Arial, Hinting, Loading);
		Composite->DefaultTypeface.AppendFont(TEXT("Bold"), ArialBold, Hinting, Loading);
		Composite->FallbackTypeface.Typeface.AppendFont(TEXT("Regular"), YaHei, Hinting, Loading);
		Composite->FallbackTypeface.Typeface.AppendFont(TEXT("Bold"), YaHeiBold, Hinting, Loading);
		return Composite;
	}();
	return SourceFont;
#else
	return nullptr;
#endif
}

bool RokUiFonts::Identify(const FSlateFontInfo& Font, ERokUiFontRole& OutRole)
{
	if (Font.FontObject)
	{
		OutRole = ERokUiFontRole::Asset;
		return true;
	}
	if (!Font.CompositeFont.IsValid())
	{
		return false;
	}
	if (Font.CompositeFont == GetEngineDefaultComposite())
	{
		OutRole = ERokUiFontRole::EngineDefault;
		return true;
	}
	const TSharedPtr<const FCompositeFont> Unity = GetUnitySourceComposite();
	if (Unity.IsValid() && Font.CompositeFont == Unity)
	{
		OutRole = ERokUiFontRole::UnitySource;
		return true;
	}
	return false;
}

FSlateFontInfo RokUiFonts::Resolve(ERokUiFontRole Role, const FSlateFontInfo& Serialized)
{
	TSharedPtr<const FCompositeFont> Composite;
	switch (Role)
	{
	case ERokUiFontRole::EngineDefault:
		Composite = GetEngineDefaultComposite();
		break;
	case ERokUiFontRole::UnitySource:
		// Same fallback as MakeUnitySourceFont when the Windows fonts are missing.
		Composite = GetUnitySourceComposite();
		if (!Composite.IsValid()) Composite = GetEngineDefaultComposite();
		break;
	default:
		return Serialized;
	}

	FSlateFontInfo Resolved = Serialized;
	Resolved.FontObject = nullptr;
	Resolved.CompositeFont = Composite;
	return Resolved;
}
