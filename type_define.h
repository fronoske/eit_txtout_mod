#ifndef _INC_TYPE_DEFINE_H_
#define _INC_TYPE_DEFINE_H_

typedef unsigned __int8		UINT8;
typedef signed __int8		INT8;
typedef unsigned __int16	UINT16;
typedef signed __int16		INT16;
typedef unsigned __int32	UINT32;
typedef signed __int32		INT32;
typedef unsigned __int64	UINT64;
typedef __int64				INT64;

typedef std::vector<UINT8>	VECT_UINT8;
typedef std::vector<INT16>	VECT_INT16;
typedef std::vector<UINT16>	VECT_UINT16;

typedef __int64				TIME_JST40;			// 計４０ビット 16bit:MJD(日付) 24bit:BCD(時刻)
typedef __int32				TIME_BCD;			// 計２４ビット 24bit:BCD(時刻)

// STLのTCHAR対応
typedef std::basic_fstream<TCHAR, std::char_traits<TCHAR> >		tfstream;
typedef std::basic_ofstream<TCHAR, std::char_traits<TCHAR> >	tofstream;
typedef std::basic_ostream<TCHAR, std::char_traits<TCHAR> >		tostream;
typedef std::basic_string<TCHAR, std::char_traits<TCHAR>, std::allocator<TCHAR> > 
																tstring;
typedef std::basic_stringstream<TCHAR>							tstrstream;

#ifdef _UNICODE
#define tcout				std::wcout
#define tcerr				std::wcerr
#define tclog				std::wclog
#else
#define tcout				std::cout
#define tcerr				std::cerr
#define tclog				std::clog
#endif

//#define _vsntprintf		vsnprints
//#define _ttoi				atoi
//#define _tcstol			strtol 
//#define _tcstoul			strtoul
//#define _tcslen			strlen
//#define _tcsftime			strftime


class CDefExcept
{
public:
	tstring		str_msg;
public:
	CDefExcept(const TCHAR *lpsz_msg,...)
	{
		TCHAR	sz_msg[4096];
		va_list	va_arg;
		va_start(va_arg, lpsz_msg);
		_vsntprintf( sz_msg, 4096, lpsz_msg, va_arg );		// vsnprintf
		va_end(va_arg);
		
		str_msg = sz_msg;
	}
};

// デバッグ用
extern void dbg_log_out(int rpt_type, TCHAR *filename, int line, TCHAR *lpsz_msg,...); 
#define	DBG_LOG_WARN(msg,...)	dbg_log_out(_CRT_WARN, _T(__FILE__), __LINE__,  msg, __VA_ARGS__)
#define DBG_LOG_ERR(msg,...)	dbg_log_out(_CRT_ERR, _T(__FILE__), __LINE__,  msg, __VA_ARGS__)
#define DBG_LOG_ASSERT(msg,...)	(void)( dbg_log_out(_CRT_ASSERT, _T(__FILE__), __LINE__,  msg, __VA_ARGS__), _ASSERT(FALSE))

#ifdef _DEBUG
#define _VERIFY(f)		_ASSERT(f)
#else
#define _VERIFY(f)		f
#endif

#endif