#include "RokSkynetCrypt.h"

#include <random>

// Port of skynet lualib-src/lua-crypt.c (MIT, Copyright (c) 2012-2020 codingnow.com).
// Source read from D:/ueDemo/Rok-unity/server/data/work/3rd/skynet/lualib-src/lua-crypt.c.
namespace
{
	// DES S-boxes and PC1 tables, verbatim from lua-crypt.c.
	const uint32 SB1[64] = {
		0x01010400, 0x00000000, 0x00010000, 0x01010404,
		0x01010004, 0x00010404, 0x00000004, 0x00010000,
		0x00000400, 0x01010400, 0x01010404, 0x00000400,
		0x01000404, 0x01010004, 0x01000000, 0x00000004,
		0x00000404, 0x01000400, 0x01000400, 0x00010400,
		0x00010400, 0x01010000, 0x01010000, 0x01000404,
		0x00010004, 0x01000004, 0x01000004, 0x00010004,
		0x00000000, 0x00000404, 0x00010404, 0x01000000,
		0x00010000, 0x01010404, 0x00000004, 0x01010000,
		0x01010400, 0x01000000, 0x01000000, 0x00000400,
		0x01010004, 0x00010000, 0x00010400, 0x01000004,
		0x00000400, 0x00000004, 0x01000404, 0x00010404,
		0x01010404, 0x00010004, 0x01010000, 0x01000404,
		0x01000004, 0x00000404, 0x00010404, 0x01010400,
		0x00000404, 0x01000400, 0x01000400, 0x00000000,
		0x00010004, 0x00010400, 0x00000000, 0x01010004
	};

	const uint32 SB2[64] = {
		0x80108020, 0x80008000, 0x00008000, 0x00108020,
		0x00100000, 0x00000020, 0x80100020, 0x80008020,
		0x80000020, 0x80108020, 0x80108000, 0x80000000,
		0x80008000, 0x00100000, 0x00000020, 0x80100020,
		0x00108000, 0x00100020, 0x80008020, 0x00000000,
		0x80000000, 0x00008000, 0x00108020, 0x80100000,
		0x00100020, 0x80000020, 0x00000000, 0x00108000,
		0x00008020, 0x80108000, 0x80100000, 0x00008020,
		0x00000000, 0x00108020, 0x80100020, 0x00100000,
		0x80008020, 0x80100000, 0x80108000, 0x00008000,
		0x80100000, 0x80008000, 0x00000020, 0x80108020,
		0x00108020, 0x00000020, 0x00008000, 0x80000000,
		0x00008020, 0x80108000, 0x00100000, 0x80000020,
		0x00100020, 0x80008020, 0x80000020, 0x00100020,
		0x00108000, 0x00000000, 0x80008000, 0x00008020,
		0x80000000, 0x80100020, 0x80108020, 0x00108000
	};

	const uint32 SB3[64] = {
		0x00000208, 0x08020200, 0x00000000, 0x08020008,
		0x08000200, 0x00000000, 0x00020208, 0x08000200,
		0x00020008, 0x08000008, 0x08000008, 0x00020000,
		0x08020208, 0x00020008, 0x08020000, 0x00000208,
		0x08000000, 0x00000008, 0x08020200, 0x00000200,
		0x00020200, 0x08020000, 0x08020008, 0x00020208,
		0x08000208, 0x00020200, 0x00020000, 0x08000208,
		0x00000008, 0x08020208, 0x00000200, 0x08000000,
		0x08020200, 0x08000000, 0x00020008, 0x00000208,
		0x00020000, 0x08020200, 0x08000200, 0x00000000,
		0x00000200, 0x00020008, 0x08020208, 0x08000200,
		0x08000008, 0x00000200, 0x00000000, 0x08020008,
		0x08000208, 0x00020000, 0x08000000, 0x08020208,
		0x00000008, 0x00020208, 0x00020200, 0x08000008,
		0x08020000, 0x08000208, 0x00000208, 0x08020000,
		0x00020208, 0x00000008, 0x08020008, 0x00020200
	};

	const uint32 SB4[64] = {
		0x00802001, 0x00002081, 0x00002081, 0x00000080,
		0x00802080, 0x00800081, 0x00800001, 0x00002001,
		0x00000000, 0x00802000, 0x00802000, 0x00802081,
		0x00000081, 0x00000000, 0x00800080, 0x00800001,
		0x00000001, 0x00002000, 0x00800000, 0x00802001,
		0x00000080, 0x00800000, 0x00002001, 0x00002080,
		0x00800081, 0x00000001, 0x00002080, 0x00800080,
		0x00002000, 0x00802080, 0x00802081, 0x00000081,
		0x00800080, 0x00800001, 0x00802000, 0x00802081,
		0x00000081, 0x00000000, 0x00000000, 0x00802000,
		0x00002080, 0x00800080, 0x00800081, 0x00000001,
		0x00802001, 0x00002081, 0x00002081, 0x00000080,
		0x00802081, 0x00000081, 0x00000001, 0x00002000,
		0x00800001, 0x00002001, 0x00802080, 0x00800081,
		0x00002001, 0x00002080, 0x00800000, 0x00802001,
		0x00000080, 0x00800000, 0x00002000, 0x00802080
	};

	const uint32 SB5[64] = {
		0x00000100, 0x02080100, 0x02080000, 0x42000100,
		0x00080000, 0x00000100, 0x40000000, 0x02080000,
		0x40080100, 0x00080000, 0x02000100, 0x40080100,
		0x42000100, 0x42080000, 0x00080100, 0x40000000,
		0x02000000, 0x40080000, 0x40080000, 0x00000000,
		0x40000100, 0x42080100, 0x42080100, 0x02000100,
		0x42080000, 0x40000100, 0x00000000, 0x42000000,
		0x02080100, 0x02000000, 0x42000000, 0x00080100,
		0x00080000, 0x42000100, 0x00000100, 0x02000000,
		0x40000000, 0x02080000, 0x42000100, 0x40080100,
		0x02000100, 0x40000000, 0x42080000, 0x02080100,
		0x40080100, 0x00000100, 0x02000000, 0x42080000,
		0x42080100, 0x00080100, 0x42000000, 0x42080100,
		0x02080000, 0x00000000, 0x40080000, 0x42000000,
		0x00080100, 0x02000100, 0x40000100, 0x00080000,
		0x00000000, 0x40080000, 0x02080100, 0x40000100
	};

	const uint32 SB6[64] = {
		0x20000010, 0x20400000, 0x00004000, 0x20404010,
		0x20400000, 0x00000010, 0x20404010, 0x00400000,
		0x20004000, 0x00404010, 0x00400000, 0x20000010,
		0x00400010, 0x20004000, 0x20000000, 0x00004010,
		0x00000000, 0x00400010, 0x20004010, 0x00004000,
		0x00404000, 0x20004010, 0x00000010, 0x20400010,
		0x20400010, 0x00000000, 0x00404010, 0x20404000,
		0x00004010, 0x00404000, 0x20404000, 0x20000000,
		0x20004000, 0x00000010, 0x20400010, 0x00404000,
		0x20404010, 0x00400000, 0x00004010, 0x20000010,
		0x00400000, 0x20004000, 0x20000000, 0x00004010,
		0x20000010, 0x20404010, 0x00404000, 0x20400000,
		0x00404010, 0x20404000, 0x00000000, 0x20400010,
		0x00000010, 0x00004000, 0x20400000, 0x00404010,
		0x00004000, 0x00400010, 0x20004010, 0x00000000,
		0x20404000, 0x20000000, 0x00400010, 0x20004010
	};

	const uint32 SB7[64] = {
		0x00200000, 0x04200002, 0x04000802, 0x00000000,
		0x00000800, 0x04000802, 0x00200802, 0x04200800,
		0x04200802, 0x00200000, 0x00000000, 0x04000002,
		0x00000002, 0x04000000, 0x04200002, 0x00000802,
		0x04000800, 0x00200802, 0x00200002, 0x04000800,
		0x04000002, 0x04200000, 0x04200800, 0x00200002,
		0x04200000, 0x00000800, 0x00000802, 0x04200802,
		0x00200800, 0x00000002, 0x04000000, 0x00200800,
		0x04000000, 0x00200800, 0x00200000, 0x04000802,
		0x04000802, 0x04200002, 0x04200002, 0x00000002,
		0x00200002, 0x04000000, 0x04000800, 0x00200000,
		0x04200800, 0x00000802, 0x00200802, 0x04200800,
		0x00000802, 0x04000002, 0x04200802, 0x04200000,
		0x00200800, 0x00000000, 0x00000002, 0x04200802,
		0x00000000, 0x00200802, 0x04200000, 0x00000800,
		0x04000002, 0x04000800, 0x00000800, 0x00200002
	};

	const uint32 SB8[64] = {
		0x10001040, 0x00001000, 0x00040000, 0x10041040,
		0x10000000, 0x10001040, 0x00000040, 0x10000000,
		0x00040040, 0x10040000, 0x10041040, 0x00041000,
		0x10041000, 0x00041040, 0x00001000, 0x00000040,
		0x10040000, 0x10000040, 0x10001000, 0x00001040,
		0x00041000, 0x00040040, 0x10040040, 0x10041000,
		0x00001040, 0x00000000, 0x00000000, 0x10040040,
		0x10000040, 0x10001000, 0x00041040, 0x00040000,
		0x00041040, 0x00040000, 0x10041000, 0x00001000,
		0x00000040, 0x10040040, 0x00001000, 0x00041040,
		0x10001000, 0x00000040, 0x10000040, 0x10040000,
		0x10040040, 0x10000000, 0x00040000, 0x10001040,
		0x00000000, 0x10041040, 0x00040040, 0x10000040,
		0x10040000, 0x10001000, 0x10001040, 0x00000000,
		0x10041040, 0x00041000, 0x00041000, 0x00001040,
		0x00001040, 0x00040040, 0x10000000, 0x10041000
	};

	// PC1: left and right halves bit-swap

	const uint32 LHs[16] = {
		0x00000000, 0x00000001, 0x00000100, 0x00000101,
		0x00010000, 0x00010001, 0x00010100, 0x00010101,
		0x01000000, 0x01000001, 0x01000100, 0x01000101,
		0x01010000, 0x01010001, 0x01010100, 0x01010101
	};

	const uint32 RHs[16] = {
		0x00000000, 0x01000000, 0x00010000, 0x01010000,
		0x00000100, 0x01000100, 0x00010100, 0x01010100,
		0x00000001, 0x01000001, 0x00010001, 0x01010001,
		0x00000101, 0x01000101, 0x00010101, 0x01010101,
	};

	inline uint32 GetUint32(const uint8* B)
	{
		return (uint32(B[0]) << 24) | (uint32(B[1]) << 16) | (uint32(B[2]) << 8) | uint32(B[3]);
	}

	inline void PutUint32(uint32 N, uint8* B)
	{
		B[0] = uint8(N >> 24);
		B[1] = uint8(N >> 16);
		B[2] = uint8(N >> 8);
		B[3] = uint8(N);
	}

	void DesMainKs(uint32 SK[32], const uint8 Key[8])
	{
		uint32 X = GetUint32(Key);
		uint32 Y = GetUint32(Key + 4);
		uint32 T;

		T = ((Y >> 4) ^ X) & 0x0F0F0F0F; X ^= T; Y ^= (T << 4);
		T = ((Y) ^ X) & 0x10101010; X ^= T; Y ^= (T);

		X = (LHs[(X) & 0xF] << 3) | (LHs[(X >> 8) & 0xF] << 2)
			| (LHs[(X >> 16) & 0xF] << 1) | (LHs[(X >> 24) & 0xF])
			| (LHs[(X >> 5) & 0xF] << 7) | (LHs[(X >> 13) & 0xF] << 6)
			| (LHs[(X >> 21) & 0xF] << 5) | (LHs[(X >> 29) & 0xF] << 4);

		Y = (RHs[(Y >> 1) & 0xF] << 3) | (RHs[(Y >> 9) & 0xF] << 2)
			| (RHs[(Y >> 17) & 0xF] << 1) | (RHs[(Y >> 25) & 0xF])
			| (RHs[(Y >> 4) & 0xF] << 7) | (RHs[(Y >> 12) & 0xF] << 6)
			| (RHs[(Y >> 20) & 0xF] << 5) | (RHs[(Y >> 28) & 0xF] << 4);

		X &= 0x0FFFFFFF;
		Y &= 0x0FFFFFFF;

		for (int32 I = 0; I < 16; ++I)
		{
			if (I < 2 || I == 8 || I == 15)
			{
				X = ((X << 1) | (X >> 27)) & 0x0FFFFFFF;
				Y = ((Y << 1) | (Y >> 27)) & 0x0FFFFFFF;
			}
			else
			{
				X = ((X << 2) | (X >> 26)) & 0x0FFFFFFF;
				Y = ((Y << 2) | (Y >> 26)) & 0x0FFFFFFF;
			}

			*SK++ = ((X << 4) & 0x24000000) | ((X << 28) & 0x10000000)
				| ((X << 14) & 0x08000000) | ((X << 18) & 0x02080000)
				| ((X << 6) & 0x01000000) | ((X << 9) & 0x00200000)
				| ((X >> 1) & 0x00100000) | ((X << 10) & 0x00040000)
				| ((X << 2) & 0x00020000) | ((X >> 10) & 0x00010000)
				| ((Y >> 13) & 0x00002000) | ((Y >> 4) & 0x00001000)
				| ((Y << 6) & 0x00000800) | ((Y >> 1) & 0x00000400)
				| ((Y >> 14) & 0x00000200) | ((Y) & 0x00000100)
				| ((Y >> 5) & 0x00000020) | ((Y >> 10) & 0x00000010)
				| ((Y >> 3) & 0x00000008) | ((Y >> 18) & 0x00000004)
				| ((Y >> 26) & 0x00000002) | ((Y >> 24) & 0x00000001);

			*SK++ = ((X << 15) & 0x20000000) | ((X << 17) & 0x10000000)
				| ((X << 10) & 0x08000000) | ((X << 22) & 0x04000000)
				| ((X >> 2) & 0x02000000) | ((X << 1) & 0x01000000)
				| ((X << 16) & 0x00200000) | ((X << 11) & 0x00100000)
				| ((X << 3) & 0x00080000) | ((X >> 6) & 0x00040000)
				| ((X << 15) & 0x00020000) | ((X >> 4) & 0x00010000)
				| ((Y >> 2) & 0x00002000) | ((Y << 8) & 0x00001000)
				| ((Y >> 14) & 0x00000808) | ((Y >> 9) & 0x00000400)
				| ((Y) & 0x00000200) | ((Y << 7) & 0x00000100)
				| ((Y >> 7) & 0x00000020) | ((Y >> 3) & 0x00000011)
				| ((Y << 2) & 0x00000004) | ((Y >> 21) & 0x00000002);
		}
	}

	inline void DesRound(const uint32*& SK, uint32 X, uint32& Y)
	{
		uint32 T = *SK++ ^ X;
		Y ^= SB8[(T) & 0x3F] ^ SB6[(T >> 8) & 0x3F] ^ SB4[(T >> 16) & 0x3F] ^ SB2[(T >> 24) & 0x3F];
		T = *SK++ ^ ((X << 28) | (X >> 4));
		Y ^= SB7[(T) & 0x3F] ^ SB5[(T >> 8) & 0x3F] ^ SB3[(T >> 16) & 0x3F] ^ SB1[(T >> 24) & 0x3F];
	}

	void DesCrypt(const uint32 SKIn[32], const uint8 Input[8], uint8 Output[8])
	{
		uint32 X = GetUint32(Input);
		uint32 Y = GetUint32(Input + 4);
		uint32 T;

		// DES_IP
		T = ((X >> 4) ^ Y) & 0x0F0F0F0F; Y ^= T; X ^= (T << 4);
		T = ((X >> 16) ^ Y) & 0x0000FFFF; Y ^= T; X ^= (T << 16);
		T = ((Y >> 2) ^ X) & 0x33333333; X ^= T; Y ^= (T << 2);
		T = ((Y >> 8) ^ X) & 0x00FF00FF; X ^= T; Y ^= (T << 8);
		Y = ((Y << 1) | (Y >> 31)) & 0xFFFFFFFF;
		T = (X ^ Y) & 0xAAAAAAAA; Y ^= T; X ^= T;
		X = ((X << 1) | (X >> 31)) & 0xFFFFFFFF;

		const uint32* SK = SKIn;
		for (int32 Round = 0; Round < 8; ++Round)
		{
			DesRound(SK, Y, X);
			DesRound(SK, X, Y);
		}

		// DES_FP(Y, X)
		Y = ((Y << 31) | (Y >> 1)) & 0xFFFFFFFF;
		T = (Y ^ X) & 0xAAAAAAAA; Y ^= T; X ^= T;
		X = ((X << 31) | (X >> 1)) & 0xFFFFFFFF;
		T = ((X >> 8) ^ Y) & 0x00FF00FF; Y ^= T; X ^= (T << 8);
		T = ((X >> 2) ^ Y) & 0x33333333; Y ^= T; X ^= (T << 2);
		T = ((Y >> 16) ^ X) & 0x0000FFFF; X ^= T; Y ^= (T << 16);
		T = ((Y >> 4) ^ X) & 0x0F0F0F0F; X ^= T; Y ^= (T << 4);

		PutUint32(Y, Output);
		PutUint32(X, Output + 4);
	}

	const uint32 Md5K[64] = {
		0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
		0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
		0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
		0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
		0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
		0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
		0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
		0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
		0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
		0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
		0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
		0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
		0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
		0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
		0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
		0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};

	const uint32 Md5R[64] = {
		7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
		5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
		4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
		6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

	inline uint32 LeftRotate(uint32 X, uint32 C)
	{
		return (X << C) | (X >> (32 - C));
	}

	void DigestMd5(const uint32 W[16], uint32 Result[4])
	{
		uint32 A = 0x67452301u;
		uint32 B = 0xefcdab89u;
		uint32 C = 0x98badcfeu;
		uint32 D = 0x10325476u;
		for (int32 I = 0; I < 64; ++I)
		{
			uint32 F;
			int32 G;
			if (I < 16) { F = (B & C) | ((~B) & D); G = I; }
			else if (I < 32) { F = (D & B) | ((~D) & C); G = (5 * I + 1) % 16; }
			else if (I < 48) { F = B ^ C ^ D; G = (3 * I + 5) % 16; }
			else { F = C ^ (B | (~D)); G = (7 * I) % 16; }
			const uint32 Temp = D;
			D = C;
			C = B;
			B = B + LeftRotate(A + F + Md5K[I] + W[G], Md5R[I]);
			A = Temp;
		}
		Result[0] = A;
		Result[1] = B;
		Result[2] = C;
		Result[3] = D;
	}

	void Read64(const TArray<uint8>& In, uint32 Out[2])
	{
		const uint8* X = In.GetData();
		Out[0] = uint32(X[0]) | uint32(X[1]) << 8 | uint32(X[2]) << 16 | uint32(X[3]) << 24;
		Out[1] = uint32(X[4]) | uint32(X[5]) << 8 | uint32(X[6]) << 16 | uint32(X[7]) << 24;
	}

	TArray<uint8> Push64(uint64 R)
	{
		TArray<uint8> Out;
		Out.SetNumUninitialized(8);
		for (int32 I = 0; I < 8; ++I)
		{
			Out[I] = uint8((R >> (8 * I)) & 0xff);
		}
		return Out;
	}

	uint64 ToUint64(const TArray<uint8>& In)
	{
		uint32 Parts[2];
		Read64(In, Parts);
		return uint64(Parts[0]) | uint64(Parts[1]) << 32;
	}

	// The biggest 64-bit prime.
	constexpr uint64 DhPrime = 0xffffffffffffffc5ull;

	uint64 MulModP(uint64 A, uint64 B)
	{
		uint64 M = 0;
		while (B)
		{
			if (B & 1)
			{
				const uint64 T = DhPrime - A;
				if (M >= T) M -= T;
				else M += A;
			}
			if (A >= DhPrime - A) A = A * 2 - DhPrime;
			else A = A * 2;
			B >>= 1;
		}
		return M;
	}

	uint64 PowModP(uint64 A, uint64 B)
	{
		if (B == 1) return A;
		uint64 T = PowModP(A, B >> 1);
		T = MulModP(T, T);
		if (B % 2) T = MulModP(T, A);
		return T;
	}

	uint64 PowModPChecked(uint64 A, uint64 B)
	{
		if (A > DhPrime) A %= DhPrime;
		return PowModP(A, B);
	}

	void HashBytes(const uint8* Str, int32 Size, uint8 Key[8])
	{
		uint32 Djb = 5381u;
		uint32 Js = 1315423911u;
		for (int32 I = 0; I < Size; ++I)
		{
			const uint8 C = Str[I];
			Djb += (Djb << 5) + C;
			Js ^= ((Js << 5) + C + (Js >> 2));
		}
		for (int32 I = 0; I < 4; ++I)
		{
			Key[I] = uint8((Djb >> (8 * I)) & 0xff);
			Key[4 + I] = uint8((Js >> (8 * I)) & 0xff);
		}
	}
}

namespace RokSkynetCrypt
{
	TArray<uint8> RandomKey()
	{
		std::random_device Device;
		TArray<uint8> Out;
		Out.SetNumUninitialized(8);
		uint8 X = 0;
		for (int32 I = 0; I < 8; ++I)
		{
			Out[I] = uint8(Device() & 0xff);
			X ^= Out[I];
		}
		if (X == 0)
		{
			Out[0] |= 1;
		}
		return Out;
	}

	TArray<uint8> DhExchange(const TArray<uint8>& PrivateKey)
	{
		if (PrivateKey.Num() != 8) return {};
		const uint64 X = ToUint64(PrivateKey);
		if (X == 0) return {};
		return Push64(PowModPChecked(5, X));
	}

	TArray<uint8> DhSecret(const TArray<uint8>& OtherPublic, const TArray<uint8>& OwnPrivate)
	{
		if (OtherPublic.Num() != 8 || OwnPrivate.Num() != 8) return {};
		const uint64 X = ToUint64(OtherPublic);
		const uint64 Y = ToUint64(OwnPrivate);
		if (X == 0 || Y == 0) return {};
		return Push64(PowModPChecked(X, Y));
	}

	TArray<uint8> Hmac64(const TArray<uint8>& InX, const TArray<uint8>& InY)
	{
		if (InX.Num() != 8 || InY.Num() != 8) return {};
		uint32 X[2], Y[2];
		Read64(InX, X);
		Read64(InY, Y);
		uint32 W[16];
		for (int32 I = 0; I < 16; I += 4)
		{
			W[I] = X[1];
			W[I + 1] = X[0];
			W[I + 2] = Y[1];
			W[I + 3] = Y[0];
		}
		uint32 R[4];
		DigestMd5(W, R);
		const uint32 Result0 = R[2] ^ R[3];
		const uint32 Result1 = R[0] ^ R[1];
		return Push64(uint64(Result0) | uint64(Result1) << 32);
	}

	TArray<uint8> HashKey(const TArray<uint8>& Text)
	{
		TArray<uint8> Out;
		Out.SetNumUninitialized(8);
		HashBytes(Text.GetData(), Text.Num(), Out.GetData());
		return Out;
	}

	bool DesEncode(const TArray<uint8>& Key, const uint8* Text, int32 Size, TArray<uint8>& Out)
	{
		Out.Reset();
		if (Key.Num() != 8 || Size < 0 || (Size > 0 && !Text)) return false;
		uint32 SK[32];
		DesMainKs(SK, Key.GetData());
		const int32 ChunkSize = (Size + 8) & ~7;
		Out.SetNumUninitialized(ChunkSize);
		int32 I = 0;
		for (; I < Size - 7; I += 8)
		{
			DesCrypt(SK, Text + I, Out.GetData() + I);
		}
		// ISO 7816-4 padding: 0x80 then zeros.
		uint8 Tail[8];
		const int32 Offset = Size - I;
		FMemory::Memcpy(Tail, Text + I, Offset);
		Tail[Offset] = 0x80;
		FMemory::Memzero(Tail + Offset + 1, 7 - Offset);
		DesCrypt(SK, Tail, Out.GetData() + I);
		return true;
	}

	bool DesDecode(const TArray<uint8>& Key, const uint8* Text, int32 Size, TArray<uint8>& Out)
	{
		Out.Reset();
		if (Key.Num() != 8 || Size <= 0 || (Size & 7) || !Text) return false;
		uint32 ESK[32];
		DesMainKs(ESK, Key.GetData());
		uint32 SK[32];
		for (int32 I = 0; I < 32; I += 2)
		{
			SK[I] = ESK[30 - I];
			SK[I + 1] = ESK[31 - I];
		}
		Out.SetNumUninitialized(Size);
		for (int32 I = 0; I < Size; I += 8)
		{
			DesCrypt(SK, Text + I, Out.GetData() + I);
		}
		// Remove ISO 7816-4 padding.
		int32 Padding = 0;
		{
			int32 Count = 1;
			const uint8* Last = Out.GetData() + Size - 1;
			for (int32 I = 0; I < 8; ++I, --Last)
			{
				if (*Last == 0) ++Count;
				else if (*Last == 0x80) { Padding = Count; break; }
				else break;
			}
		}
		if (Padding <= 0 || Padding > 8)
		{
			Out.Reset();
			return false;
		}
		Out.SetNum(Size - Padding);
		return true;
	}

	TArray<uint8> Utf8Bytes(const FString& Text)
	{
		const FTCHARToUTF8 Converter(*Text);
		return TArray<uint8>(reinterpret_cast<const uint8*>(Converter.Get()), Converter.Length());
	}
}
