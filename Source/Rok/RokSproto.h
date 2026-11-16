#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// Runtime sproto codec for the Rok server protocol (Unity Assets/Protocol/Protocol.sproto).
// The schema text is parsed at load time, so a protocol change only needs the new file.
// Wire format, 0-pack and the RPC package header follow skynet's
// lualib-src/sproto/sproto.c (MIT, codingnow.com); values travel as FJsonObject:
//   integer -> number (int64 beyond 2^53 as FJsonValueNumberString), integer(N) -> number,
//   boolean -> bool, string -> string, binary -> base64 string, double -> number,
//   struct -> object, *T / *T(key) -> array (keyed maps stay arrays of objects).
enum class ERokSprotoType : uint8
{
	Integer,
	Boolean,
	String,
	Binary,
	Double,
	Struct
};

struct FRokSprotoField
{
	FString Name;
	int32 Tag = 0;
	ERokSprotoType Type = ERokSprotoType::Integer;
	bool bArray = false;
	int32 StructIndex = INDEX_NONE;
	// integer(N): wire value = value * Decimal.
	int64 Decimal = 0;
	FString MapKey;
};

struct FRokSprotoStruct
{
	FString Name;
	// Sorted by tag.
	TArray<FRokSprotoField> Fields;

	const FRokSprotoField* FindTag(int32 Tag) const;
};

struct FRokSprotoProtocol
{
	FString Name;
	int32 Tag = 0;
	int32 RequestType = INDEX_NONE;
	int32 ResponseType = INDEX_NONE;
};

class ROK_API FRokSprotoSchema
{
public:
	bool Parse(const FString& Text, FString& OutError);
	bool IsValid() const { return Structs.Num() > 0; }

	int32 FindStruct(const FString& Name) const;
	const FRokSprotoStruct* GetStruct(int32 Index) const;
	const FRokSprotoProtocol* FindProtocol(int32 Tag) const;
	const FRokSprotoProtocol* FindProtocol(const FString& Name) const;
	int32 NumStructs() const { return Structs.Num(); }
	int32 NumProtocols() const { return Protocols.Num(); }

	// Raw (unpacked) struct body.
	bool Encode(int32 StructIndex, const FJsonObject& Value, TArray<uint8>& Out, FString& OutError) const;
	// Returns the number of bytes consumed, or -1.
	int32 Decode(int32 StructIndex, const uint8* Data, int32 Size, TSharedPtr<FJsonObject>& Out, FString& OutError) const;

	// sproto_pack / sproto_unpack (0-packing).
	static void Pack(const uint8* Data, int32 Size, TArray<uint8>& Out);
	static bool Unpack(const uint8* Data, int32 Size, TArray<uint8>& Out);

private:
	bool EncodeStruct(const FRokSprotoStruct& Type, const FJsonObject& Value, TArray<uint8>& Out, FString& OutError, int32 Depth) const;
	int32 DecodeStruct(const FRokSprotoStruct& Type, const uint8* Data, int32 Size, TSharedPtr<FJsonObject>& Out, FString& OutError, int32 Depth) const;

	TArray<FRokSprotoStruct> Structs;
	TMap<FString, int32> StructByName;
	TArray<FRokSprotoProtocol> Protocols;
	TMap<int32, int32> ProtocolByTag;
	TMap<FString, int32> ProtocolByName;
};

// One RPC package as SprotoRpc.cs builds it: `.package { type, session }` + body, 0-packed.
struct FRokSprotoMessage
{
	bool bRequest = false;
	int32 Tag = 0;
	bool bHasSession = false;
	int64 Session = 0;
	TSharedPtr<FJsonObject> Body;
};

class ROK_API FRokSprotoRpc
{
public:
	explicit FRokSprotoRpc(const FRokSprotoSchema& InSchema);

	// Packs a request. A wire session can be sent without retaining response state
	// for protocols without a response (e.g. Map_Move inside GateMessage).
	bool EncodeRequest(int32 ProtocolTag, const FJsonObject* Body, bool bWithSession, int64 Session,
		TArray<uint8>& OutPacked, FString& OutError, bool bTrackResponse = true);
	// Unpacks a message. Responses decode with the protocol registered for their session.
	bool Dispatch(const uint8* Packed, int32 Size, FRokSprotoMessage& Out, FString& OutError);

	// Protocol tag a pending response session belongs to (INDEX_NONE when unknown).
	int32 TakePendingTag(int64 Session);
	// Server may omit an outer GateMessage response when its content is empty.
	// Expire optional ACK sessions and return their IDs so callers can drop metadata.
	TArray<int64> ExpirePendingResponses(double NowSeconds, double MaxAgeSeconds);
	int32 NumPendingResponses() const { return PendingSessions.Num(); }

private:
	const FRokSprotoSchema& Schema;
	int32 PackageType = INDEX_NONE;
	TMap<int64, int32> PendingSessions;
	TMap<int64, double> PendingSessionStartTimes;
};

namespace RokSprotoJson
{
	ROK_API bool TryGetInt64(const TSharedPtr<FJsonObject>& Object, const FString& Field, int64& Out);
	ROK_API FString ToString(const TSharedPtr<FJsonObject>& Object, bool bPretty = false);
}
