#ifndef _INC_TSDATA_H_
#define _INC_TSDATA_H_

#include "type_define.h"

// セクションデータ(ペイロード)バッファリング用
class CSectionBuf
{
public:
	CSectionBuf(){next_index = add_count = 0;}
private:
	VECT_UINT8	data;
	int			next_index;		// 次のインデックス
	int			add_count;
public:
	int					GetNextIndex(){return next_index;}
	const VECT_UINT8&	GetData(){return data;}
public:
	//bool AddData(int data_index, VECT_UINT8::iterator beg_pos, VECT_UINT8::iterator end_pos)
	bool AddData(int data_index, UINT8 *beg_pos, UINT8 *end_pos)
	{
		if ( data_index == next_index || data.empty() )
		{
			// VC2010では通るが、本来はこういう書き方は駄目かも。
			std::copy( beg_pos, end_pos, std::back_inserter( data ) );
			next_index = data_index + 1;
			next_index &= 0xF;
			add_count++;
			return true;
		}
		else
		{
			return false;
		}
	}
	void ClearData()
	{
		next_index = 0;
		add_count = 0;
		data.clear();
	}
};

// キー：項目名  値：バイト配列
// (ここでエンコードすると内容が複数記述子にまたがってた場合にまずい)
typedef std::map<tstring, VECT_UINT8> CNTR_EXTEV;

// 番組情報
struct SProgramInfo
{
	int			event_id;			// イベントID
	int			flg;				// 情報設定フラグ
	tstring		name_str;			// 番組名
	tstring		desc_str;			// 内容
	time_t		start_time;			// 開始時間
	time_t		end_time;			// 終了時間
	int			genre;				// ジャンル。初期値-1
	int			stream_content;		// コンポーネント内容
	int			component_type;		// 
	int			ext_evt_flg;
	CNTR_EXTEV	ext_evts;	
};

typedef std::map<int,SProgramInfo>	MAP_PRG;

struct SServiceInfo
{
	int			network_id;
	int			transport_id;
	int			service_id;
	int			service_type;
	tstring		name;
	tstring		prov_name;
	MAP_PRG		programs;			// 番組情報。キーはイベントIDとする
};

typedef std::map<UINT64,SServiceInfo>	MAP_SERV;

#define PID_PAT		0x000			// PAT
#define PID_EIT_0	0x012			// EIT
#define PID_EIT_1	0x026			// EIT
#define PID_EIT_2	0x027			// EIT
#define PID_TOT		0x014			// TOT/TDT
#define PID_SIT		0x01F
//#define PID_NIT		0x010			// NIT
#define PID_SDT		0x011			// SDT

#define TS_PACKET_SIZE		188

#endif	// #ifndef