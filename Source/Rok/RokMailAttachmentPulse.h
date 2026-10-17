#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"

namespace RokMailAttachmentPulse
{
    // UI_Item_MailBtn/img_bag legacy Animation: UA_Item_HotTask_bag.
    // The transparent parent scales its visible child; no alpha/rotation tracks.
    constexpr double Duration = 2.0;
    inline FVector2D ChinesePivot() { return FVector2D(1.0, 0.0); }
    inline float Evaluate(double Seconds)
    {
        if (!FMath::IsFinite(Seconds) || Seconds <= 0.0) return 1.0f;
        const double Time = FMath::Fmod(Seconds, Duration);
        constexpr double Times[] = {0.0, 0.2, 0.36666667, 0.56666666, 0.75, 2.0};
        constexpr double Values[] = {1.0, 1.15, 1.0, 1.15, 1.0, 1.0};
        for (int32 Index = 1; Index < UE_ARRAY_COUNT(Times); ++Index)
        {
            if (Time <= Times[Index])
            {
                const double T = (Time - Times[Index - 1]) / (Times[Index] - Times[Index - 1]);
                // All original tangents are zero: exact unweighted cubic Hermite.
                const double Smooth = T * T * (3.0 - 2.0 * T);
                return static_cast<float>(FMath::Lerp(Values[Index - 1], Values[Index], Smooth));
            }
        }
        return 1.0f;
    }
}

// Owned by the screen, never by an authority/inventory subsystem. Register only
// a source-visible attachment parent. Rebuilding a still-visible row preserves
// its phase; removing then reactivating it creates a fresh legacy autoplay.
class FRokMailAttachmentPulse
{
public:
    void BeginRebuild()
    {
        for (auto& Pair : Rows)
        {
            Pair.Value.bRegistered = false;
            Pair.Value.Target.Reset();
        }
    }
    void Register(int64 MailIdentity, UWidget* Parent)
    {
        check(Parent);
        FRow& Row = Rows.FindOrAdd(MailIdentity);
        Row.Target = Parent;
        Row.bRegistered = true;
        Parent->SetRenderTransformPivot(RokMailAttachmentPulse::ChinesePivot());
        Apply(Row);
    }
    void EndRebuild()
    {
        for (auto It = Rows.CreateIterator(); It; ++It)
            if (!It.Value().bRegistered || !It.Value().Target.IsValid()) It.RemoveCurrent();
    }
    void Advance(double DeltaSeconds)
    {
        if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0) return;
        for (auto It = Rows.CreateIterator(); It; ++It)
        {
            FRow& Row = It.Value();
            if (!Row.Target.IsValid()) { It.RemoveCurrent(); continue; }
            Row.Seconds = FMath::Fmod(Row.Seconds + DeltaSeconds, RokMailAttachmentPulse::Duration);
            Apply(Row);
        }
    }
    void Reset()
    {
        for (auto& Pair : Rows)
            if (UWidget* Target = Pair.Value.Target.Get()) Target->SetRenderScale(FVector2D(1.0));
        Rows.Reset();
    }
    int32 Num() const { return Rows.Num(); }
    double GetSeconds(int64 MailIdentity) const
    {
        const FRow* Row = Rows.Find(MailIdentity);
        return Row ? Row->Seconds : 0.0;
    }

private:
    struct FRow { TWeakObjectPtr<UWidget> Target; double Seconds = 0.0; bool bRegistered = false; };
    TMap<int64, FRow> Rows;
    static void Apply(FRow& Row)
    {
        if (UWidget* Target = Row.Target.Get())
        {
            const float Scale = RokMailAttachmentPulse::Evaluate(Row.Seconds);
            Target->SetRenderScale(FVector2D(Scale));
        }
    }
};
