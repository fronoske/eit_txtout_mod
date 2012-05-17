#ifndef _INC_TS_SIDEF_H_
#define _INC_TS_SIDEF_H_

#include "type_define.h"
#include "ts_desc.h"

// テーブルID。とりあえず使うやつだけ。
#define TBLID_PAT			0x00			// PATのテーブルID
#define TBLID_PMT			0x02			// PMTのテーブルID
#define TBLID_EIT_EVMY		0x4E			// 自ストリームの現在と次の番組
#define TBLID_EIT_EVOT		0x4F			// 他ストリームの現在と次の番組
#define TBLID_EIT_SCMY_B	0x50			// 自ストリームのスケジュール(先頭)
#define TBLID_EIT_SCMY_E	0x5F			// 自ストリームのスケジュール(末尾)
#define TBLID_EIT_SCOT_B	0x60			// 他ストリームのスケジュール(先頭)
#define TBLID_EIT_SCOT_E	0x6E			// 他ストリームのスケジュール(末尾)
#define TBLID_TDT			0x70
#define TBLID_TOT			0x73			
#define TBLID_SDT_MY		0x42			// 自ストリームのSDT
#define TBLID_SDT_OT		0x46			// 他ストリームのSDT

// 番組情報配列基本型
class CServiceInfo
{
public:
	int			table_id;
	int			section_syntax;
	int			length;
	int			id;							// SIによって意味が異なる
	int			version;
	int			current_next;				// 1:current 0:next
	int			section;
	int			last_section;
protected:
	static const int	c_ext_offset		= 9;	// 各番組情報の拡張メンバへのオフセットバイト数1
	static const int	c_ext_short_offset	= 4;	// 各番組情報の拡張メンバへのオフセットバイト数2
	std::set<UINT8>		m_proc_sections;	// 処理済みセクション番号
	UINT32 calc_crc(const UINT8 *start, size_t len)
	{	// TsRenameのCTsData::Calc_crc32()をコピペ
		UINT32 crc=0xffffffff;
		size_t i;

		static const UINT32 table[256] = {
			0x00000000, 0x04C11DB7, 0x09823B6E, 0x0D4326D9,
			0x130476DC, 0x17C56B6B, 0x1A864DB2, 0x1E475005,
			0x2608EDB8, 0x22C9F00F, 0x2F8AD6D6, 0x2B4BCB61, 
			0x350C9B64, 0x31CD86D3, 0x3C8EA00A, 0x384FBDBD,
			
			0x4C11DB70, 0x48D0C6C7, 0x4593E01E, 0x4152FDA9,
			0x5F15ADAC, 0x5BD4B01B, 0x569796C2, 0x52568B75, 
			0x6A1936C8, 0x6ED82B7F, 0x639B0DA6, 0x675A1011,
			0x791D4014, 0x7DDC5DA3, 0x709F7B7A, 0x745E66CD,
			
			0x9823B6E0, 0x9CE2AB57, 0x91A18D8E, 0x95609039,
			0x8B27C03C, 0x8FE6DD8B, 0x82A5FB52, 0x8664E6E5,
			0xBE2B5B58, 0xBAEA46EF, 0xB7A96036, 0xB3687D81, 
			0xAD2F2D84, 0xA9EE3033, 0xA4AD16EA, 0xA06C0B5D,
			
			0xD4326D90, 0xD0F37027, 0xDDB056FE, 0xD9714B49,
			0xC7361B4C, 0xC3F706FB, 0xCEB42022, 0xCA753D95,
			0xF23A8028, 0xF6FB9D9F, 0xFBB8BB46, 0xFF79A6F1, 
			0xE13EF6F4, 0xE5FFEB43, 0xE8BCCD9A, 0xEC7DD02D,

			0x34867077, 0x30476DC0, 0x3D044B19, 0x39C556AE,
			0x278206AB, 0x23431B1C, 0x2E003DC5, 0x2AC12072, 
			0x128E9DCF, 0x164F8078, 0x1B0CA6A1, 0x1FCDBB16,
			0x018AEB13, 0x054BF6A4, 0x0808D07D, 0x0CC9CDCA,

			0x7897AB07, 0x7C56B6B0, 0x71159069, 0x75D48DDE, 
			0x6B93DDDB, 0x6F52C06C, 0x6211E6B5, 0x66D0FB02,
			0x5E9F46BF, 0x5A5E5B08, 0x571D7DD1, 0x53DC6066,
			0x4D9B3063, 0x495A2DD4, 0x44190B0D, 0x40D816BA,
			
			0xACA5C697, 0xA864DB20, 0xA527FDF9, 0xA1E6E04E,
			0xBFA1B04B, 0xBB60ADFC, 0xB6238B25, 0xB2E29692,
			0x8AAD2B2F, 0x8E6C3698, 0x832F1041, 0x87EE0DF6, 
			0x99A95DF3, 0x9D684044, 0x902B669D, 0x94EA7B2A,

			0xE0B41DE7, 0xE4750050, 0xE9362689, 0xEDF73B3E,
			0xF3B06B3B, 0xF771768C, 0xFA325055, 0xFEF34DE2, 
			0xC6BCF05F, 0xC27DEDE8, 0xCF3ECB31, 0xCBFFD686,
			0xD5B88683, 0xD1799B34, 0xDC3ABDED, 0xD8FBA05A,

			0x690CE0EE, 0x6DCDFD59, 0x608EDB80, 0x644FC637, 
			0x7A089632, 0x7EC98B85, 0x738AAD5C, 0x774BB0EB,
			0x4F040D56, 0x4BC510E1, 0x46863638, 0x42472B8F,
			0x5C007B8A, 0x58C1663D, 0x558240E4, 0x51435D53, 
			
			0x251D3B9E, 0x21DC2629, 0x2C9F00F0, 0x285E1D47,
			0x36194D42, 0x32D850F5, 0x3F9B762C, 0x3B5A6B9B,
			0x0315D626, 0x07D4CB91, 0x0A97ED48, 0x0E56F0FF,
			0x1011A0FA, 0x14D0BD4D, 0x19939B94, 0x1D528623,

			0xF12F560E, 0xF5EE4BB9, 0xF8AD6D60, 0xFC6C70D7,
			0xE22B20D2, 0xE6EA3D65, 0xEBA91BBC, 0xEF68060B, 
			0xD727BBB6, 0xD3E6A601, 0xDEA580D8, 0xDA649D6F,
			0xC423CD6A, 0xC0E2D0DD, 0xCDA1F604, 0xC960EBB3,
			
			0xBD3E8D7E, 0xB9FF90C9, 0xB4BCB610, 0xB07DABA7, 
			0xAE3AFBA2, 0xAAFBE615, 0xA7B8C0CC, 0xA379DD7B,
			0x9B3660C6, 0x9FF77D71, 0x92B45BA8, 0x9675461F,
			0x8832161A, 0x8CF30BAD, 0x81B02D74, 0x857130C3, 
			
			0x5D8A9099, 0x594B8D2E, 0x5408ABF7, 0x50C9B640,
			0x4E8EE645, 0x4A4FFBF2, 0x470CDD2B, 0x43CDC09C,
			0x7B827D21, 0x7F436096, 0x7200464F, 0x76C15BF8,
			0x68860BFD, 0x6C47164A, 0x61043093, 0x65C52D24,

			0x119B4BE9, 0x155A565E, 0x18197087, 0x1CD86D30,
			0x029F3D35, 0x065E2082, 0x0B1D065B, 0x0FDC1BEC,
			0x3793A651, 0x3352BBE6, 0x3E119D3F, 0x3AD08088,
			0x2497D08D, 0x2056CD3A, 0x2D15EBE3, 0x29D4F654,

			0xC5A92679, 0xC1683BCE, 0xCC2B1D17, 0xC8EA00A0,
			0xD6AD50A5, 0xD26C4D12, 0xDF2F6BCB, 0xDBEE767C,
			0xE3A1CBC1, 0xE760D676, 0xEA23F0AF, 0xEEE2ED18,
			0xF0A5BD1D, 0xF464A0AA, 0xF9278673, 0xFDE69BC4, 

			0x89B8FD09, 0x8D79E0BE, 0x803AC667, 0x84FBDBD0,
			0x9ABC8BD5, 0x9E7D9662, 0x933EB0BB, 0x97FFAD0C,
			0xAFB010B1, 0xAB710D06, 0xA6322BDF, 0xA2F33668, 
			0xBCB4666D, 0xB8757BDA, 0xB5365D03, 0xB1F740B4,
		};		
		for(i=0;i<len;i++)
		{
			crc=(crc<<8)^table[(crc>>24)^start[i]];
		}
		return crc;
	}
public:
	static int GetVersion(const VECT_UINT8 &payload)
	{
		if ( payload.size() < 6 )
			return -1;
		return (payload[6] & 0x3E) >> 1;
	}
	static int GetSection(const VECT_UINT8 &payload)
	{
		if ( payload.size() < 7 )
			return -1;
		return payload[7];
	}
	static int GetID(const VECT_UINT8 &payload)
	{
		if ( payload.size() < 5 )
			return -1;
        return (payload[4] << 8) | payload[5];
	}
	const std::set<UINT8>	&GetProcSections()const
	{
		return m_proc_sections;
	}
	// 渡されたsectionは既に処理済み？
	bool IsProcSection(int section)
	{
		return m_proc_sections.find( static_cast<UINT8>(section) ) != m_proc_sections.end();
	}
	// 全てのサブセクションを処理したか
	bool IsFullProcSection()
	{	// 通常は==の筈
		if		( m_proc_sections.size() > static_cast<size_t>(last_section+1) )
		{	// 
			_ASSERT(FALSE);
			return true;
		}
		else if ( m_proc_sections.size() == last_section+1 )
			return true;
		else
			return false;
	}
	// 番組情報配列先頭からCRC直前までのサイズを返す
	size_t GetBodySize()
	{
		// +4 [0]ポインターフィールド(0x00) [1]table_id [2]～[3]section_syntaxとlength
		// -4 CRC
		return length; //+ 4 - 4;
	}
	bool SetData(const VECT_UINT8 &sect_dat)
	{
		if ( sect_dat.size() < 4 )
			return false;
        table_id		= sect_dat[1];
        section_syntax	= (sect_dat[2] & 0x80) >> 7;
        length			= ((sect_dat[2] & 0x0F) << 8) | sect_dat[3];
		if ( section_syntax )
		{	
			// 拡張形式の場合。
			if ( static_cast<int>(sect_dat.size()) >= length + 4 )
			{
				UINT32 crc = calc_crc( &sect_dat[1], length+3 );
				if ( crc )
				{
					DBG_LOG_WARN( _T("CRC error(table_id:0x%X)\n"), table_id );
					return false;				// CRCエラー
				}
				id		= (sect_dat[4] << 8) | sect_dat[5];
		        version = (sect_dat[6] & 0x3E) >> 1;
		        current_next = sect_dat[6] & 0x01;
		        section = sect_dat[7];
		        last_section = sect_dat[8];
				m_proc_sections.clear();
				m_proc_sections.insert( section );
			}
			else
				return false;					// セクションデータが揃ってない
		}
		return true;
	}
	// 比較。-1:p<this 0:p==this 1:p>this
	virtual int CompareTo(const CServiceInfo *p, bool ignore_section)const
	{
		// lengthは比較しない(ignore_section==trueの時比較した方がいいかも？)
		// section_syntaxは常に1の筈なので比較しない
		if ( !p )
			return -1;
		if		( p->id < id )				return -1;
		else if	( p->id > id )				return 1;
		if		( p->table_id < table_id )	return -1;
		else if	( p->table_id > table_id )	return 1;
		if		( p->current_next < current_next )	return -1;
		else if ( p->current_next > current_next )	return 1;
		if		( !ignore_section )
		{
			if		( p->section < section )	return -1;
			else if ( p->section > section )	return 1;
		}
		if		( p->last_section < last_section )	return -1;
		else if	( p->last_section > last_section )	return 1;
		if		( p->version < version )	return -1;
		else if ( p->version > version )	return 1;

		return 0;
	}
};

// PATは複数パケットに分かれることはなかろうと思うので、AddDataメソッドは実装してない
class CPAT : public CServiceInfo
{
	typedef CServiceInfo	base;
public:
	VECT_UINT16	prg_nos;
	VECT_INT16	prg_map_pids;
	INT16		network_pid;
	// ソートされたPIDの配列。上記2つとはインデックスの意味が異なる
//	std::set<INT16>		m_prg_map_sort_pids;
public:
	bool SetData(const VECT_UINT8 &sect_dat)
	{
		if ( base::SetData( sect_dat ) )
		{
			if ( section_syntax != 1 )
				return false;
			if ( table_id != TBLID_PAT )
				return false;
			VECT_UINT8::const_iterator end = sect_dat.begin() + GetBodySize();
			for ( VECT_UINT8::const_iterator p = sect_dat.begin() + c_ext_offset; p < end; p += 4 )
			{
				int prg_no = (p[0] << 8) | p[1];
				int pmt_pid = (p[2] << 8) | p[3];
				pmt_pid &= (1<<13) - 1;
				if ( prg_no > 0 && pmt_pid < 0x1fc8 )
				{
					prg_nos.push_back( prg_no );
					prg_map_pids.push_back( pmt_pid );
					//m_prg_map_sort_pids.insert( pmt_pid );
				}
				else if ( prg_no == 0 )
					network_pid = pmt_pid;
			}
			return true;
		}
		else
			return false;
	}
};

class CPMT : public CServiceInfo
{
public:
	class CStream
	{
	public:
		int			stream_type;
		int			elementary_pid;
		VECT_DESC	descriptors;

		bool		m_is_exist_pid;			// PMT定義にはない。elementaryPIDがあったかどうか。
	public:
		// 返り値は処理したバイト数.マイナスの場合、エラー
		int SetData( VECT_UINT8::const_iterator beg_pos, VECT_UINT8::const_iterator end_pos )
		{
			VECT_UINT8::const_iterator	p = beg_pos;
			if ( end_pos - beg_pos < 5 )
				return -1;
			m_is_exist_pid	= false;
			stream_type		= p[0];
			elementary_pid	= ((p[1] & (0x1F)) << 8) | p[2];
			int info_len	= ((p[3] & (0x0F)) << 8) | p[4];
			p += 5;
			descriptors.clear();
			while ( info_len > 0 )
			{
				int len = 0;
				CDescriptor desc;
				len = desc.SetData(p, end_pos);
				descriptors.push_back(desc);

				if ( len < 0 )
					return len;
				// +2は記述子のtagとlength分
				p += len+2;
				info_len -= len+2;
			}
			_ASSERT( info_len == 0 );
			return p - beg_pos;
		}
	};
	typedef CServiceInfo			base;
	typedef std::vector<CStream>	VECT_STRM;
public:
	int			pcr_pid;
	VECT_DESC	descriptors;
	VECT_STRM	stream_tbl;

	int			m_self_pid;				// 自分自身のPID。PMT定義にはない。PATと情報をリンクするのに使用
	UINT64		m_file_pos;				// 入力ファイルの位置
public:
	bool SetData(const VECT_UINT8 &sect_dat)
	{
		if ( base::SetData( sect_dat ) )
		{
			VECT_UINT8::const_iterator  p = sect_dat.begin() + c_ext_offset,
										p_end = sect_dat.begin() + GetBodySize();
			if ( section_syntax != 1 )
				return false;
			// 未定義テーブルIDの場合エラー
			if ( table_id != TBLID_PMT )
			{
				_ASSERT(FALSE);				// _dump_section_data()に処理を書いてない可能性大。
				return false;
			}
			if ( sect_dat.size() < 4 )
				return false;
			pcr_pid		= ((p[0] & 0x1f) << 8) | p[1];
			int info_len= ((p[2] & 0x0f) << 8) | p[3];
			p += 4;
			descriptors.clear();
			while ( info_len > 0 )
			{
				int len = 0;
				CDescriptor desc;
				len = desc.SetData( p, p_end );
				if ( len < 0 )
					return false;
				descriptors.push_back( desc );

				// +2は記述子のtagとlength分
				p += len+2;
				info_len -= len+2;
			}
			_ASSERT( info_len == 0 );
			stream_tbl.clear();
			while ( p < p_end )
			{
				CStream stream;
				int len = stream.SetData( p, p_end );
				if ( len < 0 )
					return false;
				stream_tbl.push_back( stream );
				p += len;
			}
			_ASSERT( p == p_end );
			return true;
		}
		else
			return false;
	}
	// 同じセクション内に属するデータを追加する
	// 同じセクションに属するかどうかはチェックしない(呼び出し前にチェックしておくこと)
	void AddData(const CPMT &pmt)
	{
		// デバッグ時は念のためチェックする
		_ASSERT( pmt.id == id );
		_ASSERT( pmt.pcr_pid == pcr_pid );
		_ASSERT( pmt.last_section == last_section );
		_ASSERT( pmt.version == version );
		if ( !IsProcSection( pmt.section ) )
		{	// ストリームテーブルを追加
			if ( pmt.stream_tbl.size() > 0 )
				std::copy( pmt.stream_tbl.begin(), pmt.stream_tbl.end(), std::back_inserter(stream_tbl) );
			m_proc_sections.insert( pmt.section );
		}
		else
		{
			int i = 0;//dummy
		}
	}
	int CompareTo(const CServiceInfo *p, bool ignore_section)const
	{	
		const CPMT *pp = dynamic_cast<const CPMT*>(p);
		int ret = 0, ret2;
		if ( !pp )
			return -1;
		if		( pp->m_self_pid < m_self_pid )	ret = -1;
		else if ( pp->m_self_pid > m_self_pid )	ret = 1;
		else if	( pp->pcr_pid < pcr_pid )		ret = -1;
		else if ( pp->pcr_pid > pcr_pid )		ret = 1;
		ret2 = base::CompareTo( p, ignore_section );
		_ASSERT( !(!ret2 && ret) );		// こういうパターンがあるのかどうかチェック
		if ( !ret && !ret2 )
		{	
			// 複数セクションに分かれてるとき、program_info_lengthの扱いは？
			if		( pp->descriptors.size() < descriptors.size() )	ret = -1;
			else if	( pp->descriptors.size() > descriptors.size() )	ret = 1;
			else											return 0;
			_ASSERT(FALSE);
			return ret;
		}
		else if ( ret )
			return ret;
		else
			return ret2;
	}
};

class CEIT : public CServiceInfo
{
public:
	class CEvent
	{
	public:
		int			event_id;
		TIME_JST40	start_time;
		TIME_BCD	duration;
		int			running_state;
		int			free_CA_mode;
		VECT_DESC	descriptors;
	public:
		// 返り値は処理したバイト数.マイナスの場合、エラー
		int SetData( VECT_UINT8::const_iterator beg_pos, VECT_UINT8::const_iterator end_pos )
		{
			VECT_UINT8::const_iterator	p = beg_pos;
			if ( end_pos - beg_pos < 12 )
				return -1;
			event_id		= (p[0]<<8)|p[1];
			start_time		= ((UINT64)p[2]<<32);
			start_time		&= ((UINT64)1<<40)-1;
			start_time		|= ((UINT64)p[3]<<24)|(p[4]<<16)|(p[5]<<8)|p[6];
			duration		= (p[7]<<16)|(p[8]<< 8)|p[9];
			running_state	= (p[10]&0xE0)>>5;
			free_CA_mode	= (p[10]&0x10)>>4;

			int	loop_len	= ((p[10]&0xF)<<8)|p[11];
			p += 12;
			descriptors.clear();
			while ( loop_len > 0 )
			{
				int len = 0;
				CDescriptor desc;
				len = desc.SetData(p, end_pos);
				descriptors.push_back(desc);
				if ( len < 0 )
					return len;
				// +2は記述子のtagとlength分
				p += len+2;
				loop_len -= len+2;
			}
			_ASSERT( loop_len == 0 );
			return p - beg_pos;
		}
	};
	typedef CServiceInfo			base;
	typedef std::vector<CEvent>		VEC_EVT;
public:
	CEIT(){}
public:
	int		transport_stream_id;
	int		original_network_id;
	int		segment_last_section_number;
	int		last_table_id;
	// event_idの順で並んでる
	VEC_EVT	event_tbl;
public:
	// ignore_sch	スケジュールEITを無視するか。
	//				trueの場合でテーブルIDが範囲内の場合、このクラスのメンバに値をセットしないが、返り値はtrueとなる
	//				後続のp_is_ignoreで無視されたかどうかを判別できる
	// p_is_ignore	無視されたかどうかを返す。ignore_schをtrueにしてテーブルIDが範囲内の場合にtrueをセットする
	bool SetData(const VECT_UINT8 &sect_dat, bool ignore_sch, bool *p_is_ignore)
	{
		if ( base::SetData( sect_dat ) )
		{
			VECT_UINT8::const_iterator  p = sect_dat.begin() + c_ext_offset,
										p_end = sect_dat.begin() + GetBodySize();
			if ( section_syntax != 1 )
				return false;
			if ( sect_dat.size() < 5 )
				return false;
			if ( p_is_ignore ) *p_is_ignore = false;
			// 範囲外テーブルIDの場合エラー
			if		( table_id == TBLID_EIT_EVMY || table_id == TBLID_EIT_EVOT )
				;		// 何もしない
			else if	(	(TBLID_EIT_SCMY_B <= table_id && table_id <= TBLID_EIT_SCMY_E)
				||		(TBLID_EIT_SCOT_B <= table_id && table_id <= TBLID_EIT_SCOT_E) )
			{
				if ( ignore_sch )
				{
					if ( p_is_ignore ) *p_is_ignore = true;
					return true;
				}
			}
			else
				return false;		// 未定義
			// メンバに値をセット
			transport_stream_id = (p[0]<<8)|p[1];
			original_network_id = (p[2]<<8)|p[3];
			segment_last_section_number = p[4];
			last_table_id = p[5];
			p+=6;
			while ( p < p_end )
			{
				CEvent evt;
				int len = evt.SetData( p, p_end );
				if ( len < 0 )
					return false;
				event_tbl.push_back( evt );
				p += len;
			}
			return true;
		}
		else
			return false;
	}
	// 同じセクション内に属するデータを追加する
	// 同じセクションに属するかどうかはチェックしない(呼び出し前にチェックしておくこと)
	void AddData(const CEIT &eit)
	{
		_ASSERT( eit.id == id );
		_ASSERT( eit.transport_stream_id == transport_stream_id );
		_ASSERT( eit.original_network_id == original_network_id );
		_ASSERT( eit.last_section == last_section );
		_ASSERT( eit.version == version );
		if ( !IsProcSection( eit.section ) )
		{	// イベントテーブルを追加
			if ( eit.event_tbl.size() > 0 )
			{
				std::copy( eit.event_tbl.begin(), eit.event_tbl.end(), std::back_inserter(event_tbl) );
			}
			m_proc_sections.insert( eit.section );
		}
		else
		{
			int i = 0;//dummy
		}
	}
	int CompareTo(const CServiceInfo *p, bool ignore_section)const
	{	
		const CEIT *pe = dynamic_cast<const CEIT*>(p);
		int ret = 0, ret2;
		if ( !pe )
			return -1;
		// transport_stream_id->id(service_id)->network_idの順で並べる
		if		( pe->transport_stream_id < transport_stream_id )	ret = -1;
		else if ( pe->transport_stream_id > transport_stream_id )	ret = 1;
		else if	( pe->id < id )										ret = -1;
		else if	( pe->id > id )										ret = 1;
		else if	( pe->original_network_id < original_network_id )	ret = -1;
		else if ( pe->original_network_id > original_network_id )	ret = 1;
		ret2 = base::CompareTo( p, ignore_section );
		_ASSERT( !(!ret2 && ret) );		// こういうパターンがあるのかどうかチェック
		if ( !ret && !ret2 )
		{	// 
			// segment_last_section_numberとlast_table_idはよく意味が分からないんで無視していいかチェック
//			if		( pe->segment_last_section_number < segment_last_section_number )	ret = -1;
//			else if	( pe->segment_last_section_number > segment_last_section_number )	ret = 1;
//			else 
			if ( pe->last_table_id < last_table_id )	ret = -1;
			else if	( pe->last_table_id > last_table_id )	ret = 1;
			else											return 0;
			_ASSERT(FALSE);
			return ret;
		}
		else if ( ret )
			return ret;
		else
			return ret2;
	}
};

// TOT/TDT両対応
class CTOT : public CServiceInfo
{
public:
	TIME_JST40		jst_time;
	VECT_DESC		descriptors;

	time_t			m_time;				// jst_timeを変換
public:
	typedef CServiceInfo	base;
	bool SetData(const VECT_UINT8 &sect_dat)
	{
		if ( base::SetData( sect_dat ) )
		{
			VECT_UINT8::const_iterator  p = sect_dat.begin(),
										p_end = sect_dat.begin() + GetBodySize();
			if ( section_syntax != 0 )
				return false;
			if ( sect_dat.size() < 5 )
				return false;
			p += c_ext_short_offset;
			jst_time		= ((UINT64)p[0]<<32)|((UINT64)p[1]<<24)|(p[2]<<16)|(p[3]<<8)|p[4];
			jst_time		&= ((UINT64)1<<40)-1;

			m_time			= _convert_time_jst40(jst_time);
			switch ( table_id )
			{
			case TBLID_TDT:	// TDT
				return true;
			case TBLID_TOT:	// TOT
				{
					if ( sect_dat.size() < 7 )
						return false;
					CDescriptor desc;
					int loop_length = ((p[5]&0xF)<<8)|p[6];
					p += 6;
					while ( loop_length > 0 )
					{
						int len = desc.SetData( p, p_end );
						if ( len < 0 )
							return false;
						descriptors.push_back( desc );
						p += len;
						loop_length -= len;
					}
					_ASSERT( loop_length == 0 );
					return true;
				}
			default:
				return false;
			}
		}
		else
			return false;
	}
};

class CSDT : public CServiceInfo
{
public:
	class CService
	{
	public:
		int			service_id;
		int			eit_user_defined_flags;
		int			eit_schedule_flag;
		int			eit_present_following_flag;
		int			running_status;
		int			free_ca_mode;
		VECT_DESC	descriptors;
	public:
		// 返り値は処理したバイト数.マイナスの場合、エラー
		int SetData( VECT_UINT8::const_iterator beg_pos, VECT_UINT8::const_iterator end_pos )
		{
			VECT_UINT8::const_iterator p = beg_pos;
			if ( end_pos - beg_pos < 5 )
				return -1;
			service_id = (p[0] << 8) | p[1];
			eit_user_defined_flags = (p[2] >> 2) & 0x7;		// b4～b2
			eit_schedule_flag = (p[2] >> 1) & 0x1;			// b1
			eit_present_following_flag = (p[2] >> 0) & 0x1;	// b0
			running_status = (p[3] >> 5) & 0x7;				// b7～b5
			free_ca_mode = (p[3] >> 4) & 0x1;				// b4
			int loop_len = ((p[3] & 0xF) << 8)|p[4];
			p += 5;
			descriptors.clear();
			while ( loop_len > 0 )
			{
				CDescriptor desc;
				int len = 0;
				len = desc.SetData(p, end_pos);
				if ( len < 0 ) return len;
				descriptors.push_back(desc);
				// +2は記述子のtagとlength分
				p += len+2;
				loop_len -= len+2;
			}
			_ASSERT( loop_len == 0 );
			return p - beg_pos;
		}
	};
	typedef CServiceInfo			base;
	typedef std::vector<CService>	VECT_SERV;
public:
	int			original_network_id;
	VECT_SERV	services;
public:
	bool SetData(const VECT_UINT8 &sect_dat)
	{
		if ( base::SetData( sect_dat ) )
		{
			VECT_UINT8::const_iterator  p = sect_dat.begin(),
										p_end = sect_dat.begin() + GetBodySize();
			if ( section_syntax != 1 )
				return false;
			if ( sect_dat.size() < 3 + c_ext_offset )
				return false;
			p += c_ext_offset;
			original_network_id = (p[0] << 8) | p[1];
			p += 3;					// 次の1バイトはreserve
			while ( p < p_end )
			{
				CService svc;
				int len = svc.SetData( p, p_end );
				if ( len < 0 )
					return false;
				services.push_back( svc );
				p += len;
			}
			return true;
		}
		else
			return false;
	}
	// 同じセクション内に属するデータを追加する
	// 同じセクションに属するかどうかはチェックしない(呼び出し前にチェックしておくこと)
	void AddData(const CSDT &sdt)
	{
		_ASSERT( sdt.id == id );
		_ASSERT( sdt.original_network_id == original_network_id );
		_ASSERT( sdt.last_section == last_section );
		_ASSERT( sdt.version == version );
		if ( !IsProcSection( sdt.section ) )
		{	// イベントテーブルを追加
			if ( sdt.services.size() > 0 )
			{
				std::copy( sdt.services.begin(), sdt.services.end(), std::back_inserter(services) );
			}
			m_proc_sections.insert( sdt.section );
		}
		else
		{
			int i = 0;//dummy
		}
	}
	int CompareTo(const CServiceInfo *p, bool ignore_section)const
	{	
		const CSDT *pp = dynamic_cast<const CSDT*>(p);
		int ret = 0;
		if ( !pp )
			return -1;
		if		( pp->original_network_id < original_network_id )	return -1;
		else if ( pp->original_network_id > original_network_id )	return  1;
		else if	( pp->id < id )										return -1;
		else if	( pp->id > id )										return  1;
		return base::CompareTo( p, ignore_section );
	}
};

#endif