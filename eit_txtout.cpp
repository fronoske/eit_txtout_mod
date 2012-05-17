// TsEITRead.cpp : コンソール アプリケーションのエントリ ポイントを定義します。
//
#include "stdafx.h"
#include "tsdata.h"
#include "ts_sidef.h"
#include "ts_packet.h"
#include "wingetopt.h"

//TODO: -C (range)	【特殊】(output)がテキストファイルではなく、入力TSファイルの指定した範囲を切り取ったものになります
//			(range)は(start_packet_idx):(end_packet_idx)
using namespace std;

#define			STR_STL_LOCALE	"japanese"					// STLのlocaleに渡す文字列
#define			MAX_PID_NUM		8192

//#define _TEST_USE_FS_

// mainの返り値
enum
{
	ERR_PARAM = 1,
	ERR_INPUTFILE,
	ERR_OUTPUTFILE,
	ERR_NOTOT,
	ERR_NOPAT,
	ERR_NOSDT,
};

// PID毎のセクションバッファ
// mapを使ってもメモリ節約量は知れてるので配列で持つ
static CSectionBuf	*ev_pSectionBuf[MAX_PID_NUM];

static UINT64		ev_BefSectionFilePos[MAX_PID_NUM];		// 以前のファイル位置(先頭:1)。packet単位(byte単位ではない)

static UINT64		ev_cur_filepos;							// 現在のファイル位置(先頭:1)。packet単位(byte単位ではない)

// オプションパラメータ
enum E_READ_SIZE{RS_SIZE,RS_RATIO,RS_SEC,};
static bool			ev_opt_is_out_patpmt = false;			// PAT/PMTを出力する？
static bool			ev_opt_is_all_out = false;				// 全サービス/イベント情報を出力する？
static bool			ev_opt_is_out_extevt = false;			// 拡張形式イベント記述子を出力する？
static int			ev_opt_offset_read = 0;					// 読み込みオフセット
static bool			ev_opt_offset_read_is_ratio = false;
static int			ev_opt_max_read_size = 0;				// 最大読み込みサイズ(*188*1000byte)
static E_READ_SIZE	ev_opt_max_read_type = RS_SIZE;
static int			ev_opt_offset_beg = 0;					// 開始オフセット秒数
static int			ev_opt_offset_end = 0;					// 終了オフセット秒数
static bool			ev_opt_read_eit_sch = false;			// EIT(スケジュール)を読み込む？
static bool			ev_opt_verbose = false;					// 詳細出力モード
static bool			ev_opt_no_progress = false;				// 進捗表示を出さない
static bool			ev_opt_outtime_offset = false;			// 
static int			ev_opt_evt_output_type = 0;
static const TCHAR	*ev_opt_input_fname = NULL;				// 現在処理してるTSファイル名(デバッグログ用に)
static const TCHAR	*ev_opt_output_fname = NULL;			// 出力ファイル
static const TCHAR	*ev_opt_dbgout_fname = NULL;
static const TCHAR	*ev_opt_dbglog_fname = NULL;

// 
static bool			ev_dbglog_flg = false;					// デバッグログフラグ。まだ1回も出力してない時FALSE
static tostream		*ev_ps_dbglog = NULL;					// デバッグログ出力用ストリーム


// TSファイル内の番組情報バッファリング用
static vector<CPAT>		ev_pats;
static vector<CEIT*>	ev_peits;		// たくさん持つ可能性があり、insertとかの実行コストがバカにならないんで、ポインタで持つ
static vector<CSDT*>	ev_psdts;		// 同上
static vector<CPMT>		ev_pmts;
static vector<CTOT>		ev_tots;		// 2個だけ保持する

// eit1 < eit2の時 trueを返す
static bool _compare_eit(const CEIT *eit1, const CEIT *eit2)
{
	const CServiceInfo *p = eit2;
	return eit1->CompareTo( p, true ) > 0;
}

// pmt1 < pmt2の時 trueを返す
static bool _compare_pmt(const CPMT &pmt1, const CPMT &pmt2)
{
	const CServiceInfo *p = &pmt2;
	return pmt1.CompareTo( p, true ) > 0;
}

// sdt1 < sdt2の時 trueを返す
static bool _compare_sdt(const CSDT *sdt1, const CSDT *sdt2)
{
	const CServiceInfo *p = sdt2;
	return sdt1->CompareTo( p, true ) > 0;
}

// 返り値：以下のいずれか。CRCエラーとかあった場合は後続パケットがあるはず
enum eSECTION_RET
{
	SEC_NG_NEXT = 0,			// NG。次パケット要求
	SEC_OK_END,					// OK。セクションデータ終了
	SEC_OK_RETRY,				// OK。
};
static eSECTION_RET _dump_section_data(const CPacket &packet, CSectionBuf *p_sec_buf, bool is_read_eit_sch)
{
	eSECTION_RET	ret = SEC_NG_NEXT;
	size_t			sec_len = 0;
	int				dbg_flg = 0;				// デバッグ用フラグ
	switch ( packet.header.pid )
	{
	case PID_PAT:
		// 簡易的に末尾の
		if ( ev_pats.size() == 0 
			|| ev_pats.back().version != CServiceInfo::GetVersion(p_sec_buf->GetData()) 
			|| ev_pats.back().id != CServiceInfo::GetID(p_sec_buf->GetData())
			)
		{
			CPAT	pat;
			if ( pat.SetData( p_sec_buf->GetData() ) )
			{
				ev_pats.push_back( pat );
				sec_len = pat.GetBodySize();
				ret = SEC_OK_END;
			}
		}
		else
			return SEC_OK_END;		// 重複データなので処理済みにしとく
		break;
	case PID_EIT_0:
	case PID_EIT_1:
	case PID_EIT_2:
		{
			//CEIT	eit;
			CEIT	*peit = new CEIT();
			bool	is_ignore;
			if ( peit->SetData( p_sec_buf->GetData(), !is_read_eit_sch, &is_ignore ) )
			{
//#ifdef _DEBUG
#if 0
				if ( peit->transport_stream_id == 16625 && peit->id == 101 && peit->table_id == 0x58 )
				{
					DBG_LOG_WARN( _T("version:%2d section:%3d segment_last:%3d last_section:%3d length:%4d\n"), 
						peit->version, peit->section, peit->segment_last_section_number, peit->last_section, peit->length );
				//	_CrtDbgBreak();
				}
#endif
				sec_len = peit->GetBodySize(); 
				if ( !is_ignore )
				{
					vector<CEIT*>::iterator pos = ev_peits.end();
					if ( ev_peits.size() > 0 )
						pos = lower_bound( ev_peits.begin(), ev_peits.end(), peit, _compare_eit );
					int idx = pos - ev_peits.begin();
					if ( pos == ev_peits.end() )
					{
						ev_peits.push_back( peit );
						dbg_flg = 1;
					}
					else if ( (*pos)->CompareTo( peit, true ) == 0 )
					{
						(*pos)->AddData( *peit );
						// イベントテーブルのないものに付いてはEITテーブルには入れない
						if ( (*pos)->IsFullProcSection() && (*pos)->event_tbl.size() == 0 )
						{
							delete ev_peits[idx];
							ev_peits.erase( pos );
						}
						dbg_flg = 2;
						delete peit;
					}
					else
					{
						ev_peits.insert( pos, peit );
						dbg_flg = 3;
					}
				}
				else
					delete peit;
				ret = SEC_OK_END;
			}
			else
			{
				ret = SEC_NG_NEXT;
				delete peit;
			}
		}
		break;
	case PID_TOT:
		{
			CTOT	tot;
			if ( tot.SetData( p_sec_buf->GetData() ) )
			{
				// 先頭時刻の挿入または追加
				if ( ev_tots.size() == 0 )
					ev_tots.push_back( tot );
				else if ( ev_tots.size() > 1 && tot.jst_time < ev_tots[0].jst_time )
				{
					ev_tots.erase( ev_tots.begin() );
					ev_tots.insert( ev_tots.begin(), tot );
					_ASSERT( FALSE );			// 時系列に並んでるはずなんで、一応警告
				}

				// 末尾時刻の追加
				if		( ev_tots.size() < 2 )
				{
					ev_tots.push_back( tot );
				}
				else if ( tot.jst_time > ev_tots[1].jst_time )
				{	
					ev_tots.pop_back();
					ev_tots.push_back( tot );
				}
				else
					_ASSERT( FALSE );

				sec_len = tot.GetBodySize(); 
				_ASSERT( ev_tots.size() <= 2 );
				
				ret = SEC_OK_END;
			}
		}
		break;
	case PID_SIT:
		//
		break;
	case PID_SDT:
		{
			CSDT	*psdt = new CSDT();
			if ( psdt->SetData( p_sec_buf->GetData() ) )
			{
				vector<CSDT*>::iterator pos = ev_psdts.end();
				sec_len = psdt->GetBodySize(); 
				if ( ev_psdts.size() > 0 )
					pos = lower_bound( ev_psdts.begin(), ev_psdts.end(), psdt, _compare_sdt );
				if ( pos == ev_psdts.end() )
				{
					ev_psdts.push_back( psdt );
				}
				else if ( (*pos)->CompareTo( psdt, true ) == 0 )
				{
					(*pos)->AddData( *psdt );
					delete psdt;
				}
				else
				{
					ev_psdts.insert( pos, psdt );
				}
				ret = SEC_OK_END;

			}
			else
			{
				ret = SEC_NG_NEXT;
				delete psdt;
			}
		}
		break;
	default:
		if ( ev_opt_is_out_patpmt )
		{
			CPMT	pmt;
			pmt.m_self_pid = packet.header.pid;
			pmt.m_file_pos = ev_cur_filepos;
			if ( pmt.SetData( p_sec_buf->GetData() ) )
			{
				vector<CPMT>::iterator pos = ev_pmts.end();
				if ( ev_pmts.size() > 0 )
					pos = lower_bound( ev_pmts.begin(), ev_pmts.end(), pmt, _compare_pmt );
				if ( pos == ev_pmts.end() )
					ev_pmts.push_back( pmt );
				else if ( pos->CompareTo( &pmt, true ) == 0 )
				{
					pos->AddData( pmt );
				}
				else
					ev_pmts.insert( pos, pmt );
				sec_len = pmt.GetBodySize(); 
				ret = SEC_OK_END;
			}
			else
				ret = SEC_NG_NEXT;
		}
		break;
	}
	if ( sec_len && p_sec_buf->GetData().size() > sec_len + 4 )
	{
		int last_pt = sec_len + 4;
		VECT_UINT8::const_iterator it = p_sec_buf->GetData().begin() + last_pt;
		if ( *it != 0xFF )
		{
			// 続く場合。
			VECT_UINT8	dmy_buf;
			dmy_buf.push_back( 0 );			// ポインターフィールドを先頭に挿入
			std::copy( it, p_sec_buf->GetData().end(), back_inserter(dmy_buf) );
			p_sec_buf->ClearData();
			p_sec_buf->AddData( packet.header.index, &dmy_buf[0], &dmy_buf[0] + dmy_buf.size() );
			return SEC_OK_RETRY;
		}
		else
			int iii = 0;
	}
	return ret;
}

// イベント大分類文字列取得
static const TCHAR*	_get_genre_lvl1_str(int genre)
{
	static const TCHAR *c_lvl1_str[] = {
		_T("ニュース／報道"),			// 0x0  
		_T("スポーツ"),					// 0x1  
		_T("情報／ワイドショー"),		// 0x2  
		_T("ドラマ"),					// 0x3  
		_T("音楽"),						// 0x4  
		_T("バラエティ"),				// 0x5  
		_T("映画"),						// 0x6  
		_T("アニメ／特撮"),				// 0x7  
		_T("ドキュメンタリー／教養"),	// 0x8  
		_T("劇場／公演"),				// 0x9  
		_T("趣味／教育"),				// 0xA  
		_T("福祉"),						// 0xB  
		_T("予備"),						// 0xC  
		_T("予備"),						// 0xD  
		_T("拡張"),						// 0xE  
		_T("その他 "),					// 0xF  

		_T("ジャンル指定なし"),				
	};
	_ASSERT( sizeof(c_lvl1_str) / sizeof(c_lvl1_str[0]) == 0x11 );
	if ( genre < 0 )
		return c_lvl1_str[0x10];
	else
		return c_lvl1_str[(genre>>4)&0xF];
}

// イベント中分類文字列取得
static const TCHAR*	_get_genre_lvl2_str(int genre)
{
	static const TCHAR *c_lvl2_str[] = {
		_T("定時・総合"),					// 0x0 0x0  
		_T("天気"),							// 0x0 0x1  
		_T("特集・ドキュメント"),			// 0x0 0x2  
		_T("政治・国会"),					// 0x0 0x3  
		_T("経済・市況"),					// 0x0  0x4  
		_T("海外・国際"),					// 0x0  0x5  
		_T("解説"),							// 0x0  0x6  
		_T("討論・会談"),					// 0x0  0x7  
		_T("報道特番"),						// 0x0  0x8  
		_T("ローカル・地域"),				// 0x0  0x9  
		_T("交通"),							// 0x0  0xA  
		_T(""),								// 0x0  0xB   
		_T(""),								// 0x0  0xC   
		_T(""),								// 0x0  0xD   
		_T(""),								// 0x0  0xE   
		_T("その他"),						// 0x0  0xF  

		_T("スポーツニュース"),				// 0x1 0x0  
		_T("野球"),							// 0x1 0x1  
		_T("サッカー"),						// 0x1 0x2  
		_T("ゴルフ"),						// 0x1 0x3  
		_T("その他の球技"),					// 0x1 0x4  
		_T("相撲・格闘技"),					// 0x1 0x5  
		_T("オリンピック・国際大会"),		// 0x1 0x6  
		_T("マラソン・陸上・水泳"),			// 0x1 0x7  
		_T("モータースポーツ"),				// 0x1 0x8  
		_T("マリン・ウィンタースポーツ"),	// 0x1 0x9  
		_T("競馬・公営競技"),				// 0x1 0xA  
		_T(""),								// 0x1 0xB   
		_T(""),								// 0x1 0xC   
		_T(""),								// 0x1 0xD   
		_T(""),								// 0x1 0xE   
		_T("その他"),						// 0x1 0xF  

		_T("芸能・ワイドショー"),			// 0x2 0x0  
		_T("ファッション"),					// 0x2 0x1  
		_T("暮らし・住まい"),				// 0x2  0x2  
		_T("健康・医療"),					// 0x2  0x3  
		_T("ショッピング・通販"),			// 0x2  0x4  
		_T("グルメ・料理"),					// 0x2  0x5  
		_T("イベント"),						// 0x2  0x6  
		_T("番組紹介・お知らせ"),			// 0x2  0x7  
		_T(""),								// 0x2 0x8   
		_T(""),								// 0x2 0x9   
		_T(""),								// 0x2 0xA   
		_T(""),								// 0x2 0xB   
		_T(""),								// 0x2 0xC   
		_T(""),								// 0x2 0xD   
		_T(""),								// 0x2 0xE   
		_T("その他"),						// 0x2 0xF  

		_T("国内ドラマ"),					// 0x3  0x0  
		_T("海外ドラマ"),					// 0x3  0x1  
		_T("時代劇"),						// 0x3  0x2  
		_T(""),								// 0x3  0x3   
		_T(""),								// 0x3  0x4   
		_T(""),								// 0x3  0x5   
		_T(""),								// 0x3  0x6   
		_T(""),								// 0x3  0x7   
		_T(""),								// 0x3  0x8   
		_T(""),								// 0x3  0x9   
		_T(""),								// 0x3  0xA   
		_T(""),								// 0x3  0xB   
		_T(""),								// 0x3  0xC   
		_T(""),								// 0x3  0xD   
		_T(""),								// 0x3  0xE   
		_T("その他"),						// 0x3  0xF  

		_T("国内ロック・ポップス"),			// 0x4 0x0  
		_T("海外ロック・ポップス"),			// 0x4 0x1  
		_T("クラシック・オペラ"),			// 0x4 0x2  
		_T("ジャズ・フュージョン"),			// 0x4 0x3  
		_T("歌謡曲・演歌"),					// 0x4 0x4  
		_T("ライブ・コンサート"),			// 0x4 0x5  
		_T("ランキング・リクエスト"),		// 0x4 0x6  
		_T("カラオケ・のど自慢"),			// 0x4 0x7  
		_T("民謡・邦楽"),					// 0x4 0x8  
		_T("童謡・キッズ"),					// 0x4 0x9  
		_T("民族音楽・ワールドミュージック"),	// 0x4 0xA  
		_T(""),									// 0x4 0xB   
		_T(""),									// 0x4 0xC   
		_T(""),									// 0x4 0xD   
		_T(""),									// 0x4 0xE   
		_T("その他"),							// 0x4 0xF  

		_T("クイズ"),							// 0x5  0x0  
		_T("ゲーム"),							// 0x5  0x1  
		_T("トークバラエティ"),					// 0x5  0x2  
		_T("お笑い・コメディ"),					// 0x5  0x3  
		_T("音楽バラエティ"),					// 0x5  0x4  
		_T("旅バラエティ"),						// 0x5  0x5  
		_T("料理バラエティ"),					// 0x5  0x6  
		_T(""),									// 0x5  0x7   
		_T(""),									// 0x5  0x8   
		_T(""),									// 0x5  0x9   
		_T(""),									// 0x5  0xA   
		_T(""),									// 0x5  0xB   
		_T(""),									// 0x5  0xC   
		_T(""),									// 0x5  0xD   
		_T(""),									// 0x5  0xE   
		_T("その他"),							// 0x5  0xF  

		_T("洋画"),								// 0x6 0x0  
		_T("邦画"),								// 0x6 0x1  
		_T("アニメ"),							// 0x6 0x2  
		_T(""),									// 0x6 0x3   
		_T(""),									// 0x6 0x4   
		_T(""),									// 0x6 0x5   
		_T(""),									// 0x6 0x6   
		_T(""),									// 0x6 0x7   
		_T(""),									// 0x6 0x8   
		_T(""),									// 0x6 0x9   
		_T(""),									// 0x6 0xA   
		_T(""),									// 0x6 0xB   
		_T(""),									// 0x6 0xC   
		_T(""),									// 0x6 0xD   
		_T(""),									// 0x6 0xE   
		_T("その他"),							// 0x6 0xF  

		_T("国内アニメ"),		// 0x7  0x0  
		_T("海外アニメ"),		// 0x7  0x1  
		_T("特撮"),		// 0x7  0x2  
		_T(""),		// 0x7  0x3   
		_T(""),		// 0x7  0x4   
		_T(""),		// 0x7  0x5   
		_T(""),		// 0x7  0x6   
		_T(""),		// 0x7  0x7   
		_T(""),		// 0x7  0x8   
		_T(""),		// 0x7  0x9   
		_T(""),		// 0x7  0xA   
		_T(""),		// 0x7  0xB   
		_T(""),		// 0x7  0xC   
		_T(""),		// 0x7  0xD   
		_T(""),		// 0x7  0xE   
		_T("その他"),		// 0x7  0xF  

		_T("社会・時事"),		// 0x8 0x0  
		_T("歴史・紀行"),		// 0x8 0x1  
		_T("自然・動物・環境"),		// 0x8 0x2  
		_T("宇宙・科学・医学"),		// 0x8 0x3  
		_T("カルチャー・伝統文化"),		// 0x8 0x4  
		_T("文学・文芸"),		// 0x8 0x5  
		_T("スポーツ"),		// 0x8 0x6  
		_T("ドキュメンタリー全般"),		// 0x8 0x7  
		_T("インタビュー・討論"),		// 0x8 0x8  
		_T(""),		// 0x8 0x9   
		_T(""),		// 0x8 0xA   
		_T(""),		// 0x8 0xB   
		_T(""),		// 0x8 0xC   
		_T(""),		// 0x8 0xD   
		_T(""),		// 0x8 0xE   
		_T("その他"),		// 0x8 0xF  

		_T("現代劇・新劇"),		// 0x9  0x0  
		_T("ミュージカル"),		// 0x9  0x1  
		_T("ダンス・バレエ"),		// 0x9  0x2  
		_T("落語・演芸"),		// 0x9  0x3  
		_T("歌舞伎・古典"),		// 0x9  0x4  
		_T(""),		// 0x9  0x5   
		_T(""),		// 0x9  0x6   
		_T(""),		// 0x9  0x7   
		_T(""),		// 0x9  0x8   
		_T(""),		// 0x9  0x9   
		_T(""),		// 0x9  0xA   
		_T(""),		// 0x9  0xB   
		_T(""),		// 0x9  0xC   
		_T(""),		// 0x9  0xD   
		_T(""),		// 0x9  0xE   
		_T("その他"),		// 0x9  0xF  

		_T("旅・釣り・アウトドア"),		// 0xA 0x0  
		_T("園芸・ペット・手芸"),		// 0xA 0x1  
		_T("音楽・美術・工芸"),		// 0xA 0x2  
		_T("囲碁・将棋"),		// 0xA 0x3  
		_T("麻雀・パチンコ"),		// 0xA 0x4  
		_T("車・オートバイ"),		// 0xA 0x5  
		_T("コンピュータ・ＴＶゲーム"),		// 0xA 0x6  
		_T("会話・語学"),		// 0xA 0x7  
		_T("幼児・小学生"),		// 0xA 0x8  
		_T("中学生・高校生"),		// 0xA 0x9  
		_T("大学生・受験"),		// 0xA 0xA  
		_T("生涯教育・資格"),		// 0xA 0xB  
		_T("教育問題"),		// 0xA 0xC  
		_T(""),		// 0xA 0xD   
		_T(""),		// 0xA 0xE   
		_T("その他"),		// 0xA 0xF  
  
		_T("高齢者"),		// 0xB  0x0  
		_T("障害者"),		// 0xB  0x1  
		_T("社会福祉"),		// 0xB  0x2  
		_T("ボランティア"),		// 0xB  0x3  
		_T("手話"),		// 0xB  0x4  
		_T("文字（字幕）"),		// 0xB  0x5  
		_T("音声解説"),		// 0xB  0x6  
		_T(""),		// 0xB  0x7   
		_T(""),		// 0xB  0x8   
		_T(""),		// 0xB  0x9   
		_T(""),		// 0xB  0xA   
		_T(""),		// 0xB  0xB   
		_T(""),		// 0xB  0xC   
		_T(""),		// 0xB  0xD   
		_T(""),		// 0xB  0xE   
		_T("その他"),		// 0xB  0xF  

		_T(""),		// 0xC 0x0   
		_T(""),		// 0xC 0x1   
		_T(""),		// 0xC 0x2   
		_T(""),		// 0xC 0x3   
		_T(""),		// 0xC 0x4   
		_T(""),		// 0xC 0x5   
		_T(""),		// 0xC 0x6   
		_T(""),		// 0xC 0x7   
		_T(""),		// 0xC 0x8   
		_T(""),		// 0xC 0x9   
		_T(""),		// 0xC 0xA   
		_T(""),		// 0xC 0xB   
		_T(""),		// 0xC 0xC   
		_T(""),		// 0xC 0xD   
		_T(""),		// 0xC 0xE   
		_T(""),		// 0xC 0xF   

		_T(""),		// 0xD 0x0   
		_T(""),		// 0xD 0x1   
		_T(""),		// 0xD 0x2   
		_T(""),		// 0xD 0x3   
		_T(""),		// 0xD 0x4   
		_T(""),		// 0xD 0x5   
		_T(""),		// 0xD 0x6   
		_T(""),		// 0xD 0x7   
		_T(""),		// 0xD 0x8   
		_T(""),		// 0xD 0x9   
		_T(""),		// 0xD 0xA   
		_T(""),		// 0xD 0xB   
		_T(""),		// 0xD 0xC   
		_T(""),		// 0xD 0xD   
		_T(""),		// 0xD 0xE   
		_T(""),		// 0xD 0xF   

		_T("BS/地上デジタル放送用番組付属情報"),		// 0xE 0x0  
		_T("広帯域 CSデジタル放送用拡張"),		// 0xE 0x1  
		_T("衛星デジタル音声放送用拡張"),		// 0xE 0x2  
		_T("サーバー型番組付属情報"),		// 0xE 0x3  
		_T("IP放送用番組付属情報"),		// 0xE 0x4  
		_T(""),		// 0xE 0x5   
		_T(""),		// 0xE 0x6   
		_T(""),		// 0xE 0x7   
		_T(""),		// 0xE 0x8   
		_T(""),		// 0xE 0x9   
		_T(""),		// 0xE 0xA   
		_T(""),		// 0xE 0xB   
		_T(""),		// 0xE 0xC   
		_T(""),		// 0xE 0xD   
		_T(""),		// 0xE 0xE   
		_T(""),		// 0xE 0xF   

		_T(""),		// 0xF  0x0   
		_T(""),		// 0xF  0x1   
		_T(""),		// 0xF  0x2   
		_T(""),		// 0xF  0x3   
		_T(""),		// 0xF  0x4   
		_T(""),		// 0xF  0x5   
		_T(""),		// 0xF  0x6   
		_T(""),		// 0xF  0x7   
		_T(""),		// 0xF  0x8   
		_T(""),		// 0xF  0x9   
		_T(""),		// 0xF  0xA   
		_T(""),		// 0xF  0xB   
		_T(""),		// 0xF  0xC   
		_T(""),		// 0xF  0xD   
		_T(""),		// 0xF  0xE   
		_T("その他"),		// 0xF  0xF  
	};
	static const TCHAR *c_undefined_str = _T("未定義");
	static const TCHAR *c_unassign_str = _T("");
	
	_ASSERT( sizeof(c_lvl2_str) / sizeof(c_lvl2_str[0]) == 0x100 );
	if ( genre < 0 )
		return c_unassign_str;
	else
	{
		const TCHAR *pret = c_lvl2_str[genre&0xFF];
		if ( !*pret || !pret )			// ""の場合、未定義
			return c_undefined_str;
		else
			return pret;
	}
}

static TCHAR *_get_datetime_str(time_t tt)
{
	static TCHAR	sz_tbuf[64];

	_tcsftime( sz_tbuf, 64, _T("%Y/%m/%d %H:%M:%S"), localtime(&tt) );

	return sz_tbuf;
}

static UINT64 _make_key( int network_id, int transport_id, int service_id)
{
	UINT64	key = network_id;
	key <<= 16;
	key |= transport_id;
	key <<= 16;
	key |= service_id;
	return key;
}

// SDT内サービスを探す。
// 見つからなかったとき
//		SDTに該当項目がない			→ev_psdtの最初の項目のservices.end()
//		services配列内に項目がない	→ev_psdtの最後の項目のservices.end()
// ev_psdtsの項目数0でないこと前提
static bool  _find_sdt_svc(int network_id, int transport_id, int service_id, CSDT::VECT_SERV::iterator &it)
{
	CSDT	key;
	key.id					= transport_id;
	key.original_network_id = network_id;
	key.table_id			= -1;		// network_id,id(transport_id)が等しい最初の項目を返すようにするため。CSDT::CompareTo()の実装依存なんであまりよろしくない
	
	vector<CSDT*>::iterator p = lower_bound( ev_psdts.begin(), ev_psdts.end(), &key, _compare_sdt );
	if ( p == ev_psdts.end() || (*p)->id != transport_id || (*p)->original_network_id != network_id )
	{
		return false;
	}
	for ( ; p != ev_psdts.end(); ++p )
	{
		CSDT *psdt = *p;
		if ( psdt->original_network_id != network_id || psdt->id != transport_id )
			break;
		for ( CSDT::VECT_SERV::iterator is = psdt->services.begin(); is != psdt->services.end(); ++is )
		{
			if ( is->service_id == service_id )
			{
				// 発見。複数あった場合、後のSDTを検索した方がいい？？
				it = is;
				return true;
			}
		}
	}
	return false;
}

static SServiceInfo*	_make_program_info(MAP_SERV	&services, const CEIT *peit, time_t tot_start, time_t tot_end)
{
	SServiceInfo	*psvc;
	{
		UINT64 map_key = _make_key( peit->original_network_id, peit->transport_stream_id, peit->id );
		MAP_SERV::_Pairib pib = services.insert( MAP_SERV::value_type( map_key, SServiceInfo() ) );

		psvc = &pib.first->second;
		if ( pib.second )
		{	// 新規登録時
			// メンバ初期化
			psvc->network_id		= peit->original_network_id;
			psvc->transport_id	= peit->transport_stream_id;
			psvc->service_id		= peit->id;
			
			// 該当サービスのSDTを検索し、名前などをセットする
			CSDT::VECT_SERV::iterator is;
			if ( !_find_sdt_svc( psvc->network_id, psvc->transport_id, psvc->service_id, is ) )
			{
				DBG_LOG_ASSERT( _T("\rnetwork_id:%d trans_id:%d svc_id:%dのSDT/Serviceがない\n"), psvc->network_id, psvc->transport_id, psvc->service_id );
			}
			else
			{
				for ( VECT_DESC::iterator iv = is->descriptors.begin(); iv != is->descriptors.end(); ++iv )
				{
					switch ( iv->tag )
					{
					case DSCTAG_SERVICE:
						{
							CServiceDesc desc(*iv);
							psvc->prov_name = desc.service_provider_name;
							psvc->name = desc.service_name;
							psvc->service_type = desc.service_type;
							goto end_serv_find_loop;
						}
					}
				}
end_serv_find_loop:
				;
			}
		}
	}
	for ( vector<CEIT::CEvent>::const_iterator ie = peit->event_tbl.begin(); ie != peit->event_tbl.end(); ie++ )
	{
		bool	b_add_prg = false;
		time_t	t_start, t_end;
		tm		tm_b;

		_convert_time_jst40( ie->start_time, tm_b );
		t_start = mktime( &tm_b );
		_convert_time_bcd( ie->duration, tm_b );
		t_end = t_start + tm_b.tm_hour * 3600 + tm_b.tm_min * 60 + tm_b.tm_sec;

		if ( ev_opt_is_all_out )
			b_add_prg = true;
		else
		{
			// tot_start～tot_endの期間にt_start、t_endがはいってるか
			if ( t_start < tot_start )
				b_add_prg = tot_start <= t_end;
			else
				b_add_prg = t_start <= tot_end;
		}

		if ( b_add_prg )
		{
			SProgramInfo *pprg;
			{
				MAP_PRG::_Pairib pib = psvc->programs.insert( MAP_PRG::value_type( ie->event_id, SProgramInfo() ) );
				pprg = &pib.first->second;
				if ( pib.second )
				{	// 新規登録時、メンバを初期化
					pprg->event_id = ie->event_id;
					pprg->start_time = t_start;
					pprg->end_time = t_end;
					pprg->genre = -1;
					pprg->flg = 0;
					pprg->stream_content = -1;
					pprg->component_type = -1;
					pprg->ext_evt_flg = 0;
				}
			}
			const int prg_all_flg = 4;
			// スケジュール系と現在/次の番組系でevent_idが重複する可能性有り。
			// 何回もセットすると遅いかもしれないんで、既に設定した情報はセットしないようにする
			// 高々２回しかセットしない(筈)なので、気にしなくてもいいかもしんない(^_^;
			if ( pprg->flg ^ ((1<<prg_all_flg)-1) )
			{
				for ( VECT_DESC::const_iterator id = ie->descriptors.begin(); id != ie->descriptors.end(); id++ )
				{
					try
					{
						switch ( id->tag )
						{
						case DSCTAG_SHORTEVT:
							if ( !(pprg->flg & (1<<0)) )
							{
								CShortEventDesc	dsc(*id);

								pprg->name_str = dsc.name_str;
								pprg->desc_str = dsc.desc_str;
								if ( dsc.name_str.length() > 0 && dsc.desc_str.length() > 0 )
									pprg->flg |= 1<<0;
							}
							break;
						case DSCTAG_CONTENT:
							if ( !(pprg->flg & (1<<1)) )
							{
								CContentDesc dsc(*id);

								pprg->genre = (dsc.content_nibble_level_1<<4) | dsc.content_nibble_level_2;
								pprg->flg |= 1<<1;
							}
							break;
						case DSCTAG_COMPONENT:
							if ( !(pprg->flg & (1<<2)) )
							{
								CComponentDesc dsc(*id);

								pprg->stream_content = dsc.stream_content;
								pprg->component_type = dsc.component_type;
								pprg->flg |= 1<<2;
							}
							break;
						case DSCTAG_EXTEVT:
							if ( !(pprg->flg & (1<<3)) )
							{
								CExtEventDesc	dsc(*id);
								static VECT_UINT8 *st_p_prev_dat = NULL;		// 項目名空白の拡張記述はその前の項目ありの記述からつながるみたい。
																				// staticにせんとforループの外で宣言しとけば良いような気もする。

								for ( CExtEventDesc::VECT_ITEM::iterator ix = dsc.items.begin(); ix != dsc.items.end(); ++ix )
								{
									if ( (*ix).description.length() > 0 || !st_p_prev_dat )
									{
										CNTR_EXTEV::_Pairib pib = pprg->ext_evts.insert( CNTR_EXTEV::value_type((*ix).description, VECT_UINT8()) );
										VECT_UINT8 &data = (*pib.first).second;
										// 項目記述を末尾に追加
										copy( (*ix).text.begin(), (*ix).text.end(), back_inserter(data) );
										st_p_prev_dat = &data;									
									}
									else
									{
										// 前の項目記述の末尾に追加
										copy( (*ix).text.begin(), (*ix).text.end(), back_inserter(*st_p_prev_dat) );
									}
								}
								if ( dsc.text.size() > 0 )
								{
									CNTR_EXTEV::_Pairib pib = pprg->ext_evts.insert( CNTR_EXTEV::value_type(_T(""), VECT_UINT8()) );
									VECT_UINT8 &data = (*pib.first).second;

									copy( dsc.text.begin(), dsc.text.end(), back_inserter(data) );
								}
								pprg->ext_evt_flg |= (1<<dsc.descriptor_number);
								// 全部の拡張形式イベント記述子を処理したら項目セットビットを立てる。
								// 同じイベントIDで別内容の拡張形式イベント記述子が来たら、それは無視される(いいのかな？)
								if ( !(pprg->ext_evt_flg ^ ((1<<(dsc.last_descriptor_number+1))-1)) )
								{
									pprg->flg |= 1<<3;
									st_p_prev_dat = NULL;
								}
							}
							break;
						// 他の記述子を追加する場合、prg_all_flgを増やして、pprg->flgで立てるビットを他と重複しないようにすること
						}
					}
					catch (CDefExcept &d)
					{
						DBG_LOG_ASSERT( _T("\rservice:%d event:%d dscidx:%d %s\n"), 
							psvc->service_id, pprg->event_id, id - ie->descriptors.begin(), d.str_msg );
					}
				}
			}
		}
	}
	return psvc;
}

static bool _compare_prg(const MAP_PRG::const_iterator &it1, const MAP_PRG::const_iterator &it2)
{
	return it1->second.start_time < it2->second.start_time;
}

// 情報の構成
// lpszFileName			出力ファイル名
// time_offset_out		TOT出力時にオフセット影響を受ける？
// offset_beg			開始時間オフセット(秒)
// offset_end			終了時間オフセット(秒)
static int _output_program_info(tofstream &ofs)
{
	MAP_SERV		services;
	// 通常TIME_JST40の比較だけでいけると思うが、0:00を跨ぐとおかしくなりそうなので、time_tで時間比較する
	time_t		tot_start, tot_end;		

	if		( ev_pats.size() == 0 )
		return ERR_NOPAT;
	else if ( ev_tots.size() < 2 )
		return ERR_NOTOT;	
	else if ( ev_psdts.size() == 0 )
		return ERR_NOSDT;

	tot_start = ev_tots[0].m_time;
	tot_end = ev_tots[1].m_time;

	if ( tot_start + ev_opt_offset_beg > tot_end - ev_opt_offset_end )
	{	// TS秒数が少なすぎる場合、start/endの中間値を取る
		time_t t_dummy = (tot_start+tot_end)/2;
		tot_start = tot_end = t_dummy;
	}
	else
	{
		tot_start += ev_opt_offset_beg;
		tot_end -= ev_opt_offset_end;
	}

	clock_t clk_st = clock();

	if ( ev_opt_is_all_out )
	{
		for ( vector<CEIT*>::iterator ie = ev_peits.begin(); ie != ev_peits.end(); ++ie )
			_make_program_info( services, *ie, tot_start, tot_end );
	}
	else
	{
		for ( vector<CPAT>::iterator ip = ev_pats.begin(); ip != ev_pats.end(); ip++ )
		{
			for ( VECT_UINT16::iterator ipp = ip->prg_nos.begin(); ipp != ip->prg_nos.end(); ipp++ )
			{
				vector<CEIT*>::iterator pos;
				SServiceInfo *psvc;
				{
					CEIT	key;
					key.transport_stream_id = ip->id;
					key.id					= *ipp;
					key.original_network_id	= 0;			// transport_id,id(service_id)が等しい最初の項目を返すようにするため。CEIT::CompareTo()の実装依存なんであまりよろしくない
					pos = lower_bound(ev_peits.begin(), ev_peits.end(), &key, _compare_eit );
					if ( pos == ev_peits.end() || (*pos)->id != key.id )
					{	
						// SDT内サービスを検索してEITがないのが正しいのかを一応確認。network_idは適当にセット
						CSDT::VECT_SERV::iterator p;
						bool flg = false;
						if ( _find_sdt_svc( ev_peits.front()->original_network_id, key.transport_stream_id, key.id, p ) )
						{
							if ( !p->eit_present_following_flag && (!ev_opt_read_eit_sch || !p->eit_schedule_flag) )
								flg = true;
						}
						if ( !flg )
							DBG_LOG_ASSERT( _T("\rtrans_id:%d svc_id:%dのSDTがないので、EIT存在しないのが正しいかどうか判別できない\n"), ip->id, *ipp );
						continue;
					}
				}
				do
				{
					psvc = _make_program_info( services, *pos, tot_start, tot_end );
					pos++;
				}	
				while (pos != ev_peits.end()
					&&	(*pos)->id == psvc->service_id 
					&&  (*pos)->transport_stream_id == psvc->transport_id
					&&	(*pos)->original_network_id == psvc->network_id );
			}
		}
		// SProgramInfoがないSServiceInfoを削除する
		{
			MAP_SERV::iterator p = services.begin();
			while ( p != services.end() )
			{
				if ( p->second.programs.size() == 0 )
					p = services.erase( p );
				else
					p++;
			}
		}
	}
	//DBG_LOG_WARN( _T("make_prg:%.2f\n"), static_cast<double>(clock() - clk_st) / CLOCKS_PER_SEC );
	//----------

	clk_st = clock();
#ifdef _TEST_USE_FS_
	tofstream&	tss = ofs;
#else
	tstrstream tss;
#endif
	//----------
	tss << _T("TOT");
	if ( !ev_opt_outtime_offset )
	{
		tss << _T("\t") << _get_datetime_str(ev_tots[0].m_time);
		tss << _T("\t") << _get_datetime_str(ev_tots[1].m_time);
	}
	else
	{
		tss << _T("\t") << _get_datetime_str(tot_start);
		tss << _T("\t") << _get_datetime_str(tot_end);
	}
	tss << endl;
	if ( ev_opt_is_out_patpmt )
	{
		for ( vector<CPAT>::iterator ip = ev_pats.begin(); ip != ev_pats.end(); ip++ )
		{
			_ASSERT( ip->prg_nos.size() == ip->prg_map_pids.size() );
			for ( size_t i = 0; i < ip->prg_nos.size(); i++ )
			{
				tss << _T("PAT");
				tss << _T("\t") << ip - ev_pats.begin();
				tss << _T("\t") << ip->id;
				tss << _T("\t") << i;
				tss << _T("\t") << ip->prg_nos[i];
				tss << _T("\t") << ip->prg_map_pids[i];
				tss << endl;
			}
		}
		int i = 0;
		for ( vector<CPMT>::iterator im = ev_pmts.begin(); im != ev_pmts.end(); im++ )
		{
			for ( CPMT::VECT_STRM::iterator is = im->stream_tbl.begin(); is != im->stream_tbl.end(); is++ )
			{
				tss << _T("PMT1");
				tss << _T("\t") << i;
				tss << _T("\t") << im->m_self_pid;
				tss << _T("\t") << im->id;
				tss << _T("\t") << is - im->stream_tbl.begin();
				tss << _T("\t") << is->stream_type;
				tss << _T("\t") << is->elementary_pid;
				tss << endl;
			}
			tss << _T("PMT2");
			tss << _T("\t") << i;
			tss << _T("\t") << im->m_self_pid;
			tss << _T("\t") << im->id;
			tss << _T("\t") << im->m_file_pos;
			tss << _T("\t") << im->pcr_pid;
			tss << endl;
			i++;
		}
	}

	for ( MAP_SERV::iterator im = services.begin(); im != services.end(); im++ )
	{
		SServiceInfo &ssi = im->second;
		tss << _T("SVC");
		tss << _T("\t") << ssi.network_id;
		tss << _T("\t") << ssi.transport_id;
		tss << _T("\t") << ssi.service_id;
		switch ( ssi.network_id )
		{
		case 4:		// BSデジタル
			tss << _T("\tBS");	break;
		case 1:		// PerfecTV!	
		case 3:		// SKY
		case 6:		// スカパー e2(CS1)
		case 7:		// スカパー e2(CS2)
		case 10:	// スカパー HD
			tss << _T("\tCS");	break;
		default:
			if ( 0x7880 <= ssi.network_id && ssi.network_id <= 0x7FE8 )
				tss << _T("\t地デ");
			else
				tss << _T("\tその他");
			break;
		}
		tss << _T("\t") << ssi.name.c_str();
		tss << _T("\t") << ssi.prov_name.c_str();
		tss << _T("\t") << ssi.service_type;
		tss << endl;

		// 開始時間でソートする
		vector<MAP_PRG::const_iterator>	sort_prgmap;
		for ( MAP_PRG::const_iterator ip = ssi.programs.begin(); ip != ssi.programs.end(); ip++ )
			sort_prgmap.push_back( ip );
		sort( sort_prgmap.begin(), sort_prgmap.end(), _compare_prg );

		//for ( MAP_PRG::iterator ip = ssi.programs.begin(); ip != ssi.programs.end(); ip++ )
		for ( vector<MAP_PRG::const_iterator>::iterator it = sort_prgmap.begin(); it != sort_prgmap.end(); ++it )
		{
			//SProgramInfo &spi = ip->second;
			const SProgramInfo &spi = (*it)->second;

			tss << _T("EVT");
			tss << _T("\t") << ssi.service_id;
			tss << _T("\t") << spi.event_id;
			tss << _T("\t") << _get_datetime_str(spi.start_time);
			tss << _T("\t") << _get_datetime_str(spi.end_time);
			tss << _T("\t");
			tss.write( spi.name_str.c_str(), spi.name_str.length() );
			tss << _T("\t") << spi.genre;
			tss << _T("\t") << _get_genre_lvl1_str(spi.genre);
			tss << _T("\t") << _get_genre_lvl2_str(spi.genre);
			if ( ev_opt_evt_output_type >= 1 )
			{
				if ( spi.stream_content == 1 || spi.stream_content == 5 )
				{
					switch ( (spi.component_type >> 4) & 0xf )
					{
					case 0x0:	tss << _T("\t480i");	break;
					case 0x9:	tss << _T("\t2160p");	break;
					case 0xA:	tss << _T("\t480p");	break;
					case 0xB:	tss << _T("\t1080i");	break;
					case 0xC:	tss << _T("\t720p");	break;
					case 0xD:	tss << _T("\t240p");	break;
					case 0xE:	tss << _T("\t1080p");	break;
					case 0xF:	tss << _T("\t180p");	break;
					default:	tss << _T("\t????");	break;
					}
				}
				else if ( spi.stream_content == -1 )
					tss << _T("\t非指定");
				else
					tss << _T("\t非映像");

				tss << _T("\t") << ((spi.stream_content<<8)|spi.component_type);
			}
			if ( ev_opt_evt_output_type >= 2 )
			{
				tss << _T("\t");
				tss.write( spi.desc_str.c_str(), spi.desc_str.length() );
			}
			tss << endl;
			if ( ev_opt_is_out_extevt )
			{
				int i = 0;
				for ( CNTR_EXTEV::const_iterator ie = spi.ext_evts.begin(); ie != spi.ext_evts.end(); ++ie )
				{
					const tstring		&name = (*ie).first;
					const VECT_UINT8	&data = (*ie).second;
					if ( name.length() > 0 || data.size() > 0 )
					{
						tstring		text;
						AribToString( &text, &data[0], data.size() );

						tss << _T("XEVT");
						tss << _T("\t") << ssi.service_id;
						tss << _T("\t") << spi.event_id;
						tss << _T("\t") << i;
						tss << _T("\t") << name.c_str();
						tss << _T("\t") << text.c_str();
						tss << endl;

						i++;
					}
				}
			}
		}
	}
#ifndef _TEST_USE_FS_
	{
		ofs << tss.rdbuf();
	}
#endif
	//DBG_LOG_WARN(_T("write_prg:%.2f\n"), static_cast<double>(clock() - clk_st) / CLOCKS_PER_SEC );

	return 0;
}

static void _dbg_out(tofstream &ofs)
{
#ifndef _TEST_USE_FS_
	tstrstream ou;
#else
	tofstream &ou = ofs;
#endif
	
	ou << _T("<<PAT>>") << endl;
	for ( vector<CPAT>::iterator p = ev_pats.begin(); p != ev_pats.end(); p++ )
	{
		ou << _T("trans_id:") << p->id << _T(" version:") << p->version << _T(" section:") << p->section << _T(" last_section:") << p->last_section << endl;
		for ( size_t i = 0; i < p->prg_nos.size(); i++ )
		{
			ou << _T("  [") << i << _T("] prg_no:") << (int)p->prg_nos[i] << _T(" pmt_pid:") << (int)p->prg_map_pids[i] << endl;
		}
		ou << endl;
	}

	ou << endl << _T("<<PMT>>") << endl;
	for ( vector<CPMT>::iterator p = ev_pmts.begin(); p != ev_pmts.end(); p++ )
	{
		ou << _T("service_id:") << p->id << _T(" pcr_pid:") << p->pcr_pid << _T(" version:") << p->version << endl;
		ou << _T("desc_tag:");
		for ( VECT_DESC::iterator d = p->descriptors.begin(); d != p->descriptors.end(); d++ )
			ou << hex << showbase << d->tag << ",";
		ou << dec << noshowbase << endl;
		int i = 0;
		for ( vector<CPMT::CStream>::iterator s = p->stream_tbl.begin(); s != p->stream_tbl.end(); s++, i++ )
		{
			ou << _T("    [") << i << _T("] stream_type:") << s->stream_type << _T(" elementary_pid:") << s->elementary_pid << endl;
			ou << _T("    desc_tag:");
			for ( VECT_DESC::iterator d = s->descriptors.begin(); d != s->descriptors.end(); d++ )
				ou << hex << showbase << d->tag << ",";
			ou << dec << noshowbase << endl;
		}
	}

	ou << endl << _T("<<SDT>>") << endl;
	for ( vector<CSDT*>::iterator sp = ev_psdts.begin(); sp != ev_psdts.end(); ++sp )
	{
		CSDT *p = *sp;

		ou << _T("trans_id:") << p->id << _T(" network_id:") << p->original_network_id;
		ou << _T(" tbl_id:") << hex << showbase << p->table_id;
		ou << _T(" version:") << dec << noshowbase << p->version << endl;
		int i = 0;
		for ( CSDT::VECT_SERV::iterator pp = p->services.begin(); pp != p->services.end(); ++pp, ++i )
		{
			ou << _T("    [") << i << _T("] service_id:") << pp->service_id << _T(" eit_sch_flg:") << pp->eit_schedule_flag;
			ou << _T(" eit_pres_flg:") << pp->eit_present_following_flag << endl;
			ou << _T("    desc_tag:");
			for ( VECT_DESC::iterator d = pp->descriptors.begin(); d != pp->descriptors.end(); d++ )
				ou << hex << showbase << d->tag << ",";
			ou << dec << noshowbase << endl;
		}
	}

	ou << endl << _T("<<TOT>>") << endl;
	for ( vector<CTOT>::iterator p = ev_tots.begin(); p != ev_tots.end(); p++ )
	{
		TCHAR	szbuf[64];
		tm		t;
		_convert_time_jst40( p->jst_time, t );
		_tcsftime( szbuf, 64, _T("%y/%m/%d %H:%M:%S"), &t );
		ou << _T("time:") << szbuf << endl;
	}

	ou << endl << _T("<<EIT>>") << endl;
	for ( vector<CEIT*>::iterator ep = ev_peits.begin(); ep != ev_peits.end(); ep++ )
	{
		CEIT *p = *ep;

		ou << _T("trans_id:") << p->transport_stream_id << _T(" service_id:") << p->id << _T(" network_id:") << p->original_network_id;
		ou << _T(" tbl_id:") << hex << showbase << p->table_id;
		ou << _T(" version:") << dec << noshowbase << p->version << endl;
		ou << _T("sections:");
		for ( set<UINT8>::const_iterator pp = p->GetProcSections().begin(); pp != p->GetProcSections().end(); pp++ )
		{
			ou << *pp << _T(",");
		}
		ou << endl;
		int i = 0;
		for ( vector<CEIT::CEvent>::iterator e = p->event_tbl.begin(); e != p->event_tbl.end(); e++, i++ )
		{
			tm		t;
			TCHAR	szbuf[64];
			_convert_time_jst40( e->start_time, t );
			_tcsftime( szbuf, 64, _T("%y/%m/%d %H:%M"), &t );
			ou << _T("  [") << i << _T("] evt_id:") << e->event_id << _T(" start_time:") << szbuf;
			_convert_time_bcd( e->duration, t );
			ou << _T(" duration:") << setfill(_T('0')) << setw(2) << t.tm_hour << _T(":") << setw(2) << t.tm_min << _T(":") << setw(2) << t.tm_sec;
			ou << setfill(_T(' ')) << endl;
			ou << _T("    desc_tag:");
			for ( VECT_DESC::iterator d = e->descriptors.begin(); d != e->descriptors.end(); d++ )
				ou << hex << showbase << d->tag << ",";
			ou << dec << noshowbase << endl;
		}
		ou << endl;
	}
#ifndef _TEST_USE_FS_
	ofs << ou.rdbuf();
#endif
}

// デバッグログ出力
// この関数は処理系依存上等で作る(^_^;
void dbg_log_out(int rpt_type, TCHAR *filename, int line, TCHAR *lpsz_msg, ... )
{
	TCHAR	szmsg[4096];
	TCHAR	sfname[256], sext[256];
	va_list	va_arg;
	
	_tsplitpath( filename, NULL, NULL, sfname, sext );

	va_start( va_arg, lpsz_msg );
	_vsntprintf( szmsg, 4096, lpsz_msg, va_arg);		// vsnprintf
	va_end( va_arg );
#ifdef _UNICODE
	_RPTW4( (rpt_type==_CRT_ASSERT?_CRT_WARN:rpt_type), _T("%s%s(%d) %s"), sfname, sext, line, szmsg );
#else
	_RPT4( (rpt_type==_CRT_ASSERT?_CRT_WARN:rpt_type), _T("%s%s(%d) %s"), sfname, sext, line, szmsg );
#endif
	if ( ev_ps_dbglog )
	{
		if ( !ev_dbglog_flg )
		{	// この実行で初めての場合、処理ファイル名をデバッグログに記述
			*ev_ps_dbglog << _T("\r") << ev_opt_input_fname << endl;
			ev_dbglog_flg = true;
		}
		switch ( rpt_type )
		{
		case _CRT_WARN:		*ev_ps_dbglog << _T("<WARN>");	break;
		case _CRT_ERROR:	*ev_ps_dbglog << _T("<ERR >");	break;
		case _CRT_ASSERT:	*ev_ps_dbglog << _T("<ASSR>");	break;
		}
		*ev_ps_dbglog << sfname << sext << _T("(") << line << _T(") ");
		*ev_ps_dbglog << szmsg;
	}
}

// 引数解析と表示
// 返り値:0		エラーなし
static int _parse_option(int argc, _TCHAR* argv[])
{
	// 引数解析
	int			ch;
	TCHAR		*p_end;

	while ((ch = getopt(argc, argv, _T("o:r:d:l:sVnB:E:e:Txap"))) != -1)
	{
		switch ( ch )
		{
		case 'o':		// offset read
			ev_opt_offset_read = _tcstol( optarg, &p_end, 0 );
			if ( *p_end == '%' )
				ev_opt_offset_read_is_ratio = true;
			else if ( !p_end || !(*p_end) )
			{
				tcerr << _T("oオプション引数を数値に変換できません：") << optarg << endl;
				return ERR_PARAM;
			}
			else if ( ev_opt_offset_read < 0 )
			{
				tcerr << _T("oオプション引数にマイナスの値は指定できません：") << optarg << endl;
				return ERR_PARAM;
			}
			break;
		case 'r':		// max read size指定				 
			ev_opt_max_read_size = _tcstol( optarg, &p_end, 0 );
			switch (*p_end)
			{
			case _T('%'):	ev_opt_max_read_type = RS_RATIO;	break;
			case _T('s'):
			case _T('S'):	if(ev_opt_max_read_size) ev_opt_max_read_type = RS_SEC;		break;
			case 0:			
				if ( ev_opt_max_read_size < 0 )
				{
					tcerr << _T("rオプション引数にマイナスの値は指定できません：") << optarg << endl;
					return ERR_PARAM;
				}
				break;
			default:
				tcerr << _T("rオプション引数を数値に変換できません：") << optarg << endl;
				return ERR_PARAM;
			}
			break;
		case 'd':			// デバッグダンプ
			ev_opt_dbgout_fname = optarg;
			break;
		case 'l':			// デバッグログ
			ev_opt_dbglog_fname = optarg;
			break;
		case 'V':			// 詳細出力
			ev_opt_verbose = true;
			break;
		case 'n':
			ev_opt_no_progress = true;
			break;
		case 's':			// スケジュールEITを読み込む
			ev_opt_read_eit_sch = true;
			break;
		case 'T':			// TOT出力時オフセット値の影響を受ける
			ev_opt_outtime_offset = true;
			break;
		case 'B':			// 開始オフセット秒数
			//ev_opt_offset_beg = _ttoi(optarg);
			ev_opt_offset_beg = _tcstol( optarg, &p_end, 0 );
			if ( !p_end || !(*p_end) )
			{
				tcerr << _T("Bオプション引数を数値に変換できません：") << optarg << endl;
				return ERR_PARAM;
			}
			else if ( ev_opt_offset_beg < 0 )
			{
				tcerr << _T("Bオプション引数にマイナスの値は指定できません：") << optarg << endl;
				return ERR_PARAM;
			}
			break;
		case 'E':			// 終了オフセット秒数
			//ev_opt_offset_end = _ttoi(optarg);
			ev_opt_offset_end = _tcstol( optarg, &p_end, 0 );
			if ( !p_end || !(*p_end) )
			{
				tcerr << _T("Eオプション引数を数値に変換できません：") << optarg << endl;
				return ERR_PARAM;
			}
			else if ( ev_opt_offset_end < 0 )
			{
				tcerr << _T("Bオプション引数にマイナスの値は指定できません：") << optarg << endl;
				return ERR_PARAM;
			}
			break;
		case 'e':			// EVT出力時の出力形式
			ev_opt_evt_output_type = _ttoi(optarg);
			break;
		case 'x':			// 拡張イベント情報出力
			ev_opt_is_out_extevt = true;
			break;
		case 'p':			// PAT/PMT出力
			ev_opt_is_out_patpmt = true;
			break;
		case 'a':			// 全サービス/イベント出力
			ev_opt_is_all_out = true;
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if ( argc > 0 )
		ev_opt_input_fname  = argv[0];
	if ( argc > 1 )
		ev_opt_output_fname = argv[1];
	if ( !ev_opt_input_fname || (!ev_opt_output_fname && !ev_opt_dbgout_fname) )
	{
		tcerr << _T("eit_txtout.exe [option] input_file output_file") << endl;
		tcerr << _T("[option]はeit_txtout.txtを参照下さい") << endl;
		return ERR_PARAM;
	}

	if ( ev_opt_verbose )
	{	// 詳細表示。標準出力に出力
		tcout << _T("入力ファイル               :") << ev_opt_input_fname << endl;
		tcout << _T("出力ファイル               :") << (ev_opt_output_fname ? ev_opt_output_fname : _T("")) << endl;
		tcout << _T("デバッグダンプ(-d)         :") << (ev_opt_dbgout_fname ? ev_opt_dbgout_fname : _T("なし")) << endl;
		tcout << _T("デバッグログ(-l)           :") << (ev_opt_dbglog_fname ? ev_opt_dbglog_fname : _T("指定なし")) << endl;
		tcout << _T("進捗非表示                 :") << ev_opt_no_progress << endl;
		tcout << _T("読み込みオフセット(-o)     :") << ev_opt_offset_read << (ev_opt_offset_read_is_ratio ? _T("%") : _T("*1000*188")) << endl;
		tcout << _T("読み込みサイズ(-r)         :") << ev_opt_max_read_size;
		switch ( ev_opt_max_read_type )
		{
		case RS_SIZE:	tcout << _T("*1000*188 byte") << endl;	break;
		case RS_RATIO:	tcout << _T("%") << endl;	break;
		case RS_SEC:	tcout << _T("sec") << endl;	break;
		}
		tcout << _T("スケジュールEIT読込(-s)    :") << ev_opt_read_eit_sch << endl;
		tcout << _T("開始オフセット秒数(-B)     :") << ev_opt_offset_beg << endl;
		tcout << _T("終了オフセット秒数(-E)     :") << ev_opt_offset_end << endl;
		tcout << _T("EVT～出力形式(-e)          :") << ev_opt_evt_output_type << endl;
		tcout << _T("PATとPMTを出力(-p)         :") << ev_opt_is_out_patpmt << endl;
		tcout << _T("拡張形式イベント出力(-x)   :") << ev_opt_is_out_extevt << endl;
		tcout << _T("全サービス/イベント出力(-a):") << ev_opt_is_all_out << endl;
	}
	return 0;
}

//#define _MY_FILE_BUFFERING_
int _tmain(int argc, _TCHAR* argv[])
{
#ifdef _MY_FILE_BUFFERING_
	// 循環ダブルバッファ(？)として扱う。速度的には変わらん(ToT)
	const int	c_ringbuf_bitsize = 20;				// 
	const int	c_ringbuf_size = 1 << c_ringbuf_bitsize;
	const int	c_ringbuf_mask = c_ringbuf_size-1;
	UINT8		*buf;								// 現在のポインタ
	int			ring_idx;
	UINT8		*ring_mybuf;						// 
	UINT64		total_rdsize = 0;
	UINT8		tempbuf[TS_PACKET_SIZE];			// リングバッファ終端を超える場合に使用
#else
	UINT8		buf[TS_PACKET_SIZE];				// ファイルバッファ
#endif
	UINT64		read_btoffset=0;					// 読込オフセットバイト
	UINT64		read_btsize;						// 読込バイトサイズ(バイト単位)
	UINT64		count = 0, max_read_count = 0;		// 読込回数、最大読込回数(0の時全部) (両方パケット単位)
	tofstream	fs_dbglog;
	FILE		*file;
	int			ret = 0;
	time_t		start_time;
	// 引数オプション

#if defined(_DEBUG) && defined(_MSC_VER)
	// 解放し忘れをチェック
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
//	_CrtSetBreakAlloc( 48068 );
#endif

	// ロケール設定
	_tsetlocale( LC_ALL, _T("") );
	std::locale loc = std::locale();
	loc = std::locale(loc, STR_STL_LOCALE, std::locale::ctype);	// 日本語。3桁区切りなし
	std::locale::global(loc);

	ret = _parse_option(argc, argv );
	if ( ret )
		return ret;
	// 
	if ( ev_opt_dbglog_fname )
	{
		fs_dbglog.open(ev_opt_dbglog_fname, ios_base::out|ios_base::app);		// 追加モードで開く
		if ( !fs_dbglog.is_open() )
			ev_ps_dbglog = &tclog;
		else
		{
			ev_ps_dbglog = &fs_dbglog;
			//fs_dbglog.imbue( loc );
		}
	}
	else if ( ev_opt_verbose )
	{
		ev_ps_dbglog = &tclog;
	}
	if ( ev_ps_dbglog && ev_opt_verbose )
	{
	}

	// "S"はシーケンシャルアクセスに最適化。
	// これを入れると入れないでは速度が違う(20sec位？)
	file = _tfopen( ev_opt_input_fname, _T("rbS") );
	if ( !file )
	{
		tcerr << _T("入力ファイルが開けません:") << ev_opt_input_fname << endl;
		return ERR_INPUTFILE;
	}
	// 最後まで処理して開けません。では悲しいのでここで開けるかどうかチェックする
	if ( ev_opt_output_fname )
	{
		tofstream	fs_out;
		fs_out.open( ev_opt_output_fname, ios::out );
		if ( !fs_out )
		{
			tcerr << _T("出力ファイルが開けません:") << ev_opt_output_fname << endl;
			return ERR_OUTPUTFILE;
		}
		fs_out.close();
	}
	// 初期化
	// 全部NULLにしとく
	memset( ev_pSectionBuf, 0, sizeof(ev_pSectionBuf) );
	memset( ev_BefSectionFilePos, 0, sizeof(ev_BefSectionFilePos) );

	// ファイルサイズ(読込バイトサイズ)を取得
	{
		INT64				beg_pos, end_pos;
		UINT64				file_btsize;
		fseek( file, 0, SEEK_END);		//ファイル末尾へ移動
		end_pos = _ftelli64(file);
		//fs.clear();
		fseek( file, 0, SEEK_SET);		//ファイル先頭へ移動
		beg_pos = _ftelli64(file);

		file_btsize = read_btsize = end_pos - beg_pos;

		if ( ev_opt_offset_read )
		{
			if ( ev_opt_offset_read_is_ratio )
			{
				read_btoffset = file_btsize * ev_opt_offset_read / 100;
				// パケットで割り切れる数にする
				read_btoffset = read_btoffset / TS_PACKET_SIZE * TS_PACKET_SIZE;
			}
			else
				read_btoffset = ev_opt_offset_read * TS_PACKET_SIZE * 1000;
			if ( ev_opt_verbose )
				tcout << _T("読込オフセットバイト       :") << read_btoffset << endl;

			INT64	offset = read_btoffset;
			const int c_32bit_offset = 0x70000000;
			_fseeki64( file, offset, SEEK_CUR );
		}

		if ( ev_opt_max_read_size )
		{
			switch ( ev_opt_max_read_type )
			{
			case RS_RATIO:
				max_read_count = ev_opt_max_read_size * file_btsize / 100;
				// パケット単位にする
				max_read_count /= TS_PACKET_SIZE;
				break;
			case RS_SIZE:
				max_read_count = ev_opt_max_read_size * 1000;
				break;
			}
		}
		read_btsize = file_btsize - read_btoffset;
		if ( max_read_count > 0 && read_btsize > max_read_count*TS_PACKET_SIZE )
			read_btsize = max_read_count*TS_PACKET_SIZE;		
		if ( ev_opt_verbose && ev_opt_max_read_type != RS_SEC )
			tcout << _T("読込バイトサイズ           :") << read_btsize << endl;
	}

	_ASSERT( read_btoffset % TS_PACKET_SIZE == 0 );

	start_time = time( &start_time );
#ifdef _MY_FILE_BUFFERING_
	// 
	ring_mybuf = new UINT8[c_ringbuf_size];
	fread( ring_mybuf, 1, c_ringbuf_size, file );
	ring_idx = 0;
	total_rdsize = 0;
#endif
	do
	{
		bool dump_flg = false;
		
#ifdef _MY_FILE_BUFFERING_
		if ( ring_mybuf[ring_idx] != 0x47 )
		{	// 基本的にここに入ってくることはないはず(転送エラーとかあったら入ってくるのかな？)
			// 0x47読むまで読み進める
			do
			{
				total_rdsize++;
				ring_idx++;
				ring_idx &= c_ringbuf_mask;
			}
			while ( total_rdsize < read_btsize && ring_mybuf[ring_idx] != 0x47 );
			if ( total_rdsize < read_btsize )
			{	// 1文字戻す
				total_rdsize--;
				ring_idx--;
				ring_idx &= c_ringbuf_mask;
			}
			else
				break;
		}
#else
		int in_char;
		if ( (in_char = fgetc(file)) != 0x47 )
		{	// 基本的にここに入ってくることはないはず(転送エラーとかあったら入ってくるのかな？)
			// 0x47読むまで読み進める
			while ( !feof(file) && (in_char = fgetc(file)) != 0x47 );
			// 1文字戻す
			if ( !feof(file) )
				ungetc( in_char, file );
			else
				break;
		}
		ungetc( in_char, file );
#endif
		count++;
		ev_cur_filepos = read_btoffset / TS_PACKET_SIZE + count;
		// 
#ifdef _MY_FILE_BUFFERING_
		if ( ring_idx + TS_PACKET_SIZE > c_ringbuf_size )
		{	
			// リングバッファ終端を超える場合は一時バッファにコピーして使用する
			// 別に毎回コピーしても良いが、遅いような気がするので
			UINT8 *pdst = tempbuf;
			UINT8 *psrc = &ring_mybuf[ring_idx];
			int i;
			for ( i = 0; i < c_ringbuf_size -ring_idx; i++ )
				*pdst++ = *psrc++;
			_ASSERT( psrc == &ring_mybuf[c_ringbuf_size] );
			psrc = &ring_mybuf[0];
			for ( ; i < TS_PACKET_SIZE; i++ )
				*pdst++ = *psrc++;
			buf = tempbuf;
		}
		else
			buf = &ring_mybuf[ring_idx];
#else
		fread( (char*)&buf[0], 1, TS_PACKET_SIZE, file );
#endif
		int pid = ((buf[1] & 0x1F) << 8) | buf[2];
		switch ( pid )
		{
		case PID_PAT:
		case PID_EIT_0:
		case PID_EIT_1:
		case PID_EIT_2:
		case PID_TOT:
		case PID_SIT:
		case PID_SDT:
			dump_flg = true;
			break;
		default:
			// PMTを検索する
			if ( ev_opt_is_out_patpmt )
			{	
				// PAT/PMT出力時しかPMT配列は使用しないので、高速化のため指定条件下でのみ検索
				for ( vector<CPAT>::iterator it = ev_pats.begin(); it != ev_pats.end(); ++it )
				{
					if ( find( it->prg_map_pids.begin(), it->prg_map_pids.end(), pid ) != it->prg_map_pids.end() )
					{
						dump_flg = true;
						break;
					}
				}
			}
			break;
		}
		if ( dump_flg )
		{
			bool no_err = true;
			CPacket packet( buf, false );

			CSectionBuf *p_sec = ev_pSectionBuf[packet.header.pid];
			if	(	packet.header.sync != 0x47										// 同期バイト不正
				||	packet.header.trans_error										// ビット誤りあり
				||	packet.header.pid >= 0x0002 && packet.header.pid <= 0x000F		// 未定義PID範囲
				||	packet.header.scambling == 0x01									// 未定義スクランブル制御値
				||	(!packet.header.has_adaptation && !packet.header.has_payload)	// 未定義アダプテーションフィールド制御値
				)
			{
				no_err = false;
				DBG_LOG_WARN( _T("\rpacket headerが不正 pos:%d\n"), ev_cur_filepos );
			}
			else if ( packet.header.has_payload )
			{
				if ( packet.header.payload_start )
				{
					if ( packet.p_start_payload[0] == 0 && !p_sec )
					{
						p_sec = ev_pSectionBuf[packet.header.pid] = new CSectionBuf();
						no_err = p_sec->AddData( packet.header.index, packet.p_start_payload, packet.p_end_payload );
						
					}
					else if ( p_sec )
					{
						no_err = p_sec->AddData( packet.header.index, packet.p_start_payload+1, packet.p_end_payload );
					}
				}
				else if ( p_sec )
					no_err = p_sec->AddData( packet.header.index, packet.p_start_payload, packet.p_end_payload );
				if ( !no_err )
					DBG_LOG_ASSERT( _T("\rセクションバッファ設定でエラー pos:%d\n"), ev_cur_filepos );
				if ( !p_sec )
				{
					if ( ev_BefSectionFilePos[packet.header.pid] )
					{
						DBG_LOG_ASSERT( _T("\r該当するセクションデータがない。pid:0x%X bef_pos:%d\n"), 
							packet.header.pid, ev_BefSectionFilePos[packet.header.pid] );
					}
				}
			}
			if ( p_sec )
			{
				if ( !no_err )
				{	// 現在のセクションデータは捨てる
					delete p_sec;
					p_sec = ev_pSectionBuf[packet.header.pid] = NULL;
				}
				else
				{
					eSECTION_RET ret;
					int				proc_cnt = 0;			// 処理回数。デバッグ用で特に深い意味無し
					while ( (ret =  _dump_section_data( packet, p_sec, ev_opt_read_eit_sch )) == SEC_OK_RETRY )
						proc_cnt++;
					if ( ret == SEC_OK_END || p_sec->GetData().size() >= 4196 )
					{
						// 厳密に言うとテーブル識別毎に最大バイト数1024/4196が異なるが、そこまで考えない
						if ( ret != SEC_OK_END && p_sec->GetData().size() >= 4196 )
						{
							DBG_LOG_ASSERT( _T("\rセクションデータサイズが4196を超えてます。pid:0x%X bef_pos:%d\n"),
								packet.header.pid, ev_BefSectionFilePos[packet.header.pid] );
						}
						// 今までのパケットで処理できる場合、もう不要なので捨てる
						delete p_sec;
						p_sec = ev_pSectionBuf[packet.header.pid] = NULL;
						ev_BefSectionFilePos[packet.header.pid] = ev_cur_filepos;
					}
				}
			}
		}
		if ( !ev_opt_no_progress && (count & ((1<<12)-1)) == 1 )			// (count % (1<<12)) == 1
		{
			time_t cur, rest;
			UINT64 percent;

			if ( ev_opt_max_read_type == RS_SEC )
			{
				if ( ev_tots.size() < 2 )
					percent = 0;
				else
					percent = (ev_tots[1].m_time - ev_tots[0].m_time) * 10000 / ev_opt_max_read_size;
			}
			else
			{
#ifdef _MY_FILE_BUFFERING_
				percent = total_rdsize * 10000 / read_btsize;					// 高速化のため小数第2位まで求める(doubleとかだと遅いと思う)
#else
				UINT64 pos = _ftelli64( file );;
				_ASSERT( pos >= read_btoffset );
				percent = (pos-read_btoffset) * 10000 / read_btsize;			// 高速化のため小数第2位まで求める(doubleとかだと遅いと思う)
#endif
			}
			tcout << _T("\r") << setw(3) << setfill(_T(' ')) << percent / 100;
			tcout << _T(".") << setw(2) << setfill(_T('0')) << percent % 100 << _T("%");

			cur = time( &cur );
			tcout << _T(" 経過:") << setw(4) << setfill(_T(' ')) << cur - start_time << _T("s");
			if ( percent )
			{
				rest = (cur - start_time) * (10000 - percent) / percent;
				tcout << _T(" 残り:") << setw(4) << rest << _T("s");
			}
		}
#ifdef _MY_FILE_BUFFERING_
		{
			int bef_blk_idx = ring_idx >> (c_ringbuf_bitsize-1);		// 前半:0 後半:1
			total_rdsize += TS_PACKET_SIZE;
			ring_idx += TS_PACKET_SIZE;
			ring_idx &= c_ringbuf_mask;
			int cur_blk_idx = ring_idx >> (c_ringbuf_bitsize-1);		// 前半:0 後半:1
			if ( bef_blk_idx != cur_blk_idx )
			{	// もう使用しない方のブロックをファイルから読み込み
				int idx = bef_blk_idx<<(c_ringbuf_bitsize-1);
				fread( &ring_mybuf[idx], 1, 1<<(c_ringbuf_bitsize-1), file );
			}
		}

	}while( total_rdsize < read_btsize
#else
	}while( !feof(file) 
#endif
		&& (!max_read_count || !(ev_opt_max_read_type != RS_SEC && count >= max_read_count))
		&& !(ev_opt_max_read_type == RS_SEC && ev_tots.size() >= 2 && ev_tots[1].m_time - ev_tots[0].m_time >= ev_opt_max_read_size )
	);

	fclose( file );
#ifdef _MY_FILE_BUFFERING_
	delete[] ring_mybuf;
#endif

	// 情報の構成
	if ( ev_opt_output_fname )
	{
		tofstream	ofs;
		//ofs.imbue( loc );
		ofs.open( ev_opt_output_fname, ios_base::out|ios_base::trunc );
		if ( !ofs )
		{
			tcerr << _T("出力ファイルが開けません:") << ev_opt_output_fname << endl;
			ret = ERR_OUTPUTFILE;
		}
		else
		{
			ret = _output_program_info(ofs);
			//ret = _output_program_info(ev_opt_output_fname, ev_opt_outtime_offset, ev_opt_offset_beg, ev_opt_offset_end, ev_opt_evt_output_type);
		}
	}
	
	if ( ev_opt_dbgout_fname )
	{
		tofstream	fs_dbgout;
		fs_dbgout.open(ev_opt_dbgout_fname, ios_base::out|ios_base::trunc);
		if ( fs_dbgout.is_open() )
		{
			//fs_dbgout.imbue( loc );			// これしないと日本語文字が出力されないみたい。
			fs_dbgout << _T("filename:") << ev_opt_input_fname << endl;
			_dbg_out(fs_dbgout);
		}
		fs_dbgout.close();
	}

	if ( fs_dbglog.is_open() )
		fs_dbglog.close();

	// 終了化
	for ( int i = 0; i < sizeof(ev_pSectionBuf)/sizeof(ev_pSectionBuf[0]); i++ )
	{
		if ( ev_pSectionBuf[i] )
		{
			delete ev_pSectionBuf[i];
			ev_pSectionBuf[i] = NULL;	// 何となくNULLで埋めとく
		}
	}
	for ( vector<CEIT*>::iterator ie = ev_peits.begin(); ie != ev_peits.end(); ++ie )
		delete *ie;
	for ( vector<CSDT*>::iterator is = ev_psdts.begin(); is != ev_psdts.end(); ++is )
		delete *is;
	{
		time_t cur = time( &cur );
		if ( cur - start_time > 10 )
			tcout << endl << _T(" 経過:") << setw(4) << setfill(_T(' ')) << cur - start_time << _T("s");
	}
	return ret;
}

