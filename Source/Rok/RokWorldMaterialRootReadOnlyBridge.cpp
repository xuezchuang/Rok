#include "RokWorldMaterialRootReadOnlyBridge.h"

#include "Dom/JsonObject.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
FString SerializeJson(const TSharedRef<FJsonObject>& Object)
{
    FString Json;
    if (!FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Json)))
    {
        return FString();
    }
    return Json;
}

FString Failure(const TCHAR* Reason)
{
    const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("schema"), TEXT("rok.world_material_hidden_roots_native.v1"));
    Result->SetBoolField(TEXT("ok"), false);
    Result->SetStringField(TEXT("error"), Reason);
    return SerializeJson(Result);
}

#if WITH_EDITOR && WITH_EDITORONLY_DATA
FString OutputName(const FExpressionOutput& Output)
{
    if (!Output.OutputName.IsNone())
    {
        return Output.OutputName.ToString();
    }
    if (Output.Mask)
    {
        if (Output.MaskR && !Output.MaskG && !Output.MaskB && !Output.MaskA) return TEXT("R");
        if (!Output.MaskR && Output.MaskG && !Output.MaskB && !Output.MaskA) return TEXT("G");
        if (!Output.MaskR && !Output.MaskG && Output.MaskB && !Output.MaskA) return TEXT("B");
        if (!Output.MaskR && !Output.MaskG && !Output.MaskB && Output.MaskA) return TEXT("A");
    }
    return FString();
}

bool AddRoot(const UMaterial* Material, const FScalarMaterialInput& Input,
             EMaterialProperty Property, const TCHAR* Key,
             const TSharedRef<FJsonObject>& Roots, FString& Error)
{
    FMaterialInputDescription Description;
    // GetExpressionInputForProperty is not used as a substitute for raw data:
    // these two members have no Python MaterialProperty enum entry.
    UMaterial* MutableForRead = const_cast<UMaterial*>(Material);
    if (!MutableForRead->GetExpressionInputDescription(Property, Description)
        || Description.Input != static_cast<const FExpressionInput*>(&Input)
        || Description.bHidden || Description.bUseConstant != static_cast<bool>(Input.UseConstant))
    {
        Error = TEXT("Native material input description differs from editor-only raw root");
        return false;
    }
    const FExpressionInput* PublicInput = MutableForRead->GetExpressionInputForProperty(Property);
    if (PublicInput != static_cast<const FExpressionInput*>(&Input))
    {
        Error = TEXT("Native material property input pointer differs from editor-only raw root");
        return false;
    }
    if (!FMath::IsFinite(Input.Constant))
    {
        Error = TEXT("Hidden scalar root constant is nonfinite");
        return false;
    }

    const UMaterialExpression* Expression = Input.Expression;
    FString NodeName;
    FString NodeClass;
    FString SelectedOutput;
    if (Expression)
    {
        if (Input.OutputIndex < 0 || Input.OutputIndex >= Expression->Outputs.Num())
        {
            Error = TEXT("Connected hidden root output index is outside expression outputs");
            return false;
        }
        NodeName = Expression->GetName();
        NodeClass = Expression->GetClass()->GetPathName();
        SelectedOutput = OutputName(Expression->Outputs[Input.OutputIndex]);
    }

    uint32 ConstantBits = 0;
    FMemory::Memcpy(&ConstantBits, &Input.Constant, sizeof(ConstantBits));
    const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("node"), NodeName);
    Root->SetStringField(TEXT("node_class"), NodeClass);
    Root->SetStringField(TEXT("output_name"), SelectedOutput);
    Root->SetStringField(TEXT("output_index"), FString::FromInt(Input.OutputIndex));
    Root->SetStringField(TEXT("input_name"), Input.InputName.ToString());
    Root->SetStringField(TEXT("mask"), FString::FromInt(Input.Mask));
    Root->SetStringField(TEXT("mask_r"), FString::FromInt(Input.MaskR));
    Root->SetStringField(TEXT("mask_g"), FString::FromInt(Input.MaskG));
    Root->SetStringField(TEXT("mask_b"), FString::FromInt(Input.MaskB));
    Root->SetStringField(TEXT("mask_a"), FString::FromInt(Input.MaskA));
    Root->SetBoolField(TEXT("use_constant"), Input.UseConstant != 0);
    Root->SetStringField(TEXT("constant_bits_hex"), FString::Printf(TEXT("%08x"), ConstantBits));
    Roots->SetObjectField(Key, Root);
    return true;
}
#endif
}

bool URokWorldMaterialRootReadOnlyBridge::IsMaterialPathSupported(const FString& MaterialObjectPath)
{
    return MaterialObjectPath.StartsWith(TEXT("/Game/RokUnityWorldMapPrototype/Materials/"))
        || MaterialObjectPath.StartsWith(TEXT("/Game/RokDiagnostics/WorldStreaming/InstancedMaterialVariantsV1/"))
        || MaterialObjectPath.Equals(TEXT("/Game/RokDiagnostics/WorldStreaming/BridgeInstancedMaterialV1/MIV1_M_WorldSource_build_bridge_A.MIV1_M_WorldSource_build_bridge_A"), ESearchCase::CaseSensitive);
}

FString URokWorldMaterialRootReadOnlyBridge::GetHiddenRootsJson(UMaterial* Material)
{
#if WITH_EDITOR && WITH_EDITORONLY_DATA
    if (!IsInGameThread()) return Failure(TEXT("Game thread required"));
    if (!IsValid(Material) || Material->GetClass() != UMaterial::StaticClass())
    {
        return Failure(TEXT("Exact UMaterial required"));
    }
    const FString Path = Material->GetPathName();
    if (!IsMaterialPathSupported(Path))
    {
        return Failure(TEXT("Material is outside the reviewed source and new-target objects"));
    }
    const UMaterialEditorOnlyData* Data = Material->GetEditorOnlyData();
    if (!Data) return Failure(TEXT("UMaterialEditorOnlyData is null"));

    const TSharedRef<FJsonObject> Roots = MakeShared<FJsonObject>();
    FString Error;
    if (!AddRoot(Material, Data->PixelDepthOffset, MP_PixelDepthOffset,
                 TEXT("MP_PIXEL_DEPTH_OFFSET"), Roots, Error)
        || !AddRoot(Material, Data->Displacement, MP_Displacement,
                    TEXT("MP_DISPLACEMENT"), Roots, Error))
    {
        return Failure(*Error);
    }
    const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("schema"), TEXT("rok.world_material_hidden_roots_native.v1"));
    Result->SetBoolField(TEXT("ok"), true);
    Result->SetStringField(TEXT("material_path"), Path);
    Result->SetObjectField(TEXT("roots"), Roots);
    return SerializeJson(Result);
#else
    return Failure(TEXT("Editor-only material input data unavailable"));
#endif
}
