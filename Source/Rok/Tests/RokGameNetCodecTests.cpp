#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonValue.h"
#include "../RokDataPaths.h"
#include "../RokSkynetCrypt.h"
#include "../RokSproto.h"

namespace
{
	TArray<uint8> Hex(const FString& Text)
	{
		TArray<uint8> Out;
		FString Clean = Text.Replace(TEXT(" "), TEXT(""));
		for (int32 I = 0; I + 1 < Clean.Len(); I += 2)
		{
			Out.Add(uint8(FParse::HexDigit(Clean[I]) << 4 | FParse::HexDigit(Clean[I + 1])));
		}
		return Out;
	}

	FString ToHex(const TArray<uint8>& Bytes)
	{
		return BytesToHex(Bytes.GetData(), Bytes.Num());
	}

	// The schema from sproto README.md ("Wire protocol" examples).
	const TCHAR* ReadmeSchema = TEXT(
		".Person {\n name 0 : string\n age 1 : integer\n marital 2 : boolean\n children 3 : *Person\n}\n"
		".Data {\n numbers 0 : *integer\n bools 1 : *boolean\n number 2 : integer\n bignumber 3 : integer\n"
		" fpn 6 : integer(2)\n}\n");

	TSharedPtr<FJsonObject> Person(const FString& Name, int32 Age)
	{
		TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
		Out->SetStringField(TEXT("name"), Name);
		Out->SetNumberField(TEXT("age"), Age);
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokSkynetCryptTest, "Rok.Net.Codec.SkynetCrypt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokSkynetCryptTest::RunTest(const FString&)
{
	// FIPS 46 / classic DES vector: K=133457799BBCDFF1, P=0123456789ABCDEF -> C=85E813540F0AB405.
	const TArray<uint8> Key = Hex(TEXT("133457799BBCDFF1"));
	const TArray<uint8> Plain = Hex(TEXT("0123456789ABCDEF"));
	TArray<uint8> Cipher;
	TestTrue(TEXT("DES encodes"), RokSkynetCrypt::DesEncode(Key, Plain.GetData(), Plain.Num(), Cipher));
	TestEqual(TEXT("8 bytes + ISO 7816-4 block"), Cipher.Num(), 16);
	TestEqual(TEXT("First block is the standard DES vector"),
		ToHex(TArray<uint8>(Cipher.GetData(), 8)), FString(TEXT("85E813540F0AB405")));
	TArray<uint8> Back;
	TestTrue(TEXT("DES decodes"), RokSkynetCrypt::DesDecode(Key, Cipher.GetData(), Cipher.Num(), Back));
	TestEqual(TEXT("DES round trip"), ToHex(Back), ToHex(Plain));

	for (int32 Size = 0; Size <= 17; ++Size)
	{
		TArray<uint8> Text;
		for (int32 I = 0; I < Size; ++I) Text.Add(uint8(I * 37 + 1));
		TArray<uint8> Encoded, Decoded;
		RokSkynetCrypt::DesEncode(Key, Text.GetData(), Text.Num(), Encoded);
		TestEqual(FString::Printf(TEXT("Padded size for %d"), Size), Encoded.Num(), (Size + 8) & ~7);
		TestTrue(FString::Printf(TEXT("Decode %d"), Size), RokSkynetCrypt::DesDecode(Key, Encoded.GetData(), Encoded.Num(), Decoded));
		TestEqual(FString::Printf(TEXT("Round trip %d"), Size), ToHex(Decoded), ToHex(Text));
	}
	TArray<uint8> Bad;
	TestFalse(TEXT("Odd-length cipher rejected"), RokSkynetCrypt::DesDecode(Key, Plain.GetData(), 7, Bad));
	TestFalse(TEXT("Short key rejected"), RokSkynetCrypt::DesEncode(Hex(TEXT("0102")), Plain.GetData(), 8, Bad));

	// DH: both sides reach the same secret.
	for (int32 Round = 0; Round < 4; ++Round)
	{
		const TArray<uint8> A = RokSkynetCrypt::RandomKey();
		const TArray<uint8> B = RokSkynetCrypt::RandomKey();
		const TArray<uint8> SecretA = RokSkynetCrypt::DhSecret(RokSkynetCrypt::DhExchange(B), A);
		const TArray<uint8> SecretB = RokSkynetCrypt::DhSecret(RokSkynetCrypt::DhExchange(A), B);
		TestEqual(TEXT("DH secret is 8 bytes"), SecretA.Num(), 8);
		TestEqual(TEXT("DH secrets agree"), ToHex(SecretA), ToHex(SecretB));
	}
	// 5^1 mod p = 5, little endian.
	TestEqual(TEXT("dhexchange(1) = 5"), ToHex(RokSkynetCrypt::DhExchange(Hex(TEXT("0100000000000000")))),
		FString(TEXT("0500000000000000")));

	// hashkey("") is djb 5381 | js 1315423911, little endian.
	TestEqual(TEXT("hashkey of empty text"), ToHex(RokSkynetCrypt::HashKey({})), FString(TEXT("05150000A7C6674E")));
	TestEqual(TEXT("hmac64 is 8 bytes"), RokSkynetCrypt::Hmac64(Key, Plain).Num(), 8);
	TestEqual(TEXT("hmac64 is deterministic"), ToHex(RokSkynetCrypt::Hmac64(Key, Plain)), ToHex(RokSkynetCrypt::Hmac64(Key, Plain)));
	TestNotEqual(TEXT("hmac64 depends on order"), ToHex(RokSkynetCrypt::Hmac64(Key, Plain)), ToHex(RokSkynetCrypt::Hmac64(Plain, Key)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokSprotoReadmeWireTest, "Rok.Net.Codec.SprotoReadmeWire",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokSprotoReadmeWireTest::RunTest(const FString&)
{
	FRokSprotoSchema Schema;
	FString Error;
	if (!TestTrue(TEXT("README schema parses"), Schema.Parse(ReadmeSchema, Error)))
	{
		AddError(Error);
		return false;
	}
	const int32 PersonType = Schema.FindStruct(TEXT("Person"));
	const int32 DataType = Schema.FindStruct(TEXT("Data"));

	auto Check = [&](const TCHAR* What, int32 Type, const TSharedPtr<FJsonObject>& Value, const FString& ExpectedHex)
	{
		TArray<uint8> Bytes;
		if (!TestTrue(FString(What) + TEXT(" encodes"), Schema.Encode(Type, *Value, Bytes, Error))) AddError(Error);
		TestEqual(FString(What) + TEXT(" bytes"), ToHex(Bytes), ToHex(Hex(ExpectedHex)));
		TSharedPtr<FJsonObject> Decoded;
		TestEqual(FString(What) + TEXT(" decodes fully"), Schema.Decode(Type, Bytes.GetData(), Bytes.Num(), Decoded, Error), Bytes.Num());
		TArray<uint8> Again;
		Schema.Encode(Type, *Decoded, Again, Error);
		TestEqual(FString(What) + TEXT(" re-encodes identically"), ToHex(Again), ToHex(Bytes));
		return Decoded;
	};

	// Example 1.
	TSharedPtr<FJsonObject> Alice = Person(TEXT("Alice"), 13);
	Alice->SetBoolField(TEXT("marital"), false);
	Check(TEXT("Example 1"), PersonType, Alice, TEXT("0300 0000 1C00 0200 05000000 416C696365"));

	// Example 2.
	TSharedPtr<FJsonObject> Bob = Person(TEXT("Bob"), 40);
	TArray<TSharedPtr<FJsonValue>> Children;
	Children.Add(MakeShared<FJsonValueObject>(Person(TEXT("Alice"), 13)));
	Children.Add(MakeShared<FJsonValueObject>(Person(TEXT("Carol"), 5)));
	Bob->SetArrayField(TEXT("children"), Children);
	const TSharedPtr<FJsonObject> BobBack = Check(TEXT("Example 2"), PersonType, Bob,
		TEXT("0400 0000 5200 0100 0000 03000000 426F62 26000000 0F000000 0200 0000 1C00 05000000 416C696365 ")
		TEXT("0F000000 0200 0000 0C00 05000000 4361726F6C"));
	if (BobBack.IsValid())
	{
		TestEqual(TEXT("Example 2 child count"), BobBack->GetArrayField(TEXT("children")).Num(), 2);
	}

	// Example 3.
	TSharedPtr<FJsonObject> Numbers = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Small;
	for (int32 I = 1; I <= 5; ++I) Small.Add(MakeShared<FJsonValueNumber>(I));
	Numbers->SetArrayField(TEXT("numbers"), Small);
	Check(TEXT("Example 3"), DataType, Numbers, TEXT("0100 0000 15000000 04 01000000 02000000 03000000 04000000 05000000"));

	// Example 4.
	TSharedPtr<FJsonObject> Wide = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Big;
	for (int64 I = 1; I <= 3; ++I) Big.Add(MakeShared<FJsonValueNumber>(double((int64(1) << 32) + I)));
	Wide->SetArrayField(TEXT("numbers"), Big);
	Check(TEXT("Example 4"), DataType, Wide,
		TEXT("0100 0000 19000000 08 0100000001000000 0200000001000000 0300000001000000"));

	// Example 5.
	TSharedPtr<FJsonObject> Bools = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> BoolItems;
	BoolItems.Add(MakeShared<FJsonValueBoolean>(false));
	BoolItems.Add(MakeShared<FJsonValueBoolean>(true));
	BoolItems.Add(MakeShared<FJsonValueBoolean>(false));
	Bools->SetArrayField(TEXT("bools"), BoolItems);
	Check(TEXT("Example 5"), DataType, Bools, TEXT("0200 0100 0000 03000000 00 01 00"));

	// Example 6.
	TSharedPtr<FJsonObject> Ints = MakeShared<FJsonObject>();
	Ints->SetNumberField(TEXT("number"), 100000);
	Ints->SetNumberField(TEXT("bignumber"), -10000000000.0);
	const TSharedPtr<FJsonObject> IntsBack = Check(TEXT("Example 6"), DataType, Ints,
		TEXT("0300 0300 0000 0000 04000000 A0860100 08000000 001CF4ABFDFFFFFF"));
	int64 BigBack = 0;
	TestTrue(TEXT("Example 6 bignumber reads back"), RokSprotoJson::TryGetInt64(IntsBack, TEXT("bignumber"), BigBack));
	TestEqual(TEXT("Example 6 bignumber value"), BigBack, int64(-10000000000));

	// Decimal integer(2): 1.82 travels as 182 inline after skipping tags 0..5.
	TSharedPtr<FJsonObject> Fixed = MakeShared<FJsonObject>();
	Fixed->SetNumberField(TEXT("fpn"), 1.82);
	Check(TEXT("Decimal"), DataType, Fixed, TEXT("0200 0B00 6E01"));

	// 0-pack examples from the README.
	const TArray<uint8> Unpacked = Hex(TEXT("08000000 03000200 19000000 AA010000"));
	TArray<uint8> Packed;
	FRokSprotoSchema::Pack(Unpacked.GetData(), Unpacked.Num(), Packed);
	TestEqual(TEXT("0-pack example 1"), ToHex(Packed), FString(TEXT("510803023119AA01")));
	TArray<uint8> Run;
	Run.Init(0x8a, 30);
	FRokSprotoSchema::Pack(Run.GetData(), Run.Num(), Packed);
	TArray<uint8> ExpectedRun = {0xff, 0x03};
	ExpectedRun.Append(Run);
	ExpectedRun.Add(0);
	ExpectedRun.Add(0);
	TestEqual(TEXT("0-pack 0xff run"), ToHex(Packed), ToHex(ExpectedRun));
	TArray<uint8> Restored;
	TestTrue(TEXT("0-unpack run"), FRokSprotoSchema::Unpack(Packed.GetData(), Packed.Num(), Restored));
	TestEqual(TEXT("0-unpack pads to 8 bytes"), Restored.Num(), 32);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokSprotoRokSchemaTest, "Rok.Net.Codec.RokProtocolSchema",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokSprotoRokSchemaTest::RunTest(const FString&)
{
	TArray<uint8> Bytes;
	const FString Path = RokDataPaths::Resolve(TEXT("Protocol/Protocol.sproto"));
	if (!TestTrue(TEXT("Protocol.sproto is in RokData"), FFileHelper::LoadFileToArray(Bytes, *Path))) return false;
	FString Text;
	for (const uint8 Byte : Bytes) Text.AppendChar(Byte < 0x80 ? TCHAR(Byte) : TCHAR(' '));
	FRokSprotoSchema Schema;
	FString Error;
	if (!TestTrue(TEXT("Rok schema parses"), Schema.Parse(Text, Error)))
	{
		AddError(Error);
		return false;
	}
	const FRokSprotoProtocol* Gate = Schema.FindProtocol(TEXT("GateMessage"));
	const FRokSprotoProtocol* Login = Schema.FindProtocol(TEXT("Role_RoleLogin"));
	const FRokSprotoProtocol* Buildings = Schema.FindProtocol(TEXT("Build_BuildingInfo"));
	TestTrue(TEXT("GateMessage is tag 0"), Gate && Gate->Tag == 0);
	TestTrue(TEXT("Role_RoleLogin is tag 3"), Login && Login->Tag == 3);
	TestTrue(TEXT("Build_BuildingInfo is tag 30151"), Buildings && Buildings->Tag == 30151);
	TestTrue(TEXT(".package exists"), Schema.FindStruct(TEXT("package")) != INDEX_NONE);
	TestTrue(TEXT("Nested type resolves"), Schema.FindStruct(TEXT("Activity.ScheduleInfo.DataInfo")) != INDEX_NONE);
	AddInfo(FString::Printf(TEXT("Rok schema: %d protocols, %d types"), Schema.NumProtocols(), Schema.NumStructs()));
	TestTrue(TEXT("More than 200 protocols"), Schema.NumProtocols() > 200);
	if (!Gate || !Login || !Buildings) return false;

	// RPC round trip through the GateMessage wrapper, as NetProxy.SendSproto builds it.
	FRokSprotoRpc Client(Schema);
	FRokSprotoRpc Server(Schema);
	TSharedPtr<FJsonObject> Request = MakeShared<FJsonObject>();
	Request->SetNumberField(TEXT("rid"), 1234567);
	Request->SetStringField(TEXT("ip"), TEXT("127.0.0.1"));
	Request->SetStringField(TEXT("phone"), TEXT("测试"));
	Request->SetNumberField(TEXT("language"), 40);
	TArray<uint8> Inner;
	TestTrue(TEXT("RoleLogin request encodes"), Client.EncodeRequest(Login->Tag, Request.Get(), true, 7, Inner, Error));
	FRokSprotoMessage Received;
	TestTrue(TEXT("Server dispatches the request"), Server.Dispatch(Inner.GetData(), Inner.Num(), Received, Error));
	TestTrue(TEXT("Request flag"), Received.bRequest);
	TestEqual(TEXT("Request tag"), Received.Tag, 3);
	TestEqual(TEXT("Request session"), Received.Session, int64(7));
	int64 Rid = 0;
	FString Phone;
	TestTrue(TEXT("rid survives"), RokSprotoJson::TryGetInt64(Received.Body, TEXT("rid"), Rid) && Rid == 1234567);
	TestTrue(TEXT("UTF-8 string survives"), Received.Body->TryGetStringField(TEXT("phone"), Phone) && Phone == TEXT("测试"));

	// A server push with a keyed struct array and a nested struct.
	const int32 PushType = Buildings->RequestType;
	TSharedPtr<FJsonObject> Building = MakeShared<FJsonObject>();
	Building->SetNumberField(TEXT("buildingIndex"), 3);
	Building->SetNumberField(TEXT("type"), 18);
	Building->SetNumberField(TEXT("level"), 25);
	Building->SetNumberField(TEXT("finishTime"), 1790000000);
	TSharedPtr<FJsonObject> Pos = MakeShared<FJsonObject>();
	Pos->SetNumberField(TEXT("x"), 12);
	Pos->SetNumberField(TEXT("y"), -4);
	Building->SetObjectField(TEXT("pos"), Pos);
	TSharedPtr<FJsonObject> Push = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> BuildingItems;
	BuildingItems.Add(MakeShared<FJsonValueObject>(Building));
	Push->SetArrayField(TEXT("buildingInfo"), BuildingItems);
	TArray<uint8> PushBytes;
	TestTrue(TEXT("Building push encodes"), Schema.Encode(PushType, *Push, PushBytes, Error));
	TSharedPtr<FJsonObject> PushBack;
	TestEqual(TEXT("Building push decodes"), Schema.Decode(PushType, PushBytes.GetData(), PushBytes.Num(), PushBack, Error), PushBytes.Num());
	const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
	if (TestTrue(TEXT("buildingInfo array"), PushBack.IsValid() && PushBack->TryGetArrayField(TEXT("buildingInfo"), Items) && Items->Num() == 1))
	{
		const TSharedPtr<FJsonObject> Item = (*Items)[0]->AsObject();
		int64 Level = 0, FinishTime = 0, Y = 0;
		RokSprotoJson::TryGetInt64(Item, TEXT("level"), Level);
		RokSprotoJson::TryGetInt64(Item, TEXT("finishTime"), FinishTime);
		RokSprotoJson::TryGetInt64(Item->GetObjectField(TEXT("pos")), TEXT("y"), Y);
		TestEqual(TEXT("level"), Level, int64(25));
		TestEqual(TEXT("finishTime (32-bit data)"), FinishTime, int64(1790000000));
		TestEqual(TEXT("negative pos.y"), Y, int64(-4));
	}
	return true;
}
#endif
