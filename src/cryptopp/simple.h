




#ifndef CRYPTOPP_SIMPLE_H
#define CRYPTOPP_SIMPLE_H

#include "config.h"

#if CRYPTOPP_MSC_VERSION
# pragma warning(push)
# pragma warning(disable: 4127 4189)
#endif

#include "cryptlib.h"
#include "misc.h"

NAMESPACE_BEGIN(CryptoPP)





template <class DERIVED, class BASE>
class CRYPTOPP_NO_VTABLE ClonableImpl : public BASE
{
public:
	Clonable * Clone() const {return new DERIVED(*static_cast<const DERIVED *>(this));}
};






template <class BASE, class ALGORITHM_INFO=BASE>
class CRYPTOPP_NO_VTABLE AlgorithmImpl : public BASE
{
public:
	static std::string CRYPTOPP_API StaticAlgorithmName() {return ALGORITHM_INFO::StaticAlgorithmName();}
	std::string AlgorithmName() const {return ALGORITHM_INFO::StaticAlgorithmName();}
};



class CRYPTOPP_DLL InvalidKeyLength : public InvalidArgument
{
public:
	explicit InvalidKeyLength(const std::string &algorithm, size_t length) : InvalidArgument(algorithm + ": " + IntToString(length) + " is not a valid key length") {}
};



class CRYPTOPP_DLL InvalidRounds : public InvalidArgument
{
public:
	explicit InvalidRounds(const std::string &algorithm, unsigned int rounds) : InvalidArgument(algorithm + ": " + IntToString(rounds) + " is not a valid number of rounds") {}
};



class CRYPTOPP_DLL InvalidPersonalizationLength : public InvalidArgument
{
public:
	explicit InvalidPersonalizationLength(const std::string &algorithm, size_t length) : InvalidArgument(algorithm + ": " + IntToString(length) + " is not a valid salt length") {}
};



class CRYPTOPP_DLL InvalidSaltLength : public InvalidArgument
{
public:
	explicit InvalidSaltLength(const std::string &algorithm, size_t length) : InvalidArgument(algorithm + ": " + IntToString(length) + " is not a valid salt length") {}
};






template <class T>
class CRYPTOPP_NO_VTABLE Bufferless : public T
{
public:
	bool IsolatedFlush(bool hardFlush, bool blocking)
		{CRYPTOPP_UNUSED(hardFlush); CRYPTOPP_UNUSED(blocking); return false;}
};




template <class T>
class CRYPTOPP_NO_VTABLE Unflushable : public T
{
public:
	bool Flush(bool completeFlush, int propagation=-1, bool blocking=true)
		{return ChannelFlush(DEFAULT_CHANNEL, completeFlush, propagation, blocking);}
	bool IsolatedFlush(bool hardFlush, bool blocking)
		{CRYPTOPP_UNUSED(hardFlush); CRYPTOPP_UNUSED(blocking); CRYPTOPP_ASSERT(false); return false;}
	bool ChannelFlush(const std::string &channel, bool hardFlush, int propagation=-1, bool blocking=true)
	{
		if (hardFlush && !InputBufferIsEmpty())
			throw CannotFlush("Unflushable<T>: this object has buffered input that cannot be flushed");
		else
		{
			BufferedTransformation *attached = this->AttachedTransformation();
			return attached && propagation ? attached->ChannelFlush(channel, hardFlush, propagation-1, blocking) : false;
		}
	}

protected:
	virtual bool InputBufferIsEmpty() const {return false;}
};





template <class T>
class CRYPTOPP_NO_VTABLE InputRejecting : public T
{
public:
	struct InputRejected : public NotImplemented
		{InputRejected() : NotImplemented("BufferedTransformation: this object doesn't allow input") {}};

	
	

	
	
	
	
	
	
	
	
	size_t Put2(const byte *inString, size_t length, int messageEnd, bool blocking)
		{CRYPTOPP_UNUSED(inString); CRYPTOPP_UNUSED(length); CRYPTOPP_UNUSED(messageEnd); CRYPTOPP_UNUSED(blocking); throw InputRejected();}
	

	
	
	bool IsolatedFlush(bool hardFlush, bool blocking)
		{CRYPTOPP_UNUSED(hardFlush); CRYPTOPP_UNUSED(blocking); return false;}
	bool IsolatedMessageSeriesEnd(bool blocking)
		{CRYPTOPP_UNUSED(blocking); throw InputRejected();}
	size_t ChannelPut2(const std::string &channel, const byte *inString, size_t length, int messageEnd, bool blocking)
		{CRYPTOPP_UNUSED(channel); CRYPTOPP_UNUSED(inString); CRYPTOPP_UNUSED(length); CRYPTOPP_UNUSED(messageEnd); CRYPTOPP_UNUSED(blocking); throw InputRejected();}
	bool ChannelMessageSeriesEnd(const std::string& channel, int messageEnd, bool blocking)
		{CRYPTOPP_UNUSED(channel); CRYPTOPP_UNUSED(messageEnd); CRYPTOPP_UNUSED(blocking); throw InputRejected();}
	
};





template <class T>
class CRYPTOPP_NO_VTABLE CustomFlushPropagation : public T
{
public:
	
	
	virtual bool Flush(bool hardFlush, int propagation=-1, bool blocking=true) =0;
	

private:
	bool IsolatedFlush(bool hardFlush, bool blocking)
		{CRYPTOPP_UNUSED(hardFlush); CRYPTOPP_UNUSED(blocking); CRYPTOPP_ASSERT(false); return false;}
};





template <class T>
class CRYPTOPP_NO_VTABLE CustomSignalPropagation : public CustomFlushPropagation<T>
{
public:
	virtual void Initialize(const NameValuePairs &parameters=g_nullNameValuePairs, int propagation=-1) =0;

private:
	void IsolatedInitialize(const NameValuePairs &parameters)
		{CRYPTOPP_UNUSED(parameters); CRYPTOPP_ASSERT(false);}
};





template <class T>
class CRYPTOPP_NO_VTABLE Multichannel : public CustomFlushPropagation<T>
{
public:
	bool Flush(bool hardFlush, int propagation=-1, bool blocking=true)
		{return this->ChannelFlush(DEFAULT_CHANNEL, hardFlush, propagation, blocking);}
	bool MessageSeriesEnd(int propagation=-1, bool blocking=true)
		{return this->ChannelMessageSeriesEnd(DEFAULT_CHANNEL, propagation, blocking);}
	byte * CreatePutSpace(size_t &size)
		{return this->ChannelCreatePutSpace(DEFAULT_CHANNEL, size);}
	size_t Put2(const byte *inString, size_t length, int messageEnd, bool blocking)
		{return this->ChannelPut2(DEFAULT_CHANNEL, inString, length, messageEnd, blocking);}
	size_t PutModifiable2(byte *inString, size_t length, int messageEnd, bool blocking)
		{return this->ChannelPutModifiable2(DEFAULT_CHANNEL, inString, length, messageEnd, blocking);}



	byte * ChannelCreatePutSpace(const std::string &channel, size_t &size)
		{CRYPTOPP_UNUSED(channel); size = 0; return NULL;}
	bool ChannelPutModifiable(const std::string &channel, byte *inString, size_t length)
		{this->ChannelPut(channel, inString, length); return false;}

	virtual size_t ChannelPut2(const std::string &channel, const byte *begin, size_t length, int messageEnd, bool blocking) =0;
	size_t ChannelPutModifiable2(const std::string &channel, byte *begin, size_t length, int messageEnd, bool blocking)
		{return ChannelPut2(channel, begin, length, messageEnd, blocking);}

	virtual bool ChannelFlush(const std::string &channel, bool hardFlush, int propagation=-1, bool blocking=true) =0;
};





template <class T>
class CRYPTOPP_NO_VTABLE AutoSignaling : public T
{
public:
	AutoSignaling(int propagation=-1) : m_autoSignalPropagation(propagation) {}

	void SetAutoSignalPropagation(int propagation)
		{m_autoSignalPropagation = propagation;}
	int GetAutoSignalPropagation() const
		{return m_autoSignalPropagation;}

private:
	int m_autoSignalPropagation;
};





class CRYPTOPP_DLL CRYPTOPP_NO_VTABLE Store : public AutoSignaling<InputRejecting<BufferedTransformation> >
{
public:
	
	Store() : m_messageEnd(false) {}

	void IsolatedInitialize(const NameValuePairs &parameters)
	{
		m_messageEnd = false;
		StoreInitialize(parameters);
	}

	unsigned int NumberOfMessages() const {return m_messageEnd ? 0 : 1;}
	bool GetNextMessage();
	unsigned int CopyMessagesTo(BufferedTransformation &target, unsigned int count=UINT_MAX, const std::string &channel=DEFAULT_CHANNEL) const;

protected:
	virtual void StoreInitialize(const NameValuePairs &parameters) =0;

	bool m_messageEnd;
};











class CRYPTOPP_DLL CRYPTOPP_NO_VTABLE Sink : public BufferedTransformation
{
public:
	size_t TransferTo2(BufferedTransformation &target, lword &transferBytes, const std::string &channel=DEFAULT_CHANNEL, bool blocking=true)
		{CRYPTOPP_UNUSED(target); CRYPTOPP_UNUSED(transferBytes); CRYPTOPP_UNUSED(channel); CRYPTOPP_UNUSED(blocking); transferBytes = 0; return 0;}
	size_t CopyRangeTo2(BufferedTransformation &target, lword &begin, lword end=LWORD_MAX, const std::string &channel=DEFAULT_CHANNEL, bool blocking=true) const
		{CRYPTOPP_UNUSED(target); CRYPTOPP_UNUSED(begin); CRYPTOPP_UNUSED(end); CRYPTOPP_UNUSED(channel); CRYPTOPP_UNUSED(blocking); return 0;}
};






class CRYPTOPP_DLL BitBucket : public Bufferless<Sink>
{
public:
	std::string AlgorithmName() const {return "BitBucket";}
	void IsolatedInitialize(const NameValuePairs &params)
		{CRYPTOPP_UNUSED(params);}
	size_t Put2(const byte *inString, size_t length, int messageEnd, bool blocking)
		{CRYPTOPP_UNUSED(inString); CRYPTOPP_UNUSED(length); CRYPTOPP_UNUSED(messageEnd); CRYPTOPP_UNUSED(blocking); return 0;}
};

NAMESPACE_END

#if CRYPTOPP_MSC_VERSION
# pragma warning(pop)
#endif

#endif
