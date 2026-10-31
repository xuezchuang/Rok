#include "RokSpriteAtlas.h"

#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Engine/Texture2D.h"

namespace
{
	FString RokSpriteRoot()
	{
		return FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::ProjectDir(), TEXT("ref/client-unity/Assets")));
	}
}

int32 FRokSpriteAtlas::LoadAll()
{
	Entries.Reset();
	TextureToSheet.Reset();
	SheetsScanned.Reset();
	Textures.Reset();

	TArray<FString> SheetFiles;
	IFileManager::Get().FindFilesRecursive(SheetFiles, *RokSpriteRoot(), TEXT("*.tpsheet"), true, false);
	SheetFiles.Sort();
	for (const FString& SheetFile : SheetFiles)
	{
		IndexSheet(SheetFile);
	}
	return Entries.Num();
}

void FRokSpriteAtlas::IndexSheet(const FString& SheetFile)
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *SheetFile))
	{
		return;
	}
	FString TextureName;
	int32 SheetWidth = 0;
	int32 SheetHeight = 0;
	TArray<FString> Lines;
	Content.ParseIntoArrayLines(Lines, false);
	for (const FString& Line : Lines)
	{
		if (Line.StartsWith(TEXT(":texture=")))
		{
			TextureName = Line.RightChop(9).TrimStartAndEnd();
		}
		else if (Line.StartsWith(TEXT(":size=")))
		{
			const FString SizeText = Line.RightChop(6).TrimStartAndEnd();
			FString WidthText;
			FString HeightText;
			if (SizeText.Split(TEXT("x"), &WidthText, &HeightText))
			{
				SheetWidth = FCString::Atoi(*WidthText);
				SheetHeight = FCString::Atoi(*HeightText);
			}
		}
	}
	if (TextureName.IsEmpty())
	{
		return;
	}
	const FString SheetPng = FPaths::Combine(FPaths::GetPath(SheetFile), TextureName);
	SheetsScanned.Add(SheetPng);
	TextureToSheet.Add(FPaths::GetBaseFilename(TextureName), SheetPng);

	for (const FString& Line : Lines)
	{
		if (Line.IsEmpty() || Line.StartsWith(TEXT("#")) || Line.StartsWith(TEXT(":")))
		{
			continue;
		}
		TArray<FString> Fields;
		Line.ParseIntoArray(Fields, TEXT(";"), true);
		if (Fields.Num() < 5)
		{
			continue;
		}
		FRokSpriteRect Rect;
		Rect.SheetPng = SheetPng;
		Rect.X = FCString::Atoi(*Fields[1]);
		Rect.Y = FCString::Atoi(*Fields[2]);
		Rect.Width = FCString::Atoi(*Fields[3]);
		Rect.Height = FCString::Atoi(*Fields[4]);
		Rect.SheetWidth = SheetWidth;
		Rect.SheetHeight = SheetHeight;
		if (Rect.Width > 0 && Rect.Height > 0)
		{
			Entries.Add(Fields[0].TrimStartAndEnd(), Rect);
		}
	}
}

bool FRokSpriteAtlas::Resolve(const FString& IconReference, FRokSpriteRect& Out) const
{
	FString Atlas;
	FString Sprite = IconReference.TrimStartAndEnd();
	int32 OpenBracket = INDEX_NONE;
	if (Sprite.FindChar(TEXT('['), OpenBracket))
	{
		Atlas = Sprite.Left(OpenBracket);
		const FString Rest = Sprite.Mid(OpenBracket + 1);
		int32 CloseBracket = INDEX_NONE;
		if (Rest.FindChar(TEXT(']'), CloseBracket))
		{
			Sprite = Rest.Left(CloseBracket);
		}
		else
		{
			Sprite = Rest;
		}
	}
	const FRokSpriteRect* Found = Entries.Find(Sprite);
	if (!Found)
	{
		return false;
	}
	Out = *Found;
	return true;
}

UTexture2D* FRokSpriteAtlas::GetSheetTexture(const FString& SheetPng)
{
	if (TObjectPtr<UTexture2D>* Cached = Textures.Find(SheetPng))
	{
		if (*Cached)
		{
			return *Cached;
		}
	}

	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *SheetPng))
	{
		return nullptr;
	}
	IImageWrapperModule& WrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> Wrapper = WrapperModule.CreateImageWrapper(EImageFormat::PNG);
	UTexture2D* Texture = nullptr;
	if (Wrapper.IsValid() && Wrapper->SetCompressed(Bytes.GetData(), Bytes.Num()))
	{
		TArray64<uint8> Raw;
		const int32 Width = Wrapper->GetWidth();
		const int32 Height = Wrapper->GetHeight();
		if (Wrapper->GetRaw(ERGBFormat::BGRA, 8, Raw) && Width > 0 && Height > 0)
		{
			Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
			if (Texture)
			{
				Texture->SRGB = true;
				Texture->Filter = TF_Bilinear;
				void* MipData = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
				FMemory::Memcpy(MipData, Raw.GetData(), Raw.Num());
				Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
				Texture->UpdateResource();
			}
		}
	}
	Textures.Add(SheetPng, Texture);
	return Texture;
}

void FRokSpriteAtlas::Reset()
{
	Entries.Reset();
	TextureToSheet.Reset();
	SheetsScanned.Reset();
	Textures.Reset();
}
