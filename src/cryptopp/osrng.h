




#ifndef CRYPTOPP_OSRNG_H
#define CRYPTOPP_OSRNG_H

#include "config.h"

#if !defined(OS_NO_DEPENDENCE) && defined(OS_RNG_AVAILABLE)

#include "cryptlib.h"
#include "randpool.h"
#include "smartptr.h"
#include "fips140.h"
#include "rng.h"
#include "aes.h"
#include "sha.h"

NAMESPACE_BEGIN(CryptoPP)



class CRYPTOPP_DLL OS_RNG_Err : public Exception
{
public:
	
	
	OS_RNG_Err(const std::string &operation);
};

#ifdef NONBLOCKING_RNG_AVAILABLE

#ifdef CRYPTOPP_WIN32_AVAILABLE



class CRYPTOPP_DLL MicrosoftCryptoProvider
{
public:
	
	MicrosoftCryptoProvider();
	~MicrosoftCryptoProvider();


#if defined(USE_MS_CRYPTOAPI)
# if defined(__CYGWIN__) && defined(__x86_64__)
	typedef unsigned long long ProviderHandle;
# elif defined(WIN64) || defined(_WIN64)
	typedef unsigned __int64 ProviderHandle;
# else
	typedef unsigned long ProviderHandle;
# endif
#elif defined(USE_MS_CNGAPI)
	typedef void *PVOID;
	typedef PVOID ProviderHandle;
#endif 

	
	
	
	
	
	
	
	ProviderHandle GetProviderHandle() const {return m_hProvider;}

private:
	ProviderHandle m_hProvider;
};

#if defined(_MSC_VER) && defined(USE_MS_CRYPTOAPI)
# pragma comment(lib, "advapi32.lib")
#endif

#if defined(_MSC_VER) && defined(USE_MS_CNGAPI)
# pragma comment(lib, "bcrypt.lib")
#endif

#endif 





class CRYPTOPP_DLL NonblockingRng : public RandomNumberGenerator
{
public:
	
	NonblockingRng();
	~NonblockingRng();

	
	
	
	
	void GenerateBlock(byte *output, size_t size);

protected:
#ifdef CRYPTOPP_WIN32_AVAILABLE
	MicrosoftCryptoProvider m_Provider;
#else
	int m_fd;
#endif
};

#endif

#if defined(BLOCKING_RNG_AVAILABLE) || defined(CRYPTOPP_DOXYGEN_PROCESSING)




class CRYPTOPP_DLL BlockingRng : public RandomNumberGenerator
{
public:
	
	BlockingRng();
	~BlockingRng();

	
	
	
	
	void GenerateBlock(byte *output, size_t size);

protected:
	int m_fd;
};

#endif











CRYPTOPP_DLL void CRYPTOPP_API OS_GenerateRandomBlock(bool blocking, byte *output, size_t size);






class CRYPTOPP_DLL AutoSeededRandomPool : public RandomPool
{
public:
	
	
	
	
	
	explicit AutoSeededRandomPool(bool blocking = false, unsigned int seedSize = 32)
		{Reseed(blocking, seedSize);}

	
	
	
	void Reseed(bool blocking = false, unsigned int seedSize = 32);
};










template <class BLOCK_CIPHER>
class AutoSeededX917RNG : public RandomNumberGenerator, public NotCopyable
{
public:
	
	
	
	
	
	
	explicit AutoSeededX917RNG(bool blocking = false, bool autoSeed = true)
		{if (autoSeed) Reseed(blocking);}

	
	
	
	
	
	
	
	void Reseed(bool blocking = false, const byte *additionalEntropy = NULL, size_t length = 0);

	
	
	
	
	
	
	
	void Reseed(const byte *key, size_t keylength, const byte *seed, const byte *timeVector);

	bool CanIncorporateEntropy() const {return true;}
	void IncorporateEntropy(const byte *input, size_t length) {Reseed(false, input, length);}
	void GenerateIntoBufferedTransformation(BufferedTransformation &target, const std::string &channel, lword length)
		{m_rng->GenerateIntoBufferedTransformation(target, channel, length);}

private:
	member_ptr<RandomNumberGenerator> m_rng;
};

template <class BLOCK_CIPHER>
void AutoSeededX917RNG<BLOCK_CIPHER>::Reseed(const byte *key, size_t keylength, const byte *seed, const byte *timeVector)
{
	m_rng.reset(new X917RNG(new typename BLOCK_CIPHER::Encryption(key, keylength), seed, timeVector));
}

template <class BLOCK_CIPHER>
void AutoSeededX917RNG<BLOCK_CIPHER>::Reseed(bool blocking, const byte *input, size_t length)
{
	SecByteBlock seed(BLOCK_CIPHER::BLOCKSIZE + BLOCK_CIPHER::DEFAULT_KEYLENGTH);
	const byte *key;
	do
	{
		OS_GenerateRandomBlock(blocking, seed, seed.size());
		if (length > 0)
		{
			SHA256 hash;
			hash.Update(seed, seed.size());
			hash.Update(input, length);
			hash.TruncatedFinal(seed, UnsignedMin(hash.DigestSize(), seed.size()));
		}
		key = seed + BLOCK_CIPHER::BLOCKSIZE;
	}	
	while (memcmp(key, seed, STDMIN((unsigned int)BLOCK_CIPHER::BLOCKSIZE, (unsigned int)BLOCK_CIPHER::DEFAULT_KEYLENGTH)) == 0);

	Reseed(key, BLOCK_CIPHER::DEFAULT_KEYLENGTH, seed, NULL);
}

CRYPTOPP_DLL_TEMPLATE_CLASS AutoSeededX917RNG<AES>;

#if defined(CRYPTOPP_DOXYGEN_PROCESSING)





class DefaultAutoSeededRNG {}
#else

#if CRYPTOPP_ENABLE_COMPLIANCE_WITH_FIPS_140_2
typedef AutoSeededX917RNG<AES> DefaultAutoSeededRNG;
#else
typedef AutoSeededRandomPool DefaultAutoSeededRNG;
#endif
#endif 

NAMESPACE_END

#endif

#endif
