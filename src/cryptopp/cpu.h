





#ifndef CRYPTOPP_CPU_H
#define CRYPTOPP_CPU_H

#include "config.h"


#if (CRYPTOPP_BOOL_ARM32 || CRYPTOPP_BOOL_ARM64)
# if defined(__GNUC__)
#  include <stdint.h>
# endif
# if CRYPTOPP_BOOL_NEON_INTRINSICS_AVAILABLE || defined(__ARM_NEON)
#  include <arm_neon.h>
# endif
# if (CRYPTOPP_BOOL_ARM_CRYPTO_INTRINSICS_AVAILABLE || CRYPTOPP_BOOL_ARM_CRC32_INTRINSICS_AVAILABLE) || defined(__ARM_ACLE)
#  include <arm_acle.h>
# endif
#endif  


#if CRYPTOPP_BOOL_X86 || CRYPTOPP_BOOL_X32 || CRYPTOPP_BOOL_X64


#if (CRYPTOPP_GCC_VERSION >= 40800)
#  include <x86intrin.h>
#endif
#if (CRYPTOPP_MSC_VERSION >= 1400)
#  include <intrin.h>
#endif


#if CRYPTOPP_BOOL_SSE2_ASM_AVAILABLE || CRYPTOPP_BOOL_SSE2_INTRINSICS_AVAILABLE
#  include <emmintrin.h>    
#endif
#if CRYPTOPP_BOOL_SSSE3_ASM_AVAILABLE
#  include <tmmintrin.h>    
#endif 
#if CRYPTOPP_BOOL_SSE4_INTRINSICS_AVAILABLE
#  include <smmintrin.h>    
#  include <nmmintrin.h>    
#endif 
#if CRYPTOPP_BOOL_AESNI_INTRINSICS_AVAILABLE
#  include <wmmintrin.h>    
#endif 
#if CRYPTOPP_BOOL_AVX_INTRINSICS_AVAILABLE
#  include <immintrin.h>    
#endif
#if CRYPTOPP_BOOL_AVX2_INTRINSICS_AVAILABLE
#  include <zmmintrin.h>    
#endif
#endif  


#if defined(_MSC_VER) || defined(__BORLANDC__)
# define CRYPTOPP_MS_STYLE_INLINE_ASSEMBLY
#else
# define CRYPTOPP_GNU_STYLE_INLINE_ASSEMBLY
#endif


#if defined(CRYPTOPP_LLVM_CLANG_VERSION) || defined(CRYPTOPP_APPLE_CLANG_VERSION) || defined(CRYPTOPP_CLANG_INTEGRATED_ASSEMBLER)
	#define NEW_LINE "\n"
	#define INTEL_PREFIX ".intel_syntax;"
	#define INTEL_NOPREFIX ".intel_syntax;"
	#define ATT_PREFIX ".att_syntax;"
	#define ATT_NOPREFIX ".att_syntax;"
#elif defined(__GNUC__)
	#define NEW_LINE
	#define INTEL_PREFIX ".intel_syntax prefix;"
	#define INTEL_NOPREFIX ".intel_syntax noprefix;"
	#define ATT_PREFIX ".att_syntax prefix;"
	#define ATT_NOPREFIX ".att_syntax noprefix;"
#else
	#define NEW_LINE
	#define INTEL_PREFIX
	#define INTEL_NOPREFIX
	#define ATT_PREFIX
	#define ATT_NOPREFIX
#endif

#ifdef CRYPTOPP_GENERATE_X64_MASM

#define CRYPTOPP_X86_ASM_AVAILABLE
#define CRYPTOPP_BOOL_X64 1
#define CRYPTOPP_BOOL_SSE2_ASM_AVAILABLE 1
#define NAMESPACE_END

#else

NAMESPACE_BEGIN(CryptoPP)

#if CRYPTOPP_BOOL_X86 || CRYPTOPP_BOOL_X32 || CRYPTOPP_BOOL_X64 || CRYPTOPP_DOXYGEN_PROCESSING

#define CRYPTOPP_CPUID_AVAILABLE


#ifndef CRYPTOPP_DOXYGEN_PROCESSING

extern CRYPTOPP_DLL bool g_x86DetectionDone;
extern CRYPTOPP_DLL bool g_hasMMX;
extern CRYPTOPP_DLL bool g_hasISSE;
extern CRYPTOPP_DLL bool g_hasSSE2;
extern CRYPTOPP_DLL bool g_hasSSSE3;
extern CRYPTOPP_DLL bool g_hasSSE4;
extern CRYPTOPP_DLL bool g_hasAESNI;
extern CRYPTOPP_DLL bool g_hasCLMUL;
extern CRYPTOPP_DLL bool g_isP4;
extern CRYPTOPP_DLL bool g_hasRDRAND;
extern CRYPTOPP_DLL bool g_hasRDSEED;
extern CRYPTOPP_DLL bool g_hasPadlockRNG;
extern CRYPTOPP_DLL bool g_hasPadlockACE;
extern CRYPTOPP_DLL bool g_hasPadlockACE2;
extern CRYPTOPP_DLL bool g_hasPadlockPHE;
extern CRYPTOPP_DLL bool g_hasPadlockPMM;
extern CRYPTOPP_DLL word32 g_cacheLineSize;

CRYPTOPP_DLL void CRYPTOPP_API DetectX86Features();
CRYPTOPP_DLL bool CRYPTOPP_API CpuId(word32 input, word32 output[4]);
#endif 





inline bool HasMMX()
{
#if CRYPTOPP_BOOL_X64
	return true;
#else
	if (!g_x86DetectionDone)
		DetectX86Features();
	return g_hasMMX;
#endif
}





inline bool HasISSE()
{
#if CRYPTOPP_BOOL_X64
	return true;
#else
	if (!g_x86DetectionDone)
		DetectX86Features();
	return g_hasISSE;
#endif
}





inline bool HasSSE2()
{
#if CRYPTOPP_BOOL_X64
	return true;
#else
	if (!g_x86DetectionDone)
		DetectX86Features();
	return g_hasSSE2;
#endif
}





inline bool HasSSSE3()
{
	if (!g_x86DetectionDone)
		DetectX86Features();
	return g_hasSSSE3;
}




inline bool HasSSE4()
{
	if (!g_x86DetectionDone)
		DetectX86Features();
	return g_hasSSE4;
}




inline bool HasAESNI()
{
	if (!g_x86DetectionDone)
		DetectX86Features();
	return g_hasAESNI;
}




inline bool HasCLMUL()
{
	if (!g_x86DetectionDone)
		DetectX86Features();
	return g_hasCLMUL;
}




inline bool IsP4()
{
	if (!g_x86DetectionDone)
		DetectX86Features();
	return g_isP4;
}




inline bool HasRDRAND()
{
	if (!g_x86DetectionDone)
		DetectX86Features();
	return g_hasRDRAND;
}




inline bool HasRDSEED()
{
	if (!g_x86DetectionDone)
		DetectX86Features();
	return g_hasRDSEED;
}




inline bool HasPadlockRNG()
{
	if (!g_x86DetectionDone)
		DetectX86Features();
	return g_hasPadlockRNG;
}




inline bool HasPadlockACE()
{
	if (!g_x86DetectionDone)
		DetectX86Features();
	return g_hasPadlockACE;
}




inline bool HasPadlockACE2()
{
	if (!g_x86DetectionDone)
		DetectX86Features();
	return g_hasPadlockACE2;
}




inline bool HasPadlockPHE()
{
	if (!g_x86DetectionDone)
		DetectX86Features();
	return g_hasPadlockPHE;
}




inline bool HasPadlockPMM()
{
	if (!g_x86DetectionDone)
		DetectX86Features();
	return g_hasPadlockPMM;
}








inline int GetCacheLineSize()
{
	if (!g_x86DetectionDone)
		DetectX86Features();
	return g_cacheLineSize;
}

#elif (CRYPTOPP_BOOL_ARM32 || CRYPTOPP_BOOL_ARM64)

extern bool g_ArmDetectionDone;
extern bool g_hasNEON, g_hasPMULL, g_hasCRC32, g_hasAES, g_hasSHA1, g_hasSHA2;
void CRYPTOPP_API DetectArmFeatures();







inline bool HasNEON()
{
	if (!g_ArmDetectionDone)
		DetectArmFeatures();
	return g_hasNEON;
}







inline bool HasPMULL()
{
	if (!g_ArmDetectionDone)
		DetectArmFeatures();
	return g_hasPMULL;
}









inline bool HasCRC32()
{
	if (!g_ArmDetectionDone)
		DetectArmFeatures();
	return g_hasCRC32;
}









inline bool HasAES()
{
	if (!g_ArmDetectionDone)
		DetectArmFeatures();
	return g_hasAES;
}









inline bool HasSHA1()
{
	if (!g_ArmDetectionDone)
		DetectArmFeatures();
	return g_hasSHA1;
}









inline bool HasSHA2()
{
	if (!g_ArmDetectionDone)
		DetectArmFeatures();
	return g_hasSHA2;
}





inline int GetCacheLineSize()
{
	return CRYPTOPP_L1_CACHE_LINE_SIZE;
}

#else

inline int GetCacheLineSize()
{
	return CRYPTOPP_L1_CACHE_LINE_SIZE;
}

#endif  

#endif

#if CRYPTOPP_BOOL_X86 || CRYPTOPP_BOOL_X32 || CRYPTOPP_BOOL_X64

#ifdef CRYPTOPP_GENERATE_X64_MASM
	#define AS1(x) x*newline*
	#define AS2(x, y) x, y*newline*
	#define AS3(x, y, z) x, y, z*newline*
	#define ASS(x, y, a, b, c, d) x, y, a*64+b*16+c*4+d*newline*
	#define ASL(x) label##x:*newline*
	#define ASJ(x, y, z) x label##y*newline*
	#define ASC(x, y) x label##y*newline*
	#define AS_HEX(y) 0##y##h
#elif defined(_MSC_VER) || defined(__BORLANDC__)
	#define CRYPTOPP_MS_STYLE_INLINE_ASSEMBLY
	#define AS1(x) __asm {x}
	#define AS2(x, y) __asm {x, y}
	#define AS3(x, y, z) __asm {x, y, z}
	#define ASS(x, y, a, b, c, d) __asm {x, y, (a)*64+(b)*16+(c)*4+(d)}
	#define ASL(x) __asm {label##x:}
	#define ASJ(x, y, z) __asm {x label##y}
	#define ASC(x, y) __asm {x label##y}
	#define CRYPTOPP_NAKED __declspec(naked)
	#define AS_HEX(y) 0x##y
#else
	#define CRYPTOPP_GNU_STYLE_INLINE_ASSEMBLY

	
	#define GNU_AS1(x) #x ";" NEW_LINE
	#define GNU_AS2(x, y) #x ", " #y ";" NEW_LINE
	#define GNU_AS3(x, y, z) #x ", " #y ", " #z ";" NEW_LINE
	#define GNU_ASL(x) "\n" #x ":" NEW_LINE
	#define GNU_ASJ(x, y, z) #x " " #y #z ";" NEW_LINE
	#define AS1(x) GNU_AS1(x)
	#define AS2(x, y) GNU_AS2(x, y)
	#define AS3(x, y, z) GNU_AS3(x, y, z)
	#define ASS(x, y, a, b, c, d) #x ", " #y ", " #a "*64+" #b "*16+" #c "*4+" #d ";"
	#define ASL(x) GNU_ASL(x)
	#define ASJ(x, y, z) GNU_ASJ(x, y, z)
	#define ASC(x, y) #x " " #y ";"
	#define CRYPTOPP_NAKED
	#define AS_HEX(y) 0x##y
#endif

#define IF0(y)
#define IF1(y) y

#ifdef CRYPTOPP_GENERATE_X64_MASM
#define ASM_MOD(x, y) ((x) MOD (y))
#define XMMWORD_PTR XMMWORD PTR
#else

#define ASM_MOD(x, y) ((x)-((x)/(y))*(y))

#define XMMWORD_PTR
#endif

#if CRYPTOPP_BOOL_X86
	#define AS_REG_1 ecx
	#define AS_REG_2 edx
	#define AS_REG_3 esi
	#define AS_REG_4 edi
	#define AS_REG_5 eax
	#define AS_REG_6 ebx
	#define AS_REG_7 ebp
	#define AS_REG_1d ecx
	#define AS_REG_2d edx
	#define AS_REG_3d esi
	#define AS_REG_4d edi
	#define AS_REG_5d eax
	#define AS_REG_6d ebx
	#define AS_REG_7d ebp
	#define WORD_SZ 4
	#define WORD_REG(x)	e##x
	#define WORD_PTR DWORD PTR
	#define AS_PUSH_IF86(x) AS1(push e##x)
	#define AS_POP_IF86(x) AS1(pop e##x)
	#define AS_JCXZ jecxz
#elif CRYPTOPP_BOOL_X32
	#define AS_REG_1 ecx
	#define AS_REG_2 edx
	#define AS_REG_3 r8d
	#define AS_REG_4 r9d
	#define AS_REG_5 eax
	#define AS_REG_6 r10d
	#define AS_REG_7 r11d
	#define AS_REG_1d ecx
	#define AS_REG_2d edx
	#define AS_REG_3d r8d
	#define AS_REG_4d r9d
	#define AS_REG_5d eax
	#define AS_REG_6d r10d
	#define AS_REG_7d r11d
	#define WORD_SZ 4
	#define WORD_REG(x)	e##x
	#define WORD_PTR DWORD PTR
	#define AS_PUSH_IF86(x) AS1(push r##x)
	#define AS_POP_IF86(x) AS1(pop r##x)
	#define AS_JCXZ jecxz
#elif CRYPTOPP_BOOL_X64
	#ifdef CRYPTOPP_GENERATE_X64_MASM
		#define AS_REG_1 rcx
		#define AS_REG_2 rdx
		#define AS_REG_3 r8
		#define AS_REG_4 r9
		#define AS_REG_5 rax
		#define AS_REG_6 r10
		#define AS_REG_7 r11
		#define AS_REG_1d ecx
		#define AS_REG_2d edx
		#define AS_REG_3d r8d
		#define AS_REG_4d r9d
		#define AS_REG_5d eax
		#define AS_REG_6d r10d
		#define AS_REG_7d r11d
	#else
		#define AS_REG_1 rdi
		#define AS_REG_2 rsi
		#define AS_REG_3 rdx
		#define AS_REG_4 rcx
		#define AS_REG_5 r8
		#define AS_REG_6 r9
		#define AS_REG_7 r10
		#define AS_REG_1d edi
		#define AS_REG_2d esi
		#define AS_REG_3d edx
		#define AS_REG_4d ecx
		#define AS_REG_5d r8d
		#define AS_REG_6d r9d
		#define AS_REG_7d r10d
	#endif
	#define WORD_SZ 8
	#define WORD_REG(x)	r##x
	#define WORD_PTR QWORD PTR
	#define AS_PUSH_IF86(x)
	#define AS_POP_IF86(x)
	#define AS_JCXZ jrcxz
#endif


#define AS_XMM_OUTPUT4(labelPrefix, inputPtr, outputPtr, x0, x1, x2, x3, t, p0, p1, p2, p3, increment)\
	AS2(	test	inputPtr, inputPtr)\
	ASC(	jz,		labelPrefix##3)\
	AS2(	test	inputPtr, 15)\
	ASC(	jnz,	labelPrefix##7)\
	AS2(	pxor	xmm##x0, [inputPtr+p0*16])\
	AS2(	pxor	xmm##x1, [inputPtr+p1*16])\
	AS2(	pxor	xmm##x2, [inputPtr+p2*16])\
	AS2(	pxor	xmm##x3, [inputPtr+p3*16])\
	AS2(	add		inputPtr, increment*16)\
	ASC(	jmp,	labelPrefix##3)\
	ASL(labelPrefix##7)\
	AS2(	movdqu	xmm##t, [inputPtr+p0*16])\
	AS2(	pxor	xmm##x0, xmm##t)\
	AS2(	movdqu	xmm##t, [inputPtr+p1*16])\
	AS2(	pxor	xmm##x1, xmm##t)\
	AS2(	movdqu	xmm##t, [inputPtr+p2*16])\
	AS2(	pxor	xmm##x2, xmm##t)\
	AS2(	movdqu	xmm##t, [inputPtr+p3*16])\
	AS2(	pxor	xmm##x3, xmm##t)\
	AS2(	add		inputPtr, increment*16)\
	ASL(labelPrefix##3)\
	AS2(	test	outputPtr, 15)\
	ASC(	jnz,	labelPrefix##8)\
	AS2(	movdqa	[outputPtr+p0*16], xmm##x0)\
	AS2(	movdqa	[outputPtr+p1*16], xmm##x1)\
	AS2(	movdqa	[outputPtr+p2*16], xmm##x2)\
	AS2(	movdqa	[outputPtr+p3*16], xmm##x3)\
	ASC(	jmp,	labelPrefix##9)\
	ASL(labelPrefix##8)\
	AS2(	movdqu	[outputPtr+p0*16], xmm##x0)\
	AS2(	movdqu	[outputPtr+p1*16], xmm##x1)\
	AS2(	movdqu	[outputPtr+p2*16], xmm##x2)\
	AS2(	movdqu	[outputPtr+p3*16], xmm##x3)\
	ASL(labelPrefix##9)\
	AS2(	add		outputPtr, increment*16)

#endif  

NAMESPACE_END

#endif  
