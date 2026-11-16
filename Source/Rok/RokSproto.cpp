#include "RokSproto.h"

#include "Dom/JsonValue.h"
#include "HAL/PlatformTime.h"
#include "Misc/Base64.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// Wire format port of skynet lualib-src/sproto/sproto.c (MIT, codingnow.com):
// sproto_encode / sproto_decode / sproto_pack / sproto_unpack.
namespace
{
	constexpr int32 SizeOfHeader = 2;
	constexpr int32 SizeOfField = 2;
	constexpr int32 SizeOfLength = 4;
	constexpr int32 MaxDepth = 64;

	// ---- schema text tokenizer -------------------------------------------------------

	struct FToken
	{
		enum EKind { Name, Number, Punct, End } Kind = End;
		FString Text;
		int32 Line = 0;
	};

	bool IsNameStart(TCHAR C) { return FChar::IsAlpha(C) || C == TEXT('_'); }
	bool IsNameChar(TCHAR C) { return FChar::IsAlnum(C) || C == TEXT('_') || C == TEXT('.'); }

	void Tokenize(const FString& Text, TArray<FToken>& Out)
	{
		int32 Line = 1;
		const int32 Len = Text.Len();
		for (int32 I = 0; I < Len;)
		{
			const TCHAR C = Text[I];
			if (C == TEXT('\n')) { ++Line; ++I; continue; }
			if (FChar::IsWhitespace(C)) { ++I; continue; }
			if (C == TEXT('#'))
			{
				while (I < Len && Text[I] != TEXT('\n')) ++I;
				continue;
			}
			FToken Token;
			Token.Line = Line;
			if (IsNameStart(C))
			{
				const int32 Start = I;
				while (I < Len && IsNameChar(Text[I])) ++I;
				Token.Kind = FToken::Name;
				Token.Text = Text.Mid(Start, I - Start);
			}
			else if (FChar::IsDigit(C))
			{
				const int32 Start = I;
				while (I < Len && FChar::IsDigit(Text[I])) ++I;
				Token.Kind = FToken::Number;
				Token.Text = Text.Mid(Start, I - Start);
			}
			else
			{
				Token.Kind = FToken::Punct;
				Token.Text = FString(1, &Text[I]);
				++I;
			}
			Out.Add(MoveTemp(Token));
		}
		FToken EndToken;
		EndToken.Line = Line;
		Out.Add(EndToken);
	}

	// ---- parse tree before type resolution --------------------------------------------

	struct FRawField
	{
		FString Name;
		int32 Tag = 0;
		bool bArray = false;
		FString TypeName;
		int32 DecimalDigits = -1;
		FString MapKey;
		int32 Line = 0;
	};

	struct FRawStruct
	{
		FString FullName;
		TArray<FRawField> Fields;
	};

	struct FRawProtocol
	{
		FString Name;
		int32 Tag = 0;
		FString Request;
		FString Response;
	};

	class FParser
	{
	public:
		FParser(const TArray<FToken>& InTokens) : Tokens(InTokens) {}

		TArray<FRawStruct> Structs;
		TArray<FRawProtocol> Protocols;
		FString Error;

		bool Run()
		{
			while (Peek().Kind != FToken::End)
			{
				if (IsPunct(TEXT(".")))
				{
					Next();
					if (!ParseStruct(FString())) return false;
				}
				else if (!ParseProtocol())
				{
					return false;
				}
			}
			return true;
		}

	private:
		const TArray<FToken>& Tokens;
		int32 Pos = 0;

		const FToken& Peek() const { return Tokens[Pos]; }
		const FToken& Next() { return Tokens[Pos < Tokens.Num() - 1 ? Pos++ : Pos]; }
		bool IsPunct(const TCHAR* P) const { return Peek().Kind == FToken::Punct && Peek().Text == P; }

		bool Fail(const FString& Message)
		{
			Error = FString::Printf(TEXT("line %d: %s (near '%s')"), Peek().Line, *Message, *Peek().Text);
			return false;
		}

		bool Expect(const TCHAR* P)
		{
			if (!IsPunct(P)) return Fail(FString::Printf(TEXT("expected '%s'"), P));
			Next();
			return true;
		}

		bool ReadName(FString& Out)
		{
			if (Peek().Kind != FToken::Name) return Fail(TEXT("expected a name"));
			Out = Next().Text;
			return true;
		}

		bool ReadNumber(int32& Out)
		{
			if (Peek().Kind != FToken::Number) return Fail(TEXT("expected a number"));
			Out = FCString::Atoi(*Next().Text);
			return true;
		}

		// After '.': Name '{' (field | '.' struct)* '}'
		bool ParseStruct(const FString& Scope)
		{
			FString Name;
			if (!ReadName(Name)) return false;
			const FString FullName = Scope.IsEmpty() ? Name : Scope + TEXT(".") + Name;
			return ParseStructBody(FullName);
		}

		bool ParseStructBody(const FString& FullName)
		{
			if (!Expect(TEXT("{"))) return false;
			const int32 Index = Structs.AddDefaulted();
			Structs[Index].FullName = FullName;
			while (!IsPunct(TEXT("}")))
			{
				if (Peek().Kind == FToken::End) return Fail(TEXT("unterminated struct ") + FullName);
				if (IsPunct(TEXT(".")))
				{
					Next();
					if (!ParseStruct(FullName)) return false;
					continue;
				}
				FRawField Field;
				Field.Line = Peek().Line;
				if (!ReadName(Field.Name) || !ReadNumber(Field.Tag) || !Expect(TEXT(":"))) return false;
				if (IsPunct(TEXT("*")))
				{
					Next();
					Field.bArray = true;
				}
				if (!ReadName(Field.TypeName)) return false;
				if (IsPunct(TEXT("(")))
				{
					Next();
					if (Peek().Kind == FToken::Number)
					{
						if (!ReadNumber(Field.DecimalDigits)) return false;
					}
					else if (Peek().Kind == FToken::Name)
					{
						Field.MapKey = Next().Text;
					}
					if (!Expect(TEXT(")"))) return false;
				}
				Structs[Index].Fields.Add(MoveTemp(Field));
			}
			Next();
			return true;
		}

		// Name Tag '{' (('request'|'response') (Name | 'nil' | '{' ... '}'))* '}'
		bool ParseProtocol()
		{
			FRawProtocol Protocol;
			if (!ReadName(Protocol.Name) || !ReadNumber(Protocol.Tag) || !Expect(TEXT("{"))) return false;
			while (!IsPunct(TEXT("}")))
			{
				FString Which;
				if (!ReadName(Which)) return false;
				if (Which != TEXT("request") && Which != TEXT("response"))
				{
					return Fail(TEXT("expected request or response in ") + Protocol.Name);
				}
				FString TypeName;
				if (IsPunct(TEXT("{")))
				{
					TypeName = Protocol.Name + TEXT(".") + Which;
					if (!ParseStructBody(TypeName)) return false;
				}
				else
				{
					if (!ReadName(TypeName)) return false;
					if (TypeName == TEXT("nil")) TypeName.Reset();
				}
				(Which == TEXT("request") ? Protocol.Request : Protocol.Response) = TypeName;
			}
			Next();
			Protocols.Add(MoveTemp(Protocol));
			return true;
		}
	};

	// ---- encode helpers ----------------------------------------------------------------

	void AppendUint32(TArray<uint8>& Out, uint32 V)
	{
		Out.Add(uint8(V & 0xff));
		Out.Add(uint8((V >> 8) & 0xff));
		Out.Add(uint8((V >> 16) & 0xff));
		Out.Add(uint8((V >> 24) & 0xff));
	}

	void AppendUint64(TArray<uint8>& Out, uint64 V)
	{
		AppendUint32(Out, uint32(V & 0xffffffffull));
		AppendUint32(Out, uint32(V >> 32));
	}

	void AppendSized(TArray<uint8>& Out, const uint8* Data, int32 Size)
	{
		AppendUint32(Out, uint32(Size));
		Out.Append(Data, Size);
	}

	bool Fits32(int64 V)
	{
		const int64 High = V >> 31;
		return High == 0 || High == -1;
	}

	uint64 DoubleBits(double V)
	{
		uint64 Bits;
		FMemory::Memcpy(&Bits, &V, sizeof(Bits));
		return Bits;
	}

	double BitsToDouble(uint64 Bits)
	{
		double V;
		FMemory::Memcpy(&V, &Bits, sizeof(V));
		return V;
	}

	bool ReadInteger(const FRokSprotoField& Field, const TSharedPtr<FJsonValue>& Value, int64& Out)
	{
		if (!Value.IsValid() || Value->Type != EJson::Number) return false;
		if (Field.Decimal > 0)
		{
			double D = 0.0;
			if (!Value->TryGetNumber(D)) return false;
			Out = int64(FMath::RoundHalfFromZero(D * double(Field.Decimal)));
			return true;
		}
		return Value->TryGetNumber(Out);
	}

	bool ReadStringBytes(const FRokSprotoField& Field, const TSharedPtr<FJsonValue>& Value, TArray<uint8>& Out)
	{
		FString Text;
		if (!Value.IsValid() || !Value->TryGetString(Text)) return false;
		if (Field.Type == ERokSprotoType::Binary)
		{
			return FBase64::Decode(Text, Out);
		}
		const FTCHARToUTF8 Utf8(*Text);
		Out = TArray<uint8>(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		return true;
	}

	TSharedPtr<FJsonValue> MakeIntegerValue(const FRokSprotoField& Field, int64 V)
	{
		if (Field.Decimal > 0)
		{
			return MakeShared<FJsonValueNumber>(double(V) / double(Field.Decimal));
		}
		constexpr int64 Exact = int64(1) << 53;
		if (V > Exact || V < -Exact)
		{
			return MakeShared<FJsonValueNumberString>(FString::Printf(TEXT("%lld"), V));
		}
		return MakeShared<FJsonValueNumber>(double(V));
	}

	TSharedPtr<FJsonValue> MakeStringValue(const FRokSprotoField& Field, const uint8* Data, int32 Size)
	{
		if (Field.Type == ERokSprotoType::Binary)
		{
			return MakeShared<FJsonValueString>(FBase64::Encode(Data, Size));
		}
		const FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Data), Size);
		return MakeShared<FJsonValueString>(FString(Converter.Length(), Converter.Get()));
	}

	inline uint32 ToDword(const uint8* P)
	{
		return uint32(P[0]) | uint32(P[1]) << 8 | uint32(P[2]) << 16 | uint32(P[3]) << 24;
	}

	inline int32 ToWord(const uint8* P)
	{
		return int32(P[0]) | int32(P[1]) << 8;
	}

	inline int64 Expand64(uint32 V)
	{
		uint64 Value = V;
		if (Value & 0x80000000u) Value |= ~uint64(0) << 32;
		return int64(Value);
	}

	// ---- 0-pack (sproto_pack) ----------------------------------------------------------

	int32 PackSegment(const uint8* Src, uint8* Buffer, int32 Size, int32 N)
	{
		uint8 Header = 0;
		int32 NotZero = 0;
		uint8* OBuffer = Buffer;
		++Buffer;
		--Size;
		if (Size < 0) OBuffer = nullptr;
		for (int32 I = 0; I < 8; ++I)
		{
			if (Src[I] != 0)
			{
				++NotZero;
				Header |= uint8(1 << I);
				if (Size > 0)
				{
					*Buffer = Src[I];
					++Buffer;
					--Size;
				}
			}
		}
		if ((NotZero == 7 || NotZero == 6) && N > 0) NotZero = 8;
		if (NotZero == 8) return N > 0 ? 8 : 10;
		if (OBuffer) *OBuffer = Header;
		return NotZero + 1;
	}

	void WriteFF(const uint8* Src, const uint8* SrcEnd, uint8* Des, int32 N)
	{
		Des[0] = 0xff;
		Des[1] = uint8(N - 1);
		if (Src + N * 8 <= SrcEnd)
		{
			FMemory::Memcpy(Des + 2, Src, N * 8);
		}
		else
		{
			const int32 Size = int32(SrcEnd - Src);
			FMemory::Memcpy(Des + 2, Src, Size);
			FMemory::Memzero(Des + 2 + Size, N * 8 - Size);
		}
	}
}

const FRokSprotoField* FRokSprotoStruct::FindTag(int32 Tag) const
{
	int32 Low = 0;
	int32 High = Fields.Num() - 1;
	while (Low <= High)
	{
		const int32 Mid = (Low + High) / 2;
		const int32 MidTag = Fields[Mid].Tag;
		if (MidTag == Tag) return &Fields[Mid];
		if (MidTag < Tag) Low = Mid + 1;
		else High = Mid - 1;
	}
	return nullptr;
}

bool FRokSprotoSchema::Parse(const FString& Text, FString& OutError)
{
	Structs.Reset();
	StructByName.Reset();
	Protocols.Reset();
	ProtocolByTag.Reset();
	ProtocolByName.Reset();

	TArray<FToken> Tokens;
	Tokenize(Text, Tokens);
	FParser Parser(Tokens);
	if (!Parser.Run())
	{
		OutError = Parser.Error;
		return false;
	}

	for (const FRawStruct& Raw : Parser.Structs)
	{
		if (StructByName.Contains(Raw.FullName))
		{
			OutError = TEXT("duplicate type ") + Raw.FullName;
			return false;
		}
		StructByName.Add(Raw.FullName, Structs.Num());
		Structs.AddDefaulted_GetRef().Name = Raw.FullName;
	}

	auto ResolveStruct = [this](const FString& Scope, const FString& TypeName) -> int32
	{
		// Innermost scope first, as sprotoparser does: A.B.T, A.T, T.
		FString Prefix = Scope;
		while (true)
		{
			const FString Candidate = Prefix.IsEmpty() ? TypeName : Prefix + TEXT(".") + TypeName;
			if (const int32* Found = StructByName.Find(Candidate)) return *Found;
			if (Prefix.IsEmpty()) return INDEX_NONE;
			int32 Dot = INDEX_NONE;
			Prefix = Prefix.FindLastChar(TEXT('.'), Dot) ? Prefix.Left(Dot) : FString();
		}
	};

	for (int32 Index = 0; Index < Parser.Structs.Num(); ++Index)
	{
		const FRawStruct& Raw = Parser.Structs[Index];
		FRokSprotoStruct& Type = Structs[Index];
		TSet<int32> SeenTags;
		TSet<FString> SeenNames;
		for (const FRawField& RawField : Raw.Fields)
		{
			FRokSprotoField Field;
			Field.Name = RawField.Name;
			Field.Tag = RawField.Tag;
			Field.bArray = RawField.bArray;
			Field.MapKey = RawField.MapKey;
			if (RawField.TypeName == TEXT("integer")) Field.Type = ERokSprotoType::Integer;
			else if (RawField.TypeName == TEXT("boolean")) Field.Type = ERokSprotoType::Boolean;
			else if (RawField.TypeName == TEXT("string")) Field.Type = ERokSprotoType::String;
			else if (RawField.TypeName == TEXT("binary")) Field.Type = ERokSprotoType::Binary;
			else if (RawField.TypeName == TEXT("double")) Field.Type = ERokSprotoType::Double;
			else
			{
				Field.Type = ERokSprotoType::Struct;
				Field.StructIndex = ResolveStruct(Raw.FullName, RawField.TypeName);
				if (Field.StructIndex == INDEX_NONE)
				{
					OutError = FString::Printf(TEXT("line %d: unknown type %s in %s"), RawField.Line, *RawField.TypeName, *Raw.FullName);
					return false;
				}
			}
			if (RawField.DecimalDigits >= 0)
			{
				if (Field.Type != ERokSprotoType::Integer)
				{
					OutError = FString::Printf(TEXT("line %d: decimal on a non-integer"), RawField.Line);
					return false;
				}
				Field.Decimal = 1;
				for (int32 Digit = 0; Digit < RawField.DecimalDigits; ++Digit) Field.Decimal *= 10;
			}
			if (Field.Tag < 0 || SeenTags.Contains(Field.Tag) || SeenNames.Contains(Field.Name))
			{
				OutError = FString::Printf(TEXT("line %d: duplicate or invalid field %s in %s"), RawField.Line, *Field.Name, *Raw.FullName);
				return false;
			}
			SeenTags.Add(Field.Tag);
			SeenNames.Add(Field.Name);
			Type.Fields.Add(MoveTemp(Field));
		}
		Type.Fields.Sort([](const FRokSprotoField& A, const FRokSprotoField& B) { return A.Tag < B.Tag; });
	}

	for (const FRawProtocol& Raw : Parser.Protocols)
	{
		if (ProtocolByTag.Contains(Raw.Tag) || ProtocolByName.Contains(Raw.Name))
		{
			OutError = TEXT("duplicate protocol ") + Raw.Name;
			return false;
		}
		FRokSprotoProtocol Protocol;
		Protocol.Name = Raw.Name;
		Protocol.Tag = Raw.Tag;
		if (!Raw.Request.IsEmpty())
		{
			Protocol.RequestType = ResolveStruct(FString(), Raw.Request);
			if (Protocol.RequestType == INDEX_NONE) { OutError = TEXT("unknown request type ") + Raw.Request; return false; }
		}
		if (!Raw.Response.IsEmpty())
		{
			Protocol.ResponseType = ResolveStruct(FString(), Raw.Response);
			if (Protocol.ResponseType == INDEX_NONE) { OutError = TEXT("unknown response type ") + Raw.Response; return false; }
		}
		ProtocolByTag.Add(Protocol.Tag, Protocols.Num());
		ProtocolByName.Add(Protocol.Name, Protocols.Num());
		Protocols.Add(MoveTemp(Protocol));
	}
	return true;
}

int32 FRokSprotoSchema::FindStruct(const FString& Name) const
{
	const int32* Found = StructByName.Find(Name);
	return Found ? *Found : INDEX_NONE;
}

const FRokSprotoStruct* FRokSprotoSchema::GetStruct(int32 Index) const
{
	return Structs.IsValidIndex(Index) ? &Structs[Index] : nullptr;
}

const FRokSprotoProtocol* FRokSprotoSchema::FindProtocol(int32 Tag) const
{
	const int32* Found = ProtocolByTag.Find(Tag);
	return Found ? &Protocols[*Found] : nullptr;
}

const FRokSprotoProtocol* FRokSprotoSchema::FindProtocol(const FString& Name) const
{
	const int32* Found = ProtocolByName.Find(Name);
	return Found ? &Protocols[*Found] : nullptr;
}

bool FRokSprotoSchema::Encode(int32 StructIndex, const FJsonObject& Value, TArray<uint8>& Out, FString& OutError) const
{
	Out.Reset();
	const FRokSprotoStruct* Type = GetStruct(StructIndex);
	if (!Type)
	{
		OutError = TEXT("unknown struct index");
		return false;
	}
	return EncodeStruct(*Type, Value, Out, OutError, 0);
}

bool FRokSprotoSchema::EncodeStruct(const FRokSprotoStruct& Type, const FJsonObject& Value, TArray<uint8>& Out,
	FString& OutError, int32 Depth) const
{
	if (Depth > MaxDepth)
	{
		OutError = TEXT("struct nesting too deep");
		return false;
	}
	TArray<uint16> Header;
	TArray<uint8> Data;
	int32 LastTag = -1;

	auto FieldError = [&OutError, &Type](const FRokSprotoField& Field, const TCHAR* Why)
	{
		OutError = FString::Printf(TEXT("%s.%s: %s"), *Type.Name, *Field.Name, Why);
		return false;
	};

	for (const FRokSprotoField& Field : Type.Fields)
	{
		const TSharedPtr<FJsonValue> FieldValue = Value.TryGetField(Field.Name);
		if (!FieldValue.IsValid() || FieldValue->IsNull()) continue;

		int32 Inline = 0;
		bool bHasData = false;
		TArray<uint8> FieldData;

		if (Field.bArray)
		{
			const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
			if (!FieldValue->TryGetArray(Items)) return FieldError(Field, TEXT("expected an array"));
			TArray<uint8> Body;
			switch (Field.Type)
			{
			case ERokSprotoType::Integer:
			case ERokSprotoType::Double:
			{
				TArray<int64> Ints;
				bool bWide = Field.Type == ERokSprotoType::Double;
				for (const TSharedPtr<FJsonValue>& Item : *Items)
				{
					int64 V = 0;
					if (Field.Type == ERokSprotoType::Double)
					{
						double D = 0.0;
						if (!Item.IsValid() || !Item->TryGetNumber(D)) return FieldError(Field, TEXT("expected numbers"));
						V = int64(DoubleBits(D));
					}
					else if (!ReadInteger(Field, Item, V))
					{
						return FieldError(Field, TEXT("expected integers"));
					}
					bWide |= !Fits32(V);
					Ints.Add(V);
				}
				if (Ints.Num() > 0)
				{
					Body.Add(uint8(bWide ? 8 : 4));
					for (const int64 V : Ints)
					{
						if (bWide) AppendUint64(Body, uint64(V));
						else AppendUint32(Body, uint32(V));
					}
				}
				break;
			}
			case ERokSprotoType::Boolean:
				for (const TSharedPtr<FJsonValue>& Item : *Items)
				{
					bool B = false;
					if (!Item.IsValid() || !Item->TryGetBool(B)) return FieldError(Field, TEXT("expected booleans"));
					Body.Add(B ? 1 : 0);
				}
				break;
			case ERokSprotoType::String:
			case ERokSprotoType::Binary:
				for (const TSharedPtr<FJsonValue>& Item : *Items)
				{
					TArray<uint8> Bytes;
					if (!ReadStringBytes(Field, Item, Bytes)) return FieldError(Field, TEXT("expected strings"));
					AppendSized(Body, Bytes.GetData(), Bytes.Num());
				}
				break;
			case ERokSprotoType::Struct:
				for (const TSharedPtr<FJsonValue>& Item : *Items)
				{
					const TSharedPtr<FJsonObject>* Object = nullptr;
					if (!Item.IsValid() || !Item->TryGetObject(Object)) return FieldError(Field, TEXT("expected objects"));
					TArray<uint8> Bytes;
					if (!EncodeStruct(Structs[Field.StructIndex], **Object, Bytes, OutError, Depth + 1)) return false;
					AppendSized(Body, Bytes.GetData(), Bytes.Num());
				}
				break;
			}
			AppendSized(FieldData, Body.GetData(), Body.Num());
			bHasData = true;
		}
		else
		{
			switch (Field.Type)
			{
			case ERokSprotoType::Integer:
			{
				int64 V = 0;
				if (!ReadInteger(Field, FieldValue, V)) return FieldError(Field, TEXT("expected an integer"));
				if (Fits32(V))
				{
					const uint32 U = uint32(V);
					if (U < 0x7fff) Inline = int32((U + 1) * 2);
					else { AppendUint32(FieldData, 4); AppendUint32(FieldData, U); bHasData = true; }
				}
				else
				{
					AppendUint32(FieldData, 8);
					AppendUint64(FieldData, uint64(V));
					bHasData = true;
				}
				break;
			}
			case ERokSprotoType::Boolean:
			{
				bool B = false;
				if (!FieldValue->TryGetBool(B)) return FieldError(Field, TEXT("expected a boolean"));
				Inline = B ? 4 : 2;
				break;
			}
			case ERokSprotoType::Double:
			{
				double D = 0.0;
				if (!FieldValue->TryGetNumber(D)) return FieldError(Field, TEXT("expected a number"));
				AppendUint32(FieldData, 8);
				AppendUint64(FieldData, DoubleBits(D));
				bHasData = true;
				break;
			}
			case ERokSprotoType::String:
			case ERokSprotoType::Binary:
			{
				TArray<uint8> Bytes;
				if (!ReadStringBytes(Field, FieldValue, Bytes)) return FieldError(Field, TEXT("expected a string"));
				AppendSized(FieldData, Bytes.GetData(), Bytes.Num());
				bHasData = true;
				break;
			}
			case ERokSprotoType::Struct:
			{
				const TSharedPtr<FJsonObject>* Object = nullptr;
				if (!FieldValue->TryGetObject(Object)) return FieldError(Field, TEXT("expected an object"));
				TArray<uint8> Bytes;
				if (!EncodeStruct(Structs[Field.StructIndex], **Object, Bytes, OutError, Depth + 1)) return false;
				AppendSized(FieldData, Bytes.GetData(), Bytes.Num());
				bHasData = true;
				break;
			}
			}
		}

		const int32 Skip = Field.Tag - LastTag - 1;
		if (Skip > 0)
		{
			const int32 SkipWord = (Skip - 1) * 2 + 1;
			if (SkipWord > 0xffff) return FieldError(Field, TEXT("tag gap too large"));
			Header.Add(uint16(SkipWord));
		}
		Header.Add(uint16(bHasData ? 0 : Inline));
		Data.Append(FieldData);
		LastTag = Field.Tag;
	}

	Out.Reset(SizeOfHeader + Header.Num() * SizeOfField + Data.Num());
	Out.Add(uint8(Header.Num() & 0xff));
	Out.Add(uint8((Header.Num() >> 8) & 0xff));
	for (const uint16 Word : Header)
	{
		Out.Add(uint8(Word & 0xff));
		Out.Add(uint8((Word >> 8) & 0xff));
	}
	Out.Append(Data);
	return true;
}

int32 FRokSprotoSchema::Decode(int32 StructIndex, const uint8* Data, int32 Size, TSharedPtr<FJsonObject>& Out,
	FString& OutError) const
{
	const FRokSprotoStruct* Type = GetStruct(StructIndex);
	if (!Type)
	{
		OutError = TEXT("unknown struct index");
		return -1;
	}
	return DecodeStruct(*Type, Data, Size, Out, OutError, 0);
}

int32 FRokSprotoSchema::DecodeStruct(const FRokSprotoStruct& Type, const uint8* Data, int32 Size,
	TSharedPtr<FJsonObject>& Out, FString& OutError, int32 Depth) const
{
	auto Fail = [&OutError, &Type](const TCHAR* Why)
	{
		OutError = FString::Printf(TEXT("%s: %s"), *Type.Name, Why);
		return -1;
	};
	if (Depth > MaxDepth) return Fail(TEXT("struct nesting too deep"));
	if (Size < SizeOfHeader) return Fail(TEXT("truncated header"));

	Out = MakeShared<FJsonObject>();
	const int32 Total = Size;
	const uint8* Stream = Data;
	const int32 FieldCount = ToWord(Stream);
	Stream += SizeOfHeader;
	Size -= SizeOfHeader;
	if (Size < FieldCount * SizeOfField) return Fail(TEXT("truncated field table"));
	const uint8* DataStream = Stream + FieldCount * SizeOfField;
	Size -= FieldCount * SizeOfField;

	int32 Tag = -1;
	for (int32 I = 0; I < FieldCount; ++I)
	{
		int32 Value = ToWord(Stream + I * SizeOfField);
		++Tag;
		if (Value & 1)
		{
			Tag += Value / 2;
			continue;
		}
		Value = Value / 2 - 1;
		const uint8* Current = DataStream;
		uint32 DataSize = 0;
		if (Value < 0)
		{
			if (Size < SizeOfLength) return Fail(TEXT("truncated data length"));
			DataSize = ToDword(DataStream);
			if (uint32(Size) < DataSize + SizeOfLength) return Fail(TEXT("truncated data"));
			DataStream += DataSize + SizeOfLength;
			Size -= int32(DataSize) + SizeOfLength;
		}
		const FRokSprotoField* Field = Type.FindTag(Tag);
		if (!Field) continue;

		if (Value >= 0)
		{
			if (Field->bArray || (Field->Type != ERokSprotoType::Integer && Field->Type != ERokSprotoType::Boolean))
			{
				return Fail(TEXT("inline value on a non-integer field"));
			}
			if (Field->Type == ERokSprotoType::Boolean) Out->SetBoolField(Field->Name, Value != 0);
			else Out->SetField(Field->Name, MakeIntegerValue(*Field, Value));
			continue;
		}

		const uint8* Body = Current + SizeOfLength;
		if (Field->bArray)
		{
			TArray<TSharedPtr<FJsonValue>> Items;
			int32 Remaining = int32(DataSize);
			const uint8* P = Body;
			switch (Field->Type)
			{
			case ERokSprotoType::Integer:
			case ERokSprotoType::Double:
				if (Remaining > 0)
				{
					const int32 IntLen = *P;
					++P;
					--Remaining;
					if ((IntLen != 4 && IntLen != 8) || Remaining % IntLen != 0) return Fail(TEXT("bad integer array"));
					for (int32 Offset = 0; Offset < Remaining; Offset += IntLen)
					{
						const int64 V = IntLen == 4 ? Expand64(ToDword(P + Offset))
							: int64(uint64(ToDword(P + Offset)) | uint64(ToDword(P + Offset + 4)) << 32);
						if (Field->Type == ERokSprotoType::Double)
							Items.Add(MakeShared<FJsonValueNumber>(BitsToDouble(uint64(V))));
						else
							Items.Add(MakeIntegerValue(*Field, V));
					}
				}
				break;
			case ERokSprotoType::Boolean:
				for (int32 Offset = 0; Offset < Remaining; ++Offset)
				{
					Items.Add(MakeShared<FJsonValueBoolean>(P[Offset] != 0));
				}
				break;
			case ERokSprotoType::String:
			case ERokSprotoType::Binary:
			case ERokSprotoType::Struct:
				while (Remaining > 0)
				{
					if (Remaining < SizeOfLength) return Fail(TEXT("truncated array item"));
					const uint32 ItemSize = ToDword(P);
					P += SizeOfLength;
					Remaining -= SizeOfLength;
					if (ItemSize > uint32(Remaining)) return Fail(TEXT("truncated array item"));
					if (Field->Type == ERokSprotoType::Struct)
					{
						TSharedPtr<FJsonObject> Child;
						if (DecodeStruct(Structs[Field->StructIndex], P, int32(ItemSize), Child, OutError, Depth + 1) < 0) return -1;
						Items.Add(MakeShared<FJsonValueObject>(Child));
					}
					else
					{
						Items.Add(MakeStringValue(*Field, P, int32(ItemSize)));
					}
					P += ItemSize;
					Remaining -= int32(ItemSize);
				}
				break;
			}
			Out->SetArrayField(Field->Name, Items);
			continue;
		}

		switch (Field->Type)
		{
		case ERokSprotoType::Integer:
		case ERokSprotoType::Double:
		{
			int64 V = 0;
			if (DataSize == 4) V = Expand64(ToDword(Body));
			else if (DataSize == 8) V = int64(uint64(ToDword(Body)) | uint64(ToDword(Body + 4)) << 32);
			else return Fail(TEXT("bad integer size"));
			if (Field->Type == ERokSprotoType::Double)
				Out->SetNumberField(Field->Name, DataSize == 8 ? BitsToDouble(uint64(V)) : double(V));
			else
				Out->SetField(Field->Name, MakeIntegerValue(*Field, V));
			break;
		}
		case ERokSprotoType::String:
		case ERokSprotoType::Binary:
			Out->SetField(Field->Name, MakeStringValue(*Field, Body, int32(DataSize)));
			break;
		case ERokSprotoType::Struct:
		{
			TSharedPtr<FJsonObject> Child;
			if (DecodeStruct(Structs[Field->StructIndex], Body, int32(DataSize), Child, OutError, Depth + 1) < 0) return -1;
			Out->SetObjectField(Field->Name, Child);
			break;
		}
		default:
			return Fail(TEXT("boolean stored as data"));
		}
	}
	return Total - Size;
}

void FRokSprotoSchema::Pack(const uint8* SrcData, int32 SrcSize, TArray<uint8>& Out)
{
	// A segment emits at most 10 bytes (the first of a 0xff run); 16 bytes of slack.
	const int32 MaxSize = (SrcSize + 7) / 8 * 10 + 16;
	Out.SetNumZeroed(MaxSize);
	uint8 Tmp[8];
	const uint8* FFSrcStart = nullptr;
	uint8* FFDesStart = nullptr;
	int32 FFN = 0;
	int32 Size = 0;
	int32 BufferSize = MaxSize;
	const uint8* Src = SrcData;
	const uint8* SrcEnd = SrcData + SrcSize;
	uint8* Buffer = Out.GetData();
	for (int32 I = 0; I < SrcSize; I += 8)
	{
		const int32 Padding = I + 8 - SrcSize;
		if (Padding > 0)
		{
			FMemory::Memcpy(Tmp, Src, 8 - Padding);
			for (int32 J = 0; J < Padding; ++J) Tmp[7 - J] = 0;
			Src = Tmp;
		}
		const int32 N = PackSegment(Src, Buffer, BufferSize, FFN);
		BufferSize -= N;
		if (N == 10)
		{
			FFSrcStart = Src;
			FFDesStart = Buffer;
			FFN = 1;
		}
		else if (N == 8 && FFN > 0)
		{
			++FFN;
			if (FFN == 256)
			{
				WriteFF(FFSrcStart, SrcEnd, FFDesStart, 256);
				FFN = 0;
			}
		}
		else if (FFN > 0)
		{
			WriteFF(FFSrcStart, SrcEnd, FFDesStart, FFN);
			FFN = 0;
		}
		Src += 8;
		Buffer += N;
		Size += N;
	}
	if (FFN > 0)
	{
		WriteFF(FFSrcStart, SrcEnd, FFDesStart, FFN);
	}
	Out.SetNum(Size);
}

bool FRokSprotoSchema::Unpack(const uint8* Src, int32 SrcSize, TArray<uint8>& Out)
{
	Out.Reset(SrcSize * 2);
	while (SrcSize > 0)
	{
		const uint8 Header = Src[0];
		--SrcSize;
		++Src;
		if (Header == 0xff)
		{
			if (SrcSize < 1) return false;
			const int32 N = (int32(Src[0]) + 1) * 8;
			if (SrcSize < N + 1) return false;
			SrcSize -= N + 1;
			++Src;
			Out.Append(Src, N);
			Src += N;
		}
		else
		{
			for (int32 I = 0; I < 8; ++I)
			{
				if ((Header >> I) & 1)
				{
					if (SrcSize <= 0) return false;
					Out.Add(*Src);
					++Src;
					--SrcSize;
				}
				else
				{
					Out.Add(0);
				}
			}
		}
	}
	return true;
}

FRokSprotoRpc::FRokSprotoRpc(const FRokSprotoSchema& InSchema)
	: Schema(InSchema)
{
	PackageType = Schema.FindStruct(TEXT("package"));
}

bool FRokSprotoRpc::EncodeRequest(int32 ProtocolTag, const FJsonObject* Body, bool bWithSession, int64 Session,
	TArray<uint8>& OutPacked, FString& OutError, bool bTrackResponse)
{
	const FRokSprotoProtocol* Protocol = Schema.FindProtocol(ProtocolTag);
	if (!Protocol || PackageType == INDEX_NONE)
	{
		OutError = FString::Printf(TEXT("unknown protocol %d"), ProtocolTag);
		return false;
	}
	FJsonObject Package;
	Package.SetNumberField(TEXT("type"), ProtocolTag);
	if (bWithSession) Package.SetNumberField(TEXT("session"), double(Session));
	TArray<uint8> Raw;
	if (!Schema.Encode(PackageType, Package, Raw, OutError)) return false;
	if (Protocol->RequestType != INDEX_NONE)
	{
		const FJsonObject Empty;
		TArray<uint8> BodyBytes;
		if (!Schema.Encode(Protocol->RequestType, Body ? *Body : Empty, BodyBytes, OutError)) return false;
		Raw.Append(BodyBytes);
	}
	FRokSprotoSchema::Pack(Raw.GetData(), Raw.Num(), OutPacked);
	if (bWithSession && bTrackResponse)
	{
		PendingSessions.Add(Session, ProtocolTag);
		PendingSessionStartTimes.Add(Session, FPlatformTime::Seconds());
	}
	return true;
}

bool FRokSprotoRpc::Dispatch(const uint8* Packed, int32 Size, FRokSprotoMessage& Out, FString& OutError)
{
	Out = FRokSprotoMessage();
	TArray<uint8> Raw;
	if (!FRokSprotoSchema::Unpack(Packed, Size, Raw))
	{
		OutError = TEXT("bad 0-pack stream");
		return false;
	}
	TSharedPtr<FJsonObject> Package;
	const int32 HeaderSize = Schema.Decode(PackageType, Raw.GetData(), Raw.Num(), Package, OutError);
	if (HeaderSize < 0) return false;
	int64 Type = 0;
	Out.bHasSession = RokSprotoJson::TryGetInt64(Package, TEXT("session"), Out.Session);
	const uint8* BodyData = Raw.GetData() + HeaderSize;
	const int32 BodySize = Raw.Num() - HeaderSize;

	int32 BodyType = INDEX_NONE;
	if (RokSprotoJson::TryGetInt64(Package, TEXT("type"), Type))
	{
		Out.bRequest = true;
		Out.Tag = int32(Type);
		const FRokSprotoProtocol* Protocol = Schema.FindProtocol(Out.Tag);
		if (!Protocol)
		{
			OutError = FString::Printf(TEXT("unknown protocol %d"), Out.Tag);
			return false;
		}
		BodyType = Protocol->RequestType;
	}
	else
	{
		if (!Out.bHasSession)
		{
			OutError = TEXT("response without session");
			return false;
		}
		Out.Tag = TakePendingTag(Out.Session);
		const FRokSprotoProtocol* Protocol = Schema.FindProtocol(Out.Tag);
		if (!Protocol)
		{
			OutError = FString::Printf(TEXT("unknown session %lld"), Out.Session);
			return false;
		}
		BodyType = Protocol->ResponseType;
	}
	if (BodyType != INDEX_NONE && BodySize > 0)
	{
		if (Schema.Decode(BodyType, BodyData, BodySize, Out.Body, OutError) < 0) return false;
	}
	else
	{
		Out.Body = MakeShared<FJsonObject>();
	}
	return true;
}

int32 FRokSprotoRpc::TakePendingTag(int64 Session)
{
	int32 Tag = INDEX_NONE;
	PendingSessions.RemoveAndCopyValue(Session, Tag);
	PendingSessionStartTimes.Remove(Session);
	return Tag;
}

TArray<int64> FRokSprotoRpc::ExpirePendingResponses(double NowSeconds, double MaxAgeSeconds)
{
	TArray<int64> Expired;
	for (const TPair<int64, double>& Entry : PendingSessionStartTimes)
	{
		if (NowSeconds - Entry.Value >= MaxAgeSeconds) Expired.Add(Entry.Key);
	}
	for (const int64 Session : Expired)
	{
		PendingSessionStartTimes.Remove(Session);
		PendingSessions.Remove(Session);
	}
	return Expired;
}

namespace RokSprotoJson
{
	bool TryGetInt64(const TSharedPtr<FJsonObject>& Object, const FString& Field, int64& Out)
	{
		if (!Object.IsValid()) return false;
		const TSharedPtr<FJsonValue> Value = Object->TryGetField(Field);
		return Value.IsValid() && Value->Type == EJson::Number && Value->TryGetNumber(Out);
	}

	FString ToString(const TSharedPtr<FJsonObject>& Object, bool bPretty)
	{
		FString Out;
		if (!Object.IsValid()) return Out;
		if (bPretty)
		{
			const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
			FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
		}
		else
		{
			const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
				TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
			FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
		}
		return Out;
	}
}
