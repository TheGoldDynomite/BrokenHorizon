#include "BHPlaytestTelemetrySubsystem.h"

#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
constexpr int32 MaxTelemetryFields = 16;
constexpr int32 MaxTelemetryValueLength = 96;

FString SerializeTelemetryEvent(
    const FString& SessionId,
    const double ElapsedSeconds,
    const FString& MapName,
    const FString& NetMode,
    const FName EventName,
    const TMap<FString, FString>& Fields
)
{
    const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetNumberField(TEXT("schema"), 1);
    Root->SetStringField(TEXT("timestampUtc"), FDateTime::UtcNow().ToIso8601());
    Root->SetStringField(TEXT("session"), SessionId);
    Root->SetNumberField(TEXT("elapsedSeconds"), FMath::Max(0.0, ElapsedSeconds));
    Root->SetStringField(TEXT("map"), UBHPlaytestTelemetrySubsystem::SanitizeToken(MapName));
    Root->SetStringField(TEXT("netMode"), UBHPlaytestTelemetrySubsystem::SanitizeToken(NetMode));
    Root->SetStringField(TEXT("event"), UBHPlaytestTelemetrySubsystem::SanitizeToken(EventName.ToString()));

    const TSharedRef<FJsonObject> FieldObject = MakeShared<FJsonObject>();
    int32 AddedFields = 0;
    for (const TPair<FString, FString>& Field : Fields)
    {
        if (AddedFields >= MaxTelemetryFields)
        {
            break;
        }

        const FString Key = UBHPlaytestTelemetrySubsystem::SanitizeToken(Field.Key);
        if (Key.IsEmpty())
        {
            continue;
        }

        FieldObject->SetStringField(
            Key,
            UBHPlaytestTelemetrySubsystem::SanitizeToken(
                Field.Value,
                MaxTelemetryValueLength
            )
        );
        ++AddedFields;
    }
    Root->SetObjectField(TEXT("fields"), FieldObject);

    FString Json;
    const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
    FJsonSerializer::Serialize(Root, Writer);
    return Json;
}
}

void UBHPlaytestTelemetrySubsystem::Initialize(
    FSubsystemCollectionBase& Collection
)
{
    Super::Initialize(Collection);

#if !UE_BUILD_SHIPPING
    bEnabled = FParse::Param(
        FCommandLine::Get(),
        TEXT("BHPlaytestTelemetry")
    );
    if (!bEnabled)
    {
        return;
    }

    SessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    SessionStartSeconds = FPlatformTime::Seconds();
    const FString Directory = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("Telemetry")
    );
    IFileManager::Get().MakeDirectory(*Directory, true);
    OutputPath = FPaths::Combine(
        Directory,
        FString::Printf(
            TEXT("BHPlaytest-%s-%s.jsonl"),
            *FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S")),
            *SessionId.Left(8)
        )
    );
    RecordEvent(TEXT("session_start"), {
        {TEXT("build"), LexToString(FApp::GetBuildConfiguration())}
    });
    UE_LOG(
        LogTemp,
        Display,
        TEXT("BH_PLAYTEST_TELEMETRY_READY path=%s"),
        *OutputPath
    );
#endif
}

void UBHPlaytestTelemetrySubsystem::Deinitialize()
{
    if (bEnabled)
    {
        RecordEvent(TEXT("session_end"));
    }
    bEnabled = false;
    Super::Deinitialize();
}

bool UBHPlaytestTelemetrySubsystem::IsTelemetryEnabled() const
{
    return bEnabled;
}

void UBHPlaytestTelemetrySubsystem::RecordEvent(
    const FName EventName,
    const TMap<FString, FString>& Fields
)
{
#if !UE_BUILD_SHIPPING
    if (!bEnabled || EventName.IsNone() || OutputPath.IsEmpty())
    {
        return;
    }

    const FString Line = BuildEventJson(EventName, Fields) + LINE_TERMINATOR;
    if (!FFileHelper::SaveStringToFile(
        Line,
        *OutputPath,
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
        &IFileManager::Get(),
        FILEWRITE_Append
    ))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("BH_PLAYTEST_TELEMETRY_WRITE_FAILED event=%s"),
            *EventName.ToString()
        );
    }
#endif
}

FString UBHPlaytestTelemetrySubsystem::SanitizeToken(
    const FString& Value,
    const int32 MaxLength
)
{
    FString Result;
    Result.Reserve(FMath::Min(Value.Len(), MaxLength));
    for (const TCHAR Character : Value)
    {
        if (Result.Len() >= FMath::Max(0, MaxLength))
        {
            break;
        }
        if (FChar::IsAlnum(Character) || Character == TEXT('_') ||
            Character == TEXT('-') || Character == TEXT('.') ||
            Character == TEXT('/') || Character == TEXT(':'))
        {
            Result.AppendChar(Character);
        }
        else if (FChar::IsWhitespace(Character))
        {
            Result.AppendChar(TEXT('_'));
        }
    }
    return Result;
}

FString UBHPlaytestTelemetrySubsystem::BuildEventJsonForTesting(
    const FName EventName,
    const TMap<FString, FString>& Fields
)
{
    return SerializeTelemetryEvent(
        TEXT("TESTSESSION"),
        12.5,
        TEXT("/Game/TestMap"),
        TEXT("Standalone"),
        EventName,
        Fields
    );
}

FString UBHPlaytestTelemetrySubsystem::BuildEventJson(
    const FName EventName,
    const TMap<FString, FString>& Fields
) const
{
    const UWorld* World = GetWorld();
    FString NetMode = TEXT("Unknown");
    if (IsValid(World))
    {
        switch (World->GetNetMode())
        {
        case NM_Standalone: NetMode = TEXT("Standalone"); break;
        case NM_DedicatedServer: NetMode = TEXT("DedicatedServer"); break;
        case NM_ListenServer: NetMode = TEXT("ListenServer"); break;
        case NM_Client: NetMode = TEXT("Client"); break;
        default: break;
        }
    }
    return SerializeTelemetryEvent(
        SessionId,
        FPlatformTime::Seconds() - SessionStartSeconds,
        IsValid(World) ? World->GetMapName() : FString(),
        NetMode,
        EventName,
        Fields
    );
}
