



































































































#ifndef TINYFORMAT_H_INCLUDED
#define TINYFORMAT_H_INCLUDED

namespace tinyformat
{
}




namespace tfm = tinyformat;


#define TINYFORMAT_ERROR(reasonString) throw std::runtime_error(reasonString)








#include <cassert>
#include <iostream>
#include <sstream>
#include <stdexcept>

#ifndef TINYFORMAT_ERROR
#define TINYFORMAT_ERROR(reason) assert(0 && reason)
#endif

#if !defined(TINYFORMAT_USE_VARIADIC_TEMPLATES) && !defined(TINYFORMAT_NO_VARIADIC_TEMPLATES)
#ifdef __GXX_EXPERIMENTAL_CXX0X__
#define TINYFORMAT_USE_VARIADIC_TEMPLATES
#endif
#endif

#ifdef __GNUC__
#define TINYFORMAT_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define TINYFORMAT_NOINLINE __declspec(noinline)
#else
#define TINYFORMAT_NOINLINE
#endif

#if defined(__GLIBCXX__) && __GLIBCXX__ < 20080201


#define TINYFORMAT_OLD_LIBSTDCPLUSPLUS_WORKAROUND
#endif

namespace tinyformat
{

namespace detail
{

template <typename T1, typename T2>
struct is_convertible {
private:
    
    struct fail {
        char dummy[2];
    };
    struct succeed {
        char dummy;
    };
    
    static fail tryConvert(...);
    static succeed tryConvert(const T2&);
    static const T1& makeT1();

public:
#ifdef _MSC_VER

#pragma warning(push)
#pragma warning(disable : 4244)
#pragma warning(disable : 4267)
#endif
    
    
    
    
    static const bool value =
        sizeof(tryConvert(makeT1())) == sizeof(succeed);
#ifdef _MSC_VER
#pragma warning(pop)
#endif
};



template <typename T>
struct is_wchar {
    typedef int tinyformat_wchar_is_not_supported;
};
template <>
struct is_wchar<wchar_t*> {
};
template <>
struct is_wchar<const wchar_t*> {
};
template <int n>
struct is_wchar<const wchar_t[n]> {
};
template <int n>
struct is_wchar<wchar_t[n]> {
};




template <typename T, typename fmtT, bool convertible = is_convertible<T, fmtT>::value>
struct formatValueAsType {
    static void invoke(std::ostream& , const T& ) { assert(0); }
};


template <typename T, typename fmtT>
struct formatValueAsType<T, fmtT, true> {
    static void invoke(std::ostream& out, const T& value)
    {
        out << static_cast<fmtT>(value);
    }
};

#ifdef TINYFORMAT_OLD_LIBSTDCPLUSPLUS_WORKAROUND
template <typename T, bool convertible = is_convertible<T, int>::value>
struct formatZeroIntegerWorkaround {
    static bool invoke(std::ostream& ) { return false; }
};
template <typename T>
struct formatZeroIntegerWorkaround<T, true> {
    static bool invoke(std::ostream& out, const T& value)
    {
        if (static_cast<int>(value) == 0 && out.flags() & std::ios::showpos) {
            out << "+0";
            return true;
        }
        return false;
    }
};
#endif 



template <typename T, bool convertible = is_convertible<T, int>::value>
struct convertToInt {
    static int invoke(const T& )
    {
        TINYFORMAT_ERROR("tinyformat: Cannot convert from argument type to "
                         "integer for use as variable width or precision");
        return 0;
    }
};

template <typename T>
struct convertToInt<T, true> {
    static int invoke(const T& value) { return static_cast<int>(value); }
};

} 
















template <typename T>
inline void formatValue(std::ostream& out, const char* , const char* fmtEnd, const T& value)
{
#ifndef TINYFORMAT_ALLOW_WCHAR_STRINGS
    
    
    typedef typename detail::is_wchar<T>::tinyformat_wchar_is_not_supported DummyType;
    (void)DummyType(); 
#endif
    
    
    
    
    
    const bool canConvertToChar = detail::is_convertible<T, char>::value;
    const bool canConvertToVoidPtr = detail::is_convertible<T, const void*>::value;
    if (canConvertToChar && *(fmtEnd - 1) == 'c')
        detail::formatValueAsType<T, char>::invoke(out, value);
    else if (canConvertToVoidPtr && *(fmtEnd - 1) == 'p')
        detail::formatValueAsType<T, const void*>::invoke(out, value);
#ifdef TINYFORMAT_OLD_LIBSTDCPLUSPLUS_WORKAROUND
    else if (detail::formatZeroIntegerWorkaround<T>::invoke(out, value)) 
        ;
#endif
    else
        out << value;
}



#define TINYFORMAT_DEFINE_FORMATVALUE_CHAR(charType)                     \
    inline void formatValue(std::ostream& out, const char* , \
        const char* fmtEnd, charType value)                              \
    {                                                                    \
        switch (*(fmtEnd - 1)) {                                         \
        case 'u':                                                        \
        case 'd':                                                        \
        case 'i':                                                        \
        case 'o':                                                        \
        case 'X':                                                        \
        case 'x':                                                        \
            out << static_cast<int>(value);                              \
            break;                                                       \
        default:                                                         \
            out << value;                                                \
            break;                                                       \
        }                                                                \
    }

TINYFORMAT_DEFINE_FORMATVALUE_CHAR(char)
TINYFORMAT_DEFINE_FORMATVALUE_CHAR(signed char)
TINYFORMAT_DEFINE_FORMATVALUE_CHAR(unsigned char)
#undef TINYFORMAT_DEFINE_FORMATVALUE_CHAR







#define TINYFORMAT_ARGTYPES(n) TINYFORMAT_ARGTYPES_##n
#define TINYFORMAT_VARARGS(n) TINYFORMAT_VARARGS_##n
#define TINYFORMAT_PASSARGS(n) TINYFORMAT_PASSARGS_##n
#define TINYFORMAT_PASSARGS_TAIL(n) TINYFORMAT_PASSARGS_TAIL_##n










/*[[[cog
maxParams = 16

def makeCommaSepLists(lineTemplate, elemTemplate, startInd=1):
    for j in range(startInd,maxParams+1):
        list = ', '.join([elemTemplate % {'i':i} for i in range(startInd,j+1)])
        cog.outl(lineTemplate % {'j':j, 'list':list})

makeCommaSepLists('#define TINYFORMAT_ARGTYPES_%(j)d %(list)s',
                  'class T%(i)d')

cog.outl()
makeCommaSepLists('#define TINYFORMAT_VARARGS_%(j)d %(list)s',
                  'const T%(i)d& v%(i)d')

cog.outl()
makeCommaSepLists('#define TINYFORMAT_PASSARGS_%(j)d %(list)s', 'v%(i)d')

cog.outl()
cog.outl('#define TINYFORMAT_PASSARGS_TAIL_1')
makeCommaSepLists('#define TINYFORMAT_PASSARGS_TAIL_%(j)d , %(list)s',
                  'v%(i)d', startInd = 2)

cog.outl()
cog.outl('#define TINYFORMAT_FOREACH_ARGNUM(m) \\\n    ' +
         ' '.join(['m(%d)' % (j,) for j in range(1,maxParams+1)]))
]]]*/
#define TINYFORMAT_ARGTYPES_1 class T1
#define TINYFORMAT_ARGTYPES_2 class T1, class T2
#define TINYFORMAT_ARGTYPES_3 class T1, class T2, class T3
#define TINYFORMAT_ARGTYPES_4 class T1, class T2, class T3, class T4
#define TINYFORMAT_ARGTYPES_5 class T1, class T2, class T3, class T4, class T5
#define TINYFORMAT_ARGTYPES_6 class T1, class T2, class T3, class T4, class T5, class T6
#define TINYFORMAT_ARGTYPES_7 class T1, class T2, class T3, class T4, class T5, class T6, class T7
#define TINYFORMAT_ARGTYPES_8 class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8
#define TINYFORMAT_ARGTYPES_9 class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9
#define TINYFORMAT_ARGTYPES_10 class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10
#define TINYFORMAT_ARGTYPES_11 class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11
#define TINYFORMAT_ARGTYPES_12 class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11, class T12
#define TINYFORMAT_ARGTYPES_13 class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11, class T12, class T13
#define TINYFORMAT_ARGTYPES_14 class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11, class T12, class T13, class T14
#define TINYFORMAT_ARGTYPES_15 class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11, class T12, class T13, class T14, class T15
#define TINYFORMAT_ARGTYPES_16 class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11, class T12, class T13, class T14, class T15, class T16

#define TINYFORMAT_VARARGS_1 const T1& v1
#define TINYFORMAT_VARARGS_2 const T1 &v1, const T2 &v2
#define TINYFORMAT_VARARGS_3 const T1 &v1, const T2 &v2, const T3 &v3
#define TINYFORMAT_VARARGS_4 const T1 &v1, const T2 &v2, const T3 &v3, const T4 &v4
#define TINYFORMAT_VARARGS_5 const T1 &v1, const T2 &v2, const T3 &v3, const T4 &v4, const T5 &v5
#define TINYFORMAT_VARARGS_6 const T1 &v1, const T2 &v2, const T3 &v3, const T4 &v4, const T5 &v5, const T6 &v6
#define TINYFORMAT_VARARGS_7 const T1 &v1, const T2 &v2, const T3 &v3, const T4 &v4, const T5 &v5, const T6 &v6, const T7 &v7
#define TINYFORMAT_VARARGS_8 const T1 &v1, const T2 &v2, const T3 &v3, const T4 &v4, const T5 &v5, const T6 &v6, const T7 &v7, const T8 &v8
#define TINYFORMAT_VARARGS_9 const T1 &v1, const T2 &v2, const T3 &v3, const T4 &v4, const T5 &v5, const T6 &v6, const T7 &v7, const T8 &v8, const T9 &v9
#define TINYFORMAT_VARARGS_10 const T1 &v1, const T2 &v2, const T3 &v3, const T4 &v4, const T5 &v5, const T6 &v6, const T7 &v7, const T8 &v8, const T9 &v9, const T10 &v10
#define TINYFORMAT_VARARGS_11 const T1 &v1, const T2 &v2, const T3 &v3, const T4 &v4, const T5 &v5, const T6 &v6, const T7 &v7, const T8 &v8, const T9 &v9, const T10 &v10, const T11 &v11
#define TINYFORMAT_VARARGS_12 const T1 &v1, const T2 &v2, const T3 &v3, const T4 &v4, const T5 &v5, const T6 &v6, const T7 &v7, const T8 &v8, const T9 &v9, const T10 &v10, const T11 &v11, const T12 &v12
#define TINYFORMAT_VARARGS_13 const T1 &v1, const T2 &v2, const T3 &v3, const T4 &v4, const T5 &v5, const T6 &v6, const T7 &v7, const T8 &v8, const T9 &v9, const T10 &v10, const T11 &v11, const T12 &v12, const T13 &v13
#define TINYFORMAT_VARARGS_14 const T1 &v1, const T2 &v2, const T3 &v3, const T4 &v4, const T5 &v5, const T6 &v6, const T7 &v7, const T8 &v8, const T9 &v9, const T10 &v10, const T11 &v11, const T12 &v12, const T13 &v13, const T14 &v14
#define TINYFORMAT_VARARGS_15 const T1 &v1, const T2 &v2, const T3 &v3, const T4 &v4, const T5 &v5, const T6 &v6, const T7 &v7, const T8 &v8, const T9 &v9, const T10 &v10, const T11 &v11, const T12 &v12, const T13 &v13, const T14 &v14, const T15 &v15
#define TINYFORMAT_VARARGS_16 const T1 &v1, const T2 &v2, const T3 &v3, const T4 &v4, const T5 &v5, const T6 &v6, const T7 &v7, const T8 &v8, const T9 &v9, const T10 &v10, const T11 &v11, const T12 &v12, const T13 &v13, const T14 &v14, const T15 &v15, const T16 &v16

#define TINYFORMAT_PASSARGS_1 v1
#define TINYFORMAT_PASSARGS_2 v1, v2
#define TINYFORMAT_PASSARGS_3 v1, v2, v3
#define TINYFORMAT_PASSARGS_4 v1, v2, v3, v4
#define TINYFORMAT_PASSARGS_5 v1, v2, v3, v4, v5
#define TINYFORMAT_PASSARGS_6 v1, v2, v3, v4, v5, v6
#define TINYFORMAT_PASSARGS_7 v1, v2, v3, v4, v5, v6, v7
#define TINYFORMAT_PASSARGS_8 v1, v2, v3, v4, v5, v6, v7, v8
#define TINYFORMAT_PASSARGS_9 v1, v2, v3, v4, v5, v6, v7, v8, v9
#define TINYFORMAT_PASSARGS_10 v1, v2, v3, v4, v5, v6, v7, v8, v9, v10
#define TINYFORMAT_PASSARGS_11 v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11
#define TINYFORMAT_PASSARGS_12 v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12
#define TINYFORMAT_PASSARGS_13 v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13
#define TINYFORMAT_PASSARGS_14 v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14
#define TINYFORMAT_PASSARGS_15 v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15
#define TINYFORMAT_PASSARGS_16 v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16

#define TINYFORMAT_PASSARGS_TAIL_1
#define TINYFORMAT_PASSARGS_TAIL_2 , v2
#define TINYFORMAT_PASSARGS_TAIL_3 , v2, v3
#define TINYFORMAT_PASSARGS_TAIL_4 , v2, v3, v4
#define TINYFORMAT_PASSARGS_TAIL_5 , v2, v3, v4, v5
#define TINYFORMAT_PASSARGS_TAIL_6 , v2, v3, v4, v5, v6
#define TINYFORMAT_PASSARGS_TAIL_7 , v2, v3, v4, v5, v6, v7
#define TINYFORMAT_PASSARGS_TAIL_8 , v2, v3, v4, v5, v6, v7, v8
#define TINYFORMAT_PASSARGS_TAIL_9 , v2, v3, v4, v5, v6, v7, v8, v9
#define TINYFORMAT_PASSARGS_TAIL_10 , v2, v3, v4, v5, v6, v7, v8, v9, v10
#define TINYFORMAT_PASSARGS_TAIL_11 , v2, v3, v4, v5, v6, v7, v8, v9, v10, v11
#define TINYFORMAT_PASSARGS_TAIL_12 , v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12
#define TINYFORMAT_PASSARGS_TAIL_13 , v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13
#define TINYFORMAT_PASSARGS_TAIL_14 , v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14
#define TINYFORMAT_PASSARGS_TAIL_15 , v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15
#define TINYFORMAT_PASSARGS_TAIL_16 , v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16

#define TINYFORMAT_FOREACH_ARGNUM(m) \
    m(1) m(2) m(3) m(4) m(5) m(6) m(7) m(8) m(9) m(10) m(11) m(12) m(13) m(14) m(15) m(16)



namespace detail
{


class FormatIterator
{
public:
    
    enum ExtraFormatFlags {
        Flag_None = 0,
        Flag_TruncateToPrecision = 1 << 0, 
        Flag_SpacePadPositive = 1 << 1,    
        Flag_VariableWidth = 1 << 2,       
        Flag_VariablePrecision = 1 << 3    
    };

    
    FormatIterator(std::ostream& out, const char* fmt)
        : m_out(out),
          m_fmt(fmt),
          m_extraFlags(Flag_None),
          m_wantWidth(false),
          m_wantPrecision(false),
          m_variableWidth(0),
          m_variablePrecision(0),
          m_origWidth(out.width()),
          m_origPrecision(out.precision()),
          m_origFlags(out.flags()),
          m_origFill(out.fill())
    {
    }

    
    void finish()
    {
        
        
        m_fmt = printFormatStringLiteral(m_out, m_fmt);
        if (*m_fmt != '\0')
            TINYFORMAT_ERROR("tinyformat: Too many conversion specifiers in format string");
    }

    ~FormatIterator()
    {
        
        m_out.width(m_origWidth);
        m_out.precision(m_origPrecision);
        m_out.flags(m_origFlags);
        m_out.fill(m_origFill);
    }

    template <typename T>
    void accept(const T& value);

private:
    
    
    static int parseIntAndAdvance(const char*& c)
    {
        int i = 0;
        for (; *c >= '0' && *c <= '9'; ++c)
            i = 10 * i + (*c - '0');
        return i;
    }

    
    
    
    template <typename T>
    static bool formatCStringTruncate(std::ostream& , const T& , std::streamsize )
    {
        return false;
    }
#define TINYFORMAT_DEFINE_FORMAT_C_STRING_TRUNCATE(type)              \
    static bool formatCStringTruncate(std::ostream& out, type* value, \
        std::streamsize truncLen)                                     \
    {                                                                 \
        std::streamsize len = 0;                                      \
        while (len < truncLen && value[len] != 0)                     \
            ++len;                                                    \
        out.write(value, len);                                        \
        return true;                                                  \
    }
    
    
    
    TINYFORMAT_DEFINE_FORMAT_C_STRING_TRUNCATE(const char)
    TINYFORMAT_DEFINE_FORMAT_C_STRING_TRUNCATE(char)
#undef TINYFORMAT_DEFINE_FORMAT_C_STRING_TRUNCATE

    
    
    
    
    
    
    static const char* printFormatStringLiteral(std::ostream& out,
        const char* fmt)
    {
        const char* c = fmt;
        for (; true; ++c) {
            switch (*c) {
            case '\0':
                out.write(fmt, static_cast<std::streamsize>(c - fmt));
                return c;
            case '%':
                out.write(fmt, static_cast<std::streamsize>(c - fmt));
                if (*(c + 1) != '%')
                    return c;
                
                fmt = ++c;
                break;
            }
        }
    }

    static const char* streamStateFromFormat(std::ostream& out,
        unsigned int& extraFlags,
        const char* fmtStart,
        int variableWidth,
        int variablePrecision);

    
    FormatIterator(const FormatIterator&);
    FormatIterator& operator=(const FormatIterator&);

    
    std::ostream& m_out;
    const char* m_fmt;
    unsigned int m_extraFlags;
    
    bool m_wantWidth;
    bool m_wantPrecision;
    int m_variableWidth;
    int m_variablePrecision;
    
    std::streamsize m_origWidth;
    std::streamsize m_origPrecision;
    std::ios::fmtflags m_origFlags;
    char m_origFill;
};



template <typename T>
TINYFORMAT_NOINLINE 
    void
    FormatIterator::accept(const T& value)
{
    
    const char* fmtEnd = 0;
    if (m_extraFlags == Flag_None && !m_wantWidth && !m_wantPrecision) {
        m_fmt = printFormatStringLiteral(m_out, m_fmt);
        fmtEnd = streamStateFromFormat(m_out, m_extraFlags, m_fmt, 0, 0);
        m_wantWidth = (m_extraFlags & Flag_VariableWidth) != 0;
        m_wantPrecision = (m_extraFlags & Flag_VariablePrecision) != 0;
    }
    
    if (m_extraFlags & (Flag_VariableWidth | Flag_VariablePrecision)) {
        if (m_wantWidth || m_wantPrecision) {
            int v = convertToInt<T>::invoke(value);
            if (m_wantWidth) {
                m_variableWidth = v;
                m_wantWidth = false;
            } else if (m_wantPrecision) {
                m_variablePrecision = v;
                m_wantPrecision = false;
            }
            return;
        }
        
        
        fmtEnd = streamStateFromFormat(m_out, m_extraFlags, m_fmt,
            m_variableWidth, m_variablePrecision);
    }

    
    if (!(m_extraFlags & (Flag_SpacePadPositive | Flag_TruncateToPrecision)))
        formatValue(m_out, m_fmt, fmtEnd, value);
    else {
        
        
        
        
        std::ostringstream tmpStream;
        tmpStream.copyfmt(m_out);
        if (m_extraFlags & Flag_SpacePadPositive)
            tmpStream.setf(std::ios::showpos);
        
        
        
        if (!((m_extraFlags & Flag_TruncateToPrecision) &&
                formatCStringTruncate(tmpStream, value, m_out.precision()))) {
            
            formatValue(tmpStream, m_fmt, fmtEnd, value);
        }
        std::string result = tmpStream.str(); 
        if (m_extraFlags & Flag_SpacePadPositive) {
            for (size_t i = 0, iend = result.size(); i < iend; ++i)
                if (result[i] == '+')
                    result[i] = ' ';
        }
        if ((m_extraFlags & Flag_TruncateToPrecision) &&
            (int)result.size() > (int)m_out.precision())
            m_out.write(result.c_str(), m_out.precision());
        else
            m_out << result;
    }
    m_extraFlags = Flag_None;
    m_fmt = fmtEnd;
}










inline const char* FormatIterator::streamStateFromFormat(std::ostream& out,
    unsigned int& extraFlags,
    const char* fmtStart,
    int variableWidth,
    int variablePrecision)
{
    if (*fmtStart != '%') {
        TINYFORMAT_ERROR("tinyformat: Not enough conversion specifiers in format string");
        return fmtStart;
    }
    
    out.width(0);
    out.precision(6);
    out.fill(' ');
    
    out.unsetf(std::ios::adjustfield | std::ios::basefield |
               std::ios::floatfield | std::ios::showbase | std::ios::boolalpha |
               std::ios::showpoint | std::ios::showpos | std::ios::uppercase);
    extraFlags = Flag_None;
    bool precisionSet = false;
    bool widthSet = false;
    const char* c = fmtStart + 1;
    
    for (;; ++c) {
        switch (*c) {
        case '#':
            out.setf(std::ios::showpoint | std::ios::showbase);
            continue;
        case '0':
            
            if (!(out.flags() & std::ios::left)) {
                
                
                out.fill('0');
                out.setf(std::ios::internal, std::ios::adjustfield);
            }
            continue;
        case '-':
            out.fill(' ');
            out.setf(std::ios::left, std::ios::adjustfield);
            continue;
        case ' ':
            
            if (!(out.flags() & std::ios::showpos))
                extraFlags |= Flag_SpacePadPositive;
            continue;
        case '+':
            out.setf(std::ios::showpos);
            extraFlags &= ~Flag_SpacePadPositive;
            continue;
        }
        break;
    }
    
    if (*c >= '0' && *c <= '9') {
        widthSet = true;
        out.width(parseIntAndAdvance(c));
    }
    if (*c == '*') {
        widthSet = true;
        if (variableWidth < 0) {
            
            out.fill(' ');
            out.setf(std::ios::left, std::ios::adjustfield);
            variableWidth = -variableWidth;
        }
        out.width(variableWidth);
        extraFlags |= Flag_VariableWidth;
        ++c;
    }
    
    if (*c == '.') {
        ++c;
        int precision = 0;
        if (*c == '*') {
            ++c;
            extraFlags |= Flag_VariablePrecision;
            precision = variablePrecision;
        } else {
            if (*c >= '0' && *c <= '9')
                precision = parseIntAndAdvance(c);
            else if (*c == '-') 
                parseIntAndAdvance(++c);
        }
        out.precision(precision);
        precisionSet = true;
    }
    
    while (*c == 'l' || *c == 'h' || *c == 'L' ||
           *c == 'j' || *c == 'z' || *c == 't')
        ++c;
    
    
    
    bool intConversion = false;
    switch (*c) {
    case 'u':
    case 'd':
    case 'i':
        out.setf(std::ios::dec, std::ios::basefield);
        intConversion = true;
        break;
    case 'o':
        out.setf(std::ios::oct, std::ios::basefield);
        intConversion = true;
        break;
    case 'X':
        out.setf(std::ios::uppercase);
    case 'x':
    case 'p':
        out.setf(std::ios::hex, std::ios::basefield);
        intConversion = true;
        break;
    case 'E':
        out.setf(std::ios::uppercase);
    case 'e':
        out.setf(std::ios::scientific, std::ios::floatfield);
        out.setf(std::ios::dec, std::ios::basefield);
        break;
    case 'F':
        out.setf(std::ios::uppercase);
    case 'f':
        out.setf(std::ios::fixed, std::ios::floatfield);
        break;
    case 'G':
        out.setf(std::ios::uppercase);
    case 'g':
        out.setf(std::ios::dec, std::ios::basefield);
        
        out.flags(out.flags() & ~std::ios::floatfield);
        break;
    case 'a':
    case 'A':
        TINYFORMAT_ERROR("tinyformat: the %a and %A conversion specs "
                         "are not supported");
        break;
    case 'c':
        
        break;
    case 's':
        if (precisionSet)
            extraFlags |= Flag_TruncateToPrecision;
        
        out.setf(std::ios::boolalpha);
        break;
    case 'n':
        
        TINYFORMAT_ERROR("tinyformat: %n conversion spec not supported");
        break;
    case '\0':
        TINYFORMAT_ERROR("tinyformat: Conversion spec incorrectly "
                         "terminated by end of string");
        return c;
    }
    if (intConversion && precisionSet && !widthSet) {
        
        
        
        
        out.width(out.precision());
        out.setf(std::ios::internal, std::ios::adjustfield);
        out.fill('0');
    }
    return c + 1;
}








#ifdef TINYFORMAT_USE_VARIADIC_TEMPLATES

template <typename T1>
void format(FormatIterator& fmtIter, const T1& value1)
{
    fmtIter.accept(value1);
    fmtIter.finish();
}


template <typename T1, typename... Args>
void format(FormatIterator& fmtIter, const T1& value1, const Args&... args)
{
    fmtIter.accept(value1);
    format(fmtIter, args...);
}

#else

inline void format(FormatIterator& fmtIter)
{
    fmtIter.finish();
}


#define TINYFORMAT_MAKE_FORMAT_DETAIL(n)                                \
    template <TINYFORMAT_ARGTYPES(n)>                                   \
    void format(detail::FormatIterator& fmtIter, TINYFORMAT_VARARGS(n)) \
    {                                                                   \
        fmtIter.accept(v1);                                             \
        format(fmtIter TINYFORMAT_PASSARGS_TAIL(n));                    \
    }

TINYFORMAT_FOREACH_ARGNUM(TINYFORMAT_MAKE_FORMAT_DETAIL)
#undef TINYFORMAT_MAKE_FORMAT_DETAIL

#endif 

} 





#ifdef TINYFORMAT_USE_VARIADIC_TEMPLATES


template <typename T1, typename... Args>
void format(std::ostream& out, const char* fmt, const T1& v1, const Args&... args)
{
    detail::FormatIterator fmtIter(out, fmt);
    format(fmtIter, v1, args...);
}

template <typename T1, typename... Args>
std::string format(const char* fmt, const T1& v1, const Args&... args)
{
    std::ostringstream oss;
    format(oss, fmt, v1, args...);
    return oss.str();
}

template <typename T1, typename... Args>
std::string format(const std::string& fmt, const T1& v1, const Args&... args)
{
    std::ostringstream oss;
    format(oss, fmt.c_str(), v1, args...);
    return oss.str();
}

template <typename T1, typename... Args>
void printf(const char* fmt, const T1& v1, const Args&... args)
{
    format(std::cout, fmt, v1, args...);
}

#else


#define TINYFORMAT_MAKE_FORMAT_FUNCS(n)                                    \
                                                                           \
    template <TINYFORMAT_ARGTYPES(n)>                                      \
    void format(std::ostream& out, const char* fmt, TINYFORMAT_VARARGS(n)) \
    {                                                                      \
        tinyformat::detail::FormatIterator fmtIter(out, fmt);              \
        tinyformat::detail::format(fmtIter, TINYFORMAT_PASSARGS(n));       \
    }                                                                      \
                                                                           \
    template <TINYFORMAT_ARGTYPES(n)>                                      \
    std::string format(const char* fmt, TINYFORMAT_VARARGS(n))             \
    {                                                                      \
        std::ostringstream oss;                                            \
        tinyformat::format(oss, fmt, TINYFORMAT_PASSARGS(n));              \
        return oss.str();                                                  \
    }                                                                      \
                                                                           \
    template <TINYFORMAT_ARGTYPES(n)>                                      \
    std::string format(const std::string& fmt, TINYFORMAT_VARARGS(n))      \
    {                                                                      \
        std::ostringstream oss;                                            \
        tinyformat::format(oss, fmt.c_str(), TINYFORMAT_PASSARGS(n));      \
        return oss.str();                                                  \
    }                                                                      \
                                                                           \
    template <TINYFORMAT_ARGTYPES(n)>                                      \
    void printf(const char* fmt, TINYFORMAT_VARARGS(n))                    \
    {                                                                      \
        tinyformat::format(std::cout, fmt, TINYFORMAT_PASSARGS(n));        \
    }

TINYFORMAT_FOREACH_ARGNUM(TINYFORMAT_MAKE_FORMAT_FUNCS)
#undef TINYFORMAT_MAKE_FORMAT_FUNCS
#endif





#define TINYFORMAT_WRAP_FORMAT_EXTRA_ARGS
#define TINYFORMAT_WRAP_FORMAT_N(n, returnType, funcName, funcDeclSuffix,  \
                                 bodyPrefix, streamName, bodySuffix)       \
    template <TINYFORMAT_ARGTYPES(n)>                                      \
    returnType funcName(TINYFORMAT_WRAP_FORMAT_EXTRA_ARGS const char* fmt, \
        TINYFORMAT_VARARGS(n)) funcDeclSuffix                              \
    {                                                                      \
        bodyPrefix                                                         \
            tinyformat::format(streamName, fmt, TINYFORMAT_PASSARGS(n));   \
        bodySuffix                                                         \
    }

#define TINYFORMAT_WRAP_FORMAT(returnType, funcName, funcDeclSuffix,                                       \
                               bodyPrefix, streamName, bodySuffix)                                         \
    inline returnType funcName(TINYFORMAT_WRAP_FORMAT_EXTRA_ARGS const char* fmt) funcDeclSuffix           \
    {                                                                                                      \
        bodyPrefix                                                                                         \
            tinyformat::detail::FormatIterator(streamName, fmt)                                            \
                .finish();                                                                                 \
        bodySuffix                                                                                         \
    }                                                                                                      \
    TINYFORMAT_WRAP_FORMAT_N(1, returnType, funcName, funcDeclSuffix, bodyPrefix, streamName, bodySuffix)  \
    TINYFORMAT_WRAP_FORMAT_N(2, returnType, funcName, funcDeclSuffix, bodyPrefix, streamName, bodySuffix)  \
    TINYFORMAT_WRAP_FORMAT_N(3, returnType, funcName, funcDeclSuffix, bodyPrefix, streamName, bodySuffix)  \
    TINYFORMAT_WRAP_FORMAT_N(4, returnType, funcName, funcDeclSuffix, bodyPrefix, streamName, bodySuffix)  \
    TINYFORMAT_WRAP_FORMAT_N(5, returnType, funcName, funcDeclSuffix, bodyPrefix, streamName, bodySuffix)  \
    TINYFORMAT_WRAP_FORMAT_N(6, returnType, funcName, funcDeclSuffix, bodyPrefix, streamName, bodySuffix)  \
    TINYFORMAT_WRAP_FORMAT_N(7, returnType, funcName, funcDeclSuffix, bodyPrefix, streamName, bodySuffix)  \
    TINYFORMAT_WRAP_FORMAT_N(8, returnType, funcName, funcDeclSuffix, bodyPrefix, streamName, bodySuffix)  \
    TINYFORMAT_WRAP_FORMAT_N(9, returnType, funcName, funcDeclSuffix, bodyPrefix, streamName, bodySuffix)  \
    TINYFORMAT_WRAP_FORMAT_N(10, returnType, funcName, funcDeclSuffix, bodyPrefix, streamName, bodySuffix) \
    TINYFORMAT_WRAP_FORMAT_N(11, returnType, funcName, funcDeclSuffix, bodyPrefix, streamName, bodySuffix) \
    TINYFORMAT_WRAP_FORMAT_N(12, returnType, funcName, funcDeclSuffix, bodyPrefix, streamName, bodySuffix) \
    TINYFORMAT_WRAP_FORMAT_N(13, returnType, funcName, funcDeclSuffix, bodyPrefix, streamName, bodySuffix) \
    TINYFORMAT_WRAP_FORMAT_N(14, returnType, funcName, funcDeclSuffix, bodyPrefix, streamName, bodySuffix) \
    TINYFORMAT_WRAP_FORMAT_N(15, returnType, funcName, funcDeclSuffix, bodyPrefix, streamName, bodySuffix) \
    TINYFORMAT_WRAP_FORMAT_N(16, returnType, funcName, funcDeclSuffix, bodyPrefix, streamName, bodySuffix)


} 

#define strprintf tfm::format

#endif 
