#ifndef _INC_TS_PACKET_H_
#define _INC_TS_PACKET_H_

// TSパケットのヘッダ
class CHeader
{
public:
	CHeader(){}
	CHeader(UINT8 val[])
	{
		SetData(val);
	}
public:
	int	sync;
	int trans_error;
	int payload_start;
	int element_priority;
	int pid;
	int scambling;
	int has_adaptation;
	int has_payload;
	int index;
public:
	int SetData(UINT8 val[])
	{
        sync = val[0];
        trans_error = (val[1] & 0x80) >> 7;
        payload_start = (val[1] & 0x40) >> 6;
        element_priority = (val[1] & 0x20) >> 5;
        pid = ((val[1] & 0x1F) << 8) | val[2];
        scambling = (val[3] & 0xC0) >> 6;
        has_adaptation = (val[3] & 0x20) >> 5;
        has_payload = (val[3] & 0x10) >> 4;
        index = (val[3] & 0x0F);

		return 4;
	}
};

// TSパケットのアダプテーションヘッダ
class CAdaptation
{
public:
	int length;
	int discontinuity;
	int random_access;
	int priority;
	int has_pcr;
	int has_opcr;
	int splicing_point;
	int transport_private;
	int extension;
public:
	// 返り値 Adaptationフィールドのバイト数
	int SetData(UINT8 buf[])
	{
        length = buf[0];
        int val = buf[1];
        discontinuity = (val & 0x80) >> 7;
        random_access = (val & 0x40) >> 6;
        priority = (val & 0x20) >> 5;
        has_pcr = (val & 0x10) >> 4;
        has_opcr = (val & 0x08) >> 3;
        splicing_point = (val & 0x04) >> 2;
        transport_private = (val & 0x02) >> 1;
        extension = val & 0x01;

		return length;
	}
};

// TSパケットのデータ
class CPacket
{
public:
	CPacket(){}
	CPacket(UINT8 val[], bool has_payload)
	{
		SetData(val, has_payload);
	}
private:
	VECT_UINT8		payload;
public:
	CHeader			header;
	CAdaptation		adaptation;
	UINT8			*p_start_payload;
	UINT8			*p_end_payload;
public:
	void SetData(UINT8 val[], bool has_payload)
	{
		UINT8 *p = val;
		int size;
		size = header.SetData( p );
		p += size;
		if ( header.has_adaptation )
		{
			size = adaptation.SetData( p );
			p += size;
		}
		else
			adaptation.length = 0;
		if ( !header.has_payload )
		{
			p_start_payload = NULL;
			p_end_payload = NULL;
		}
		else
		{
			if ( has_payload )
			{
				payload.clear();
				// 残りをpayloadに入れとく
				std::copy( p, &val[TS_PACKET_SIZE], back_inserter( payload ) );
				p_start_payload = &payload[0];
				p_end_payload = p_start_payload + payload.size();
			}
			else
			{	
				// 引数の配列のポインタをそのまま使用。
				// 配列が解放されないこと前提
				p_start_payload = p;
				p_end_payload = &val[TS_PACKET_SIZE];
			}
		}
	}
};

#endif