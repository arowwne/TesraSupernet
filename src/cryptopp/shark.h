




#ifndef CRYPTOPP_SHARK_H
#define CRYPTOPP_SHARK_H

#include "config.h"
#include "seckey.h"
#include "secblock.h"

NAMESPACE_BEGIN(CryptoPP)



struct SHARK_Info : public FixedBlockSize<8>, public FixedKeyLength<16>, public VariableRounds<6, 2>
{
	CRYPTOPP_CONSTEXPR static const char *StaticAlgorithmName() {return "SHARK-E";}
};




class SHARK : public SHARK_Info, public BlockCipherDocumentation
{
	
	
	class CRYPTOPP_NO_VTABLE Base : public BlockCipherImpl<SHARK_Info>
	{
	public:
		void UncheckedSetKey(const byte *key, unsigned int length, const NameValuePairs &param);

	protected:
		unsigned int m_rounds;
		SecBlock<word64> m_roundKeys;
	};

	
	
	class CRYPTOPP_NO_VTABLE Enc : public Base
	{
	public:
		void ProcessAndXorBlock(const byte *inBlock, const byte *xorBlock, byte *outBlock) const;

		
		void InitForKeySetup();

	private:
		static const byte sbox[256];
		static const word64 cbox[8][256];
	};

	
	
	class CRYPTOPP_NO_VTABLE Dec : public Base
	{
	public:
		void ProcessAndXorBlock(const byte *inBlock, const byte *xorBlock, byte *outBlock) const;

	private:
		static const byte sbox[256];
		static const word64 cbox[8][256];
	};

public:
	typedef BlockCipherFinal<ENCRYPTION, Enc> Encryption;
	typedef BlockCipherFinal<DECRYPTION, Dec> Decryption;
};

typedef SHARK::Encryption SHARKEncryption;
typedef SHARK::Decryption SHARKDecryption;

NAMESPACE_END

#endif
