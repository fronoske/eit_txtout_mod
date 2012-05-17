#ifndef _INC_TS_DESC_H_
#define _INC_TS_DESC_H_

#include "aribstr.h"

#define DSCTAG_SHORTEVT		0x4D
#define DSCTAG_EXTEVT		0x4E
#define DSCTAG_COMPONENT	0x50
#define DSCTAG_CONTENT		0x54
#define DSCTAG_SERVICE		0x48

// SIのBCD時刻をtm構造体に変換
static void _convert_time_bcd(TIME_BCD time, struct tm &rt)
{
	rt.tm_hour	= ((time>>20)&0xF) * 10 + ((time>>16)&0xF);
	rt.tm_min	= ((time>>12)&0xF) * 10 + ((time>> 8)&0xF);
	rt.tm_sec	= ((time>> 4)&0xF) * 10 + ((time>> 0)&0xF);
}

// SIの日付時刻形式をtm構造体に変換
static void _convert_time_jst40(TIME_JST40 time, tm &t)
{
	int tnum = (time >> 24) & 0xFFFF;		// 上位16ビット取り出し
  
	t.tm_year = static_cast<int>((tnum - 15078.2) / 365.25);
	t.tm_mon = static_cast<int>(((tnum - 14956.1) - (int)(t.tm_year * 365.25)) / 30.6001);
	t.tm_mday = (tnum - 14956) - (int)(t.tm_year * 365.25) - (int)(t.tm_mon * 30.6001);
	if ( t.tm_mon == 14 || t.tm_mon == 15 )
	{
		t.tm_year++;
		t.tm_mon -= 1 + 12;
	}
	else
		t.tm_mon--;

	_convert_time_bcd( static_cast<TIME_BCD>(time), t );

	// tmのmonthは0～11の間でないと駄目
	t.tm_mon--;
}

// SIの日付時刻形式をtime_tに変換
static time_t _convert_time_jst40(TIME_JST40 time)
{
	tm tm_b;

	_convert_time_jst40( time, tm_b );
	return mktime( &tm_b );
}

// 記述子クラス
class CDescriptor
{
public:
	int			tag;						// 記述子タグ
	VECT_UINT8	buf;						// データ
public:
	// Dataをセットする。マイナスを返すとエラー
	// プラスの場合、この記述子本体のサイズを返す
	int			SetData(VECT_UINT8::const_iterator beg, VECT_UINT8::const_iterator end)
	{	
		VECT_UINT8::const_iterator data = beg;
		if ( end - beg < 2 )
			return -1;
		tag	= beg[0];
		int len = beg[1];			
		if ( beg+2+len > end )
			return -1;
		buf.clear();
		std::copy( beg+2, beg+2+len, std::back_inserter( buf ) );
		return len;					// lengthを返す
	}
};

// 派生記述子クラス
// 短形式イベント記述子
class CShortEventDesc : public CDescriptor
{
public:
	tstring		name_str;
	tstring		desc_str;
public:
	CShortEventDesc();
	CShortEventDesc(const CDescriptor &desc)
	{
		const UINT8	*pp = &desc.buf[0];	
		const UINT8	*ep = pp + desc.buf.size();
		int buf_len = desc.buf.size();
		int src_len;
		tag = desc.tag;
		if ( desc.tag != DSCTAG_SHORTEVT )
			throw CDefExcept(_T("<CShortEventDesc>記述子タグが%Xでなく%Xです"), DSCTAG_SHORTEVT, desc.tag );
		if ( buf_len < 4 )
			throw CDefExcept(_T("<CShortEventDesc>記述子バイト数が少ない(%d)"), buf_len );
		pp += 3;			// 最初の3バイトは無視("jpn"と入ってるはず)
		src_len = *pp++;
		if ( pp + src_len >= ep )		// 次のdesc_strのlenまでチェック
			throw CDefExcept(_T("<CShortEventDesc>記述子バイト数が少ない(%d)"), buf_len );
		AribToString( &name_str, pp, src_len );
		
		pp += src_len;
		src_len = *pp++;
		if ( pp + src_len > ep )
			throw CDefExcept(_T("<CShortEventDesc>記述子バイト数が少ない(%d)"), buf_len );
		AribToString( &desc_str, pp, src_len );
	}
};

// コンテント記述子
class CContentDesc : public CDescriptor
{
public:
	int		content_nibble_level_1;
	int		content_nibble_level_2;
	int		user_nibble_1;
	int		user_nibble_2;
public:
	CContentDesc(const CDescriptor &desc)
	{
		tag = desc.tag;
		if ( desc.tag != DSCTAG_CONTENT )
			throw CDefExcept(_T("<CContentDesc>記述子タグが0x%Xでなく0x%Xです"), DSCTAG_CONTENT, desc.tag );
		if ( desc.buf.size() < 2 )
			throw CDefExcept(_T("<CContentDesc>記述子バイト数が%dより少ない(%d)"), 2, desc.buf.size() );
		const UINT8	*sp = &desc.buf[0];	
		int size = desc.buf.size();
		while ( TRUE ) {
			content_nibble_level_1 = (sp[0] >> 4) & 0xF;
			content_nibble_level_2 = (sp[0] >> 0) & 0xF;
			user_nibble_1 = (sp[1] >> 4) & 0xF;
			user_nibble_2 = (sp[1] >> 0) & 0xF;
			sp += 2 ;
			size -= 2 ;
			// ジャンル=拡張の場合は次を見る
			if ( size >= 2 && content_nibble_level_1 == 0xe ) continue ;
			break ;
		}
	}
};

// コンポーネント記述子
class CComponentDesc : public CDescriptor
{
public:
	int		stream_content;
	int		component_type;
	int		component_tag;
	tstring	text;
public:
	CComponentDesc(const CDescriptor &desc)
	{
		const UINT8	*sp = &desc.buf[0];	
		int buf_len = desc.buf.size();
		tag = desc.tag;
		if ( desc.tag != DSCTAG_COMPONENT )
			throw CDefExcept(_T("<CComponentDesc>記述子タグが0x%Xでなく0x%Xです"), DSCTAG_COMPONENT, desc.tag );
		if ( buf_len < 6 )
			throw CDefExcept(_T("<CComponentDesc>記述子バイト数が少ない(%d)"), buf_len );
		stream_content = sp[0] & 0xF;
		component_type = sp[1];
		component_tag = sp[2];
		sp += 6;
		AribToString( &text, sp, buf_len - 6 );
	}
};

class CServiceDesc : public CDescriptor
{
public:
	int		service_type;
	tstring	service_provider_name;
	tstring	service_name;
public:
	CServiceDesc(const CDescriptor &desc)
	{
		const UINT8 *sp = &desc.buf[0];
		const UINT8 *ep = sp + desc.buf.size();		// 終了チェック用
		int buf_len = desc.buf.size();
		tag = desc.tag;
		if ( desc.tag != DSCTAG_SERVICE )
			throw CDefExcept(_T("<CServiceDesc>記述子タグが0x%Xでなく0x%Xです"), DSCTAG_SERVICE, desc.tag );
		int str_len;
		if ( buf_len < 2 )
			throw CDefExcept(_T("<CServiceDesc>記述子バイト数が少ない(%d)"), buf_len );
		service_type = sp[0];
		str_len = sp[1];
		sp += 2;
		if ( sp + str_len >= ep )					// service_name_lenの所までチェック
			throw CDefExcept(_T("<CServiceDesc>記述子バイト数が少ない(%d)"), buf_len );
		AribToString( &service_provider_name, sp, str_len );
		sp += str_len;
		str_len = *sp++;
		if ( sp + str_len > ep )
			throw CDefExcept(_T("<CServiceDesc>記述子バイト数が少ない(%d)"), buf_len );
		AribToString( &service_name, sp, str_len );
	}
};

// 拡張形式イベント記述子
class CExtEventDesc : public CDescriptor
{
public:
	class CItem
	{
	public:
		tstring		description;	// 項目名
		VECT_UINT8	text;			// 説明。別の拡張形式イベント記述子とつながってる事があるので、バイト配列(後でつなげる)
	};
	typedef std::vector<CItem>	VECT_ITEM;
public:
	int			descriptor_number;
	int			last_descriptor_number;
	VECT_UINT8	text;				// 項目なし説明。別の拡張形式イベント記述子とつながってる事があるので、バイト配列(後でつなげる)
	VECT_ITEM	items;
public:
	CExtEventDesc(const CDescriptor &desc)
	{
//		VECT_UINT8::const_iterator sp = desc.buf.begin();
		VECT_UINT8::const_iterator pp = desc.buf.begin();
		VECT_UINT8::const_iterator ep = desc.buf.end();

		int buf_len = desc.buf.size();
		int len_of_items;
		int str_len;
		CItem item;

		tag = desc.tag;
		if ( desc.tag != DSCTAG_EXTEVT )
			throw CDefExcept(_T("<CExtEventDesc>記述子タグが0x%Xでなく0x%Xです"), DSCTAG_EXTEVT, desc.tag );
		if ( buf_len < 5 )		// length_of_itemsまでのチェック
			throw CDefExcept(_T("<CExtEventDesc>記述子バイト数が少ない(%d)"), buf_len );
		descriptor_number = (pp[0] >> 4) & 0xF;
		last_descriptor_number = (pp[0] >> 0) & 0xF;
		pp += 4;				// +3はlanguage_code("jpn")
		len_of_items = *pp++;
		while ( len_of_items > 0 )
		{
			str_len = *pp++;
			if ( pp + str_len >= ep )				// 次のlenまで
				throw CDefExcept(_T("<CExtEventDesc>記述子バイト数が少ない(%d)"), buf_len );
			AribToString( &item.description, &pp[0], str_len );
			len_of_items -= str_len + 1;
			pp += str_len;

			str_len = *pp++;
			if ( pp + str_len > ep )
				throw CDefExcept(_T("<CExtEventDesc>記述子バイト数が少ない(%d)"), buf_len );
			std::copy( pp, pp+str_len, std::back_inserter(item.text) );
			len_of_items -= str_len + 1;
			pp += str_len;

			items.push_back( item );
		}
		if ( len_of_items )
			DBG_LOG_ASSERT( _T("\rlen_of_itemsが0ではない:%d\n"), len_of_items );
		str_len = *pp++;
		if ( pp + str_len > ep )
			throw CDefExcept(_T("<CExtEventDesc>記述子バイト数が少ない(%d)"), buf_len );
		std::copy( pp, pp+str_len, std::back_inserter(text) );
	}
	
};

typedef std::vector<CDescriptor>	VECT_DESC;

#endif