#pragma once

#include "CoreMinimal.h"

// C++ port of skynet's lualib-src/lua-crypt.c (MIT, codingnow.com), the primitives
// the Rok server's LoginGate.lua / GameGate.lua use and the Unity client mirrors in
// Skyunion/RunTime/Utils/Core/Crypt.cs plus the native cpatch DES (CPatch.cs).
// Keys and digests are 8 bytes; every function returns false / an empty array on bad input.
namespace RokSkynetCrypt
{
	// crypt.randomkey(): 8 random bytes whose xor is never 0.
	ROK_API TArray<uint8> RandomKey();

	// crypt.dhexchange(private): 5^private mod the largest 64-bit prime.
	ROK_API TArray<uint8> DhExchange(const TArray<uint8>& PrivateKey);

	// crypt.dhsecret(otherPublic, ownPrivate).
	ROK_API TArray<uint8> DhSecret(const TArray<uint8>& OtherPublic, const TArray<uint8>& OwnPrivate);

	// crypt.hmac64(x, y): the md5-without-padding mix the login challenge uses.
	ROK_API TArray<uint8> Hmac64(const TArray<uint8>& X, const TArray<uint8>& Y);

	// crypt.hashkey(text): djb + js hash of the text.
	ROK_API TArray<uint8> HashKey(const TArray<uint8>& Text);

	// crypt.desencode / desdecode with the default ISO 7816-4 padding.
	ROK_API bool DesEncode(const TArray<uint8>& Key, const uint8* Text, int32 Size, TArray<uint8>& Out);
	ROK_API bool DesDecode(const TArray<uint8>& Key, const uint8* Text, int32 Size, TArray<uint8>& Out);

	// Raw bytes of a string, without a terminator (the server treats every text as bytes).
	ROK_API TArray<uint8> Utf8Bytes(const FString& Text);
}
