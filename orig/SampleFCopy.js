//-------------------------------------------------
// %1：処理するtsファイル
// %2：ジャンル解析するサービス番号。１サービスしか入ってないなら指定する必要なし
//-------------------------------------------------
//↓↓↓ 必要に応じて変更 !!!!!
var prm_strEITOutPath = "E:\\Tools\\PT1\\script\\eit_txtout.exe";	// eit_txtoutのパス
var prm_strEITOutPrm = "-pso10% -r10s"
var prm_isEITOutLog = true;
var prm_strTsStorePath = "c:\\share\\ts";
var prm_isMove = false;												// コピー/移動
var prm_CopyCmd = "";												// コピーコマンドは外部コマンドを使用可能
//var prm_CopyCmd = "E:\\Tools\\FireCopy\\FFC.exe \"<src>\" /min /t /to:\"<dstdir>\"";
																	// コピーコマンド。以下の文字列は置換される
																	// <src>	:コピー元ファイル名
																	// <dst>	:コピー先ファイル名
																	// <dstdir>	:コピー先フォルダ名
//↑↑↑ 必要に応じて変更 !!!!!
//*** はじめに ***
// このスクリプトはstore_dir内のフォルダ構成を変更しません。
// つまり、自動的にジャンル名やタイトル名の入ったフォルダを作成する。ということはしません。
// 今あるフォルダ内で最も条件に合致するサブフォルダ内に移動/コピーします。
//
// 移動/コピーするフォルダ内に既に同名のファイルが存在した場合、今移動/コピーしようとしているファ
// イルタイトルの末尾に(数字)を付加して移動/コピーします(但しコピー元とコピー先が同じフォルダの場
// 合は何もしません)
//
// コピー/移動どちらを行うかはprm_isMoveの値に依ります
//
//*** 動作仕様 ***
//以下のフォルダ構成とします
//	<store_dir>
//		<@映画>
//		<@アニメ>
//			<ルパン三世>
//			<ルパン>
//			<fate／zero>
//			<灼眼のシャナIII>
//
// (1) 渡されたファイル名(src_fpath)内にサブフォルダ名が含まれる場合、そのフォルダに移動/コピーします
// 	   複数のフォルダ名に含まれる場合、最長一致するフォルダ名となります
// 	   (例) ルパン三世 カリオストロの城.ts → /store_dir/@アニメ/ルパン三世
// 	   			※ <ルパン>(3文字)より<ルパン三世>(5文字)の方が長く一致してるのでこちらが選択される
//
// (2) ファイル名(タイトル)を含むフォルダがなかった場合、eit_txtoutで読み込んだジャンル(g_GenreStr
// 	   の値)と一致するフォルダにコピー／移動します	
// 	   (例) ダイハード４.ts → /store_dir/@映画	(ダイハード４.tsのジャンルが"映画"である場合)
//
// (3) タイトルにもジャンルにも合致しなかった場合、<store_dir>直下にコピー／移動します。
// 	   (例) 南極大陸 第1話.ts → /store_dir	
//
// その他
// (a) 番組名は放送局によって、全角/半角 大文字/小文字、空白の有無がバラバラなので全部同じモノとして
// 	   扱います。またローマ数字(Ⅰ,Ⅱ,Ⅲ)とローマ数字風アルファベット(I,II,III)も同じモノと扱います
// 	   (例)	Fate／Zero #01.ts				→ store_dir\@アニメ\fate／zero
// 	   		Ｆａｔｅ／Ｚｅｒｏ 第1話.ts		→ store_dir\@アニメ\fate／zero
// 	   		灼眼のシャナ Ⅲ　#01.ts			→ store_dir\@アニメ\灼眼のシャナIII
// 	   		灼眼のシャナⅢ　#01.ts			→ store_dir\@アニメ\灼眼のシャナIII
// 	   		灼眼のシャナIII #01.ts			→ store_dir\@アニメ\灼眼のシャナIII
// 	   なお漢数字(一二三)、アラビア数字(１２３)、ローマ数字(ⅠⅡⅢ)は同じモノとしては扱わないので
// 	   (例) ルパン３世 カリオストロの城.ts → store_dir\@アニメ\ルパン
// 	        ルパンⅢ世 カリオストロの城.ts → store_dir\@アニメ\ルパン
// 	   となります。
// 	   上記のことから、サブフォルダに全角/半角、空白の有無の異なるフォルダが混在してると思った通りに
// 	   動かないかもしれません。
// 	   つまり<fate／zero>フォルダと<Ｆａｔｅ／Ｚｅｒｏ>フォルダがstore_dirのサブフォルダ内にあるとど
// 	   ちらに入るかはFileSystemObjectの仕様とサブフォルダの構成に依ります。一番最初に一致したフォルダ
// 	   になると思いますが、何をもって一番最初と定義するかの説明は省略します。
//
// (b) store_dir内のサブフォルダはOSが許容すれば、どれだけ深い階層に作っても全て検索する筈です。
//
// (c) ジャンル名フォルダを変更したい場合、g_GenreStrにジャンル名フォルダを代入してる場所を修正して下さい
//     但し、アニメジャンルを"アニメ"というジャンル名フォルダとかにすると、「アニメータのお仕事」みたいな
//     ドキュメンタリー番組がもしあった場合にファイル名(タイトル)一致するので、あまり番組名で使われないよ
//     うなフォルダ名にしておいた方がいいです。
//     
// (d) eit_txtoutの出力ファイルoutput.txtが残って鬱陶しい場合は
// 			g_objFileSys.DeleteFile( output_file );
// 	   のコメントを外すといい。
//**************************************************
var g_GenreStr;														// 読み込んだTSファイルのジャンル
var g_objFileSys = new ActiveXObject("Scripting.FileSystemObject");
var g_objShell = WScript.CreateObject("WScript.Shell");

//**************************************************
// コピー
// 【引数】
// 	src_file		コピー元ファイル
// 	dst_file		コピー先ファイル
// 	is_over_write	上書きする？
// 	is_move			移動(コピー後コピー元ファイルを削除)する?
//
//**************************************************
function copy_file(src_file, dst_file, is_over_write, is_move)
{
	var date_1, date_2;
	var drv_src = g_objFileSys.GetDriveName(src_file);
	var drv_dst = g_objFileSys.GetDriveName(dst_file);
	// コピー/移動する必要があるかどうかを判別。同じ場所にあるファイルは処理しない
	if ( g_objFileSys.FolderExists( dst_file ) )					// コピー先がフォルダの場合
	{
		if ( g_objFileSys.GetParentFolderName(src_file).toLowerCase() == dst_file.toLowerCase() )
			return;
	}
	else if ( src_file.toLowerCase() == dst_file.toLowerCase() )	// コピー先がファイルの場合
		return;

	if ( is_move )
		WScript.Echo( "Move: " + src_file +  " -> " + dst_file );
	else
		WScript.Echo( "Copy: " + src_file +  " -> " + dst_file );
	date_1 = new Date();
	if ( !is_over_write && is_move && drv_src == drv_dst )
		g_objFileSys.MoveFile( src_file, dst_file );
	else
	{
		if ( prm_CopyCmd == null || prm_CopyCmd == "" )
		{
			// 定義されてないとき、普通のコピーを使用する
			g_objFileSys.CopyFile( src_file, dst_file, is_over_write );
		}
		else
		{
			var is_rename = false;
			var org_src_file = "";
			var s_cmd_line = prm_CopyCmd;
			s_cmd_line = s_cmd_line.replace("<dst>", dst_file );
			if ( s_cmd_line.indexOf( "<dstdir>" ) >= 0 )
			{
				if ( g_objFileSys.FolderExists( dst_file ) )
					s_cmd_line = s_cmd_line.replace("<dstdir>", dst_file );
				else
				{
					s_cmd_line = s_cmd_line.replace("<dstdir>", g_objFileSys.GetParentFolderName(dst_file) );
					// src_file を適当な名前に変更(後でリネームする)
					// コピー元とコピー先が同じ場合、問題有り
					var d = new Date();
					var new_src_file = g_objFileSys.GetParentFolderName(src_file);
					new_src_file = g_objFileSys.BuildPath( new_src_file, g_objFileSys.GetTempName() );
					g_objFileSys.MoveFile( src_file, new_src_file );
					org_src_file = src_file;
					src_file = new_src_file;
					is_rename = true;
				}
			}
			s_cmd_line = s_cmd_line.replace("<src>", src_file );
			g_objShell.Run( s_cmd_line, 1, true );
			if ( is_rename )
			{	// 名前を変更する
				var src_fname = g_objFileSys.GetFileName( src_file );
				var dst_fname = g_objFileSys.GetFileName( dst_file );
				if ( src_fname != dst_fname )
				{
					var dst_dir = g_objFileSys.GetParentFolderName( dst_file );
					g_objFileSys.MoveFile( 
							g_objFileSys.BuildPath(dst_dir, src_fname),
							g_objFileSys.BuildPath(dst_dir, dst_fname)
					);
				}
				// 元ファイルの名前を元に戻す
				if ( !is_move )
					g_objFileSys.MoveFile( src_file, org_src_file );
			}
		}
		// 移動の時は元ファイルを削除する
		if ( is_move )
			g_objFileSys.DeleteFile( src_file );

	}
	date_2 = new Date();
	WScript.Echo( (date_2.valueOf() - date_1.valueOf())/1000 + "sec" );
}

//**************************************************
// eit_txtout ファイル処理
// 【引数】
//	strTsFile		入力TSファイル
//	svc_no			キーとなるサービス№
//	strOutDir		output.txtを格納するフォルダ
//	
//	【返り値】
//		-1		失敗
//		他		bit:0	inc_svc
//				bit:1	multi_svc
//				bit:2	bs_nan
//**************************************************
function eit_txtout_procs( strTsFile, strSvsNo, strOutDir )
{
	var cmd_line = prm_strEITOutPath + " " + prm_strEITOutPrm;
	if ( prm_isEITOutLog )
	{
		cmd_line += " -l\"" + g_objFileSys.BuildPath( g_objFileSys.GetParentFolderName(strTsFile), "eit_txtout_log.txt" ) + "\"";
	}
	WScript.Echo("*** eit_txtout_procs ***");
	cmd_line += " \"" + strTsFile + "\"";
	var output_file = g_objFileSys.BuildPath( strOutDir, "output.txt" );
	cmd_line += " \"" +  output_file + "\"";
	// 前のoutput.txtを削除。そのままにしておいても普通上書きされるが、起動失敗した際に前のが残ってると混乱するかもしれないので。
	if ( g_objFileSys.FileExists( output_file ) )
		g_objFileSys.DeleteFile( output_file );
	WScript.Echo( cmd_line );
	if ( g_objShell.Run( cmd_line, 1, true ) == 0 ) 
    {
        var fs = g_objFileSys.OpenTextFile(output_file, 1);
		var lines = fs.ReadAll();
		var ret = 0;
		fs.Close();
		// ジャンル文字列取得
		var s_find_ptn;				// 検索パターン
		if ( strSvsNo == null || strSvsNo == "" )
			// サービス指定なしの場合、最初のEVTを検索する。ジャンル値を記憶
			s_find_ptn = "^EVT\\t\\d+";
		else
		
			// EVT	<strSvsNo>･･･となる最初のEVTを検索する。ジャンル値を記憶
			s_find_ptn = "^EVT\\t"+strSvsNo;
		s_find_ptn += "\\t\\d+\\t[\\d :/]+\\t[\\d :/]+\\t[^\\t]+\\t([\\-\\d]+)\\t([^\\t]+)\\t.*$";
		if ( lines.match( new RegExp(s_find_ptn, "m") ) != null )
		{
			var genre_val = RegExp.$1.valueOf();
			//WScript.Echo( "ジャンル大分類" + RegExp.$2 );
			if ( genre_val >= 0 )
			{
				switch ( (genre_val >> 4) & 0xF )
				{
				case 0:	g_GenreStr = "@ニュース";			break;
				case 1: 
					if ( (genre_val & 0xF) == 0 )	g_GenreStr = "@ニュース";		// スポーツニュースはニュースに入れる
					else							g_GenreStr = "@スポーツ";
					break;
				case 2:	g_GenreStr = "@ワイドショー";		break;
				case 3: g_GenreStr = "@ドラマ";				break;
				case 4: g_GenreStr = "@音楽";				break;
				case 5: g_GenreStr = "@バラエティ";			break;
				case 6: 
					if ( (genre_val & 0xF) == 2 )	g_GenreStr = "@アニメ";			// アニメ映画はアニメに入れる
					else							g_GenreStr = "@映画";
					break;
				case 7: g_GenreStr = "@アニメ";				break;
				case 8: g_GenreStr = "@ドキュメンタリー";	break;
				case 9: g_GenreStr = "@劇場";				break;
				case 10: 
					if ( (genre_val & 0xF) <= 6 )	g_GenreStr = "@趣味";
					else							g_GenreStr = "@教育";
					break;
				case 11: g_GenreStr = "@福祉";				break;
				}
				g_GenreStr = convert_zenhan( g_GenreStr );
				g_GenreStr = g_GenreStr.toLowerCase();
			}
		}
		else
			g_GenreStr = "";
		WScript.Echo( "ジャンル：" + g_GenreStr );
		// output.txtが残ってて鬱陶しい場合はコメントアウト
//		g_objFileSys.DeleteFile( output_file );

		return ret;
	}
	else
		return -1;
}

//**************************************************
// TSファイルを自動リネームして移動/コピー
// 	
// 【引数】
// 	src_file		移動元ファイル名フルパス
// 	s_dst_dir		移動先フォルダ
// 	【返り値】
// 	移動後のファイル名フルパス
//**************************************************
function copy_file_auto_rename(src_file, s_dst_dir)
{
	var aft_name;
	// 移動先に同じ名前のファイルがあった場合、(?)を付ける
	aft_name = g_objFileSys.GetFileName( src_file );
	aft_name = g_objFileSys.BuildPath( s_dst_dir, aft_name );
	if ( g_objFileSys.FileExists( aft_name ) )
	{
		var s_base = g_objFileSys.GetBaseName(src_file);
		var s_ext = g_objFileSys.GetExtensionName(src_file);
		var i = 0;
		do
		{
			i++;
			aft_name = g_objFileSys.BuildPath(s_dst_dir, s_base+"("+i+")."+s_ext );
		}while ( g_objFileSys.FileExists( aft_name ) );
	}
	// コピー／移動
	copy_file( src_file, aft_name, false, prm_isMove );
	return aft_name;
}

//**************************************************
// 全角→半角変換。但しカナは変換しない
// 	
// 【引数】
// 	src_fpath	
//**************************************************
// JScriptは定数がないので、仕方なしにここで宣言(毎回代入されるのはなんか嫌なので)
var c_full_alpha = "ＡＢＣＤＥＦＧＨＩＪＫＬＭＮＯＰＱＲＳＴＵＶＷＸＹＺａｂｃｄｅｆｇｈｉｊｋｌｍｎｏｐｑｒｓｔｕｖｗｘｙｚ";
var c_half_alpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
var c_full_number = "０１２３４５６７８９";
var c_half_number = "0123456789";
var c_full_symbol = "　！”＃＄％＆’＇（）＊＋，ー－．／：；＜＝＞？＠［￥］＾＿｀｛｜｝～";
var c_half_symbol = " !\"#$%&''()*+,--./:;<=>?@[\\]^_`{|}~";
var c_full_romanum = "ⅠⅡⅢⅣⅤⅥⅦⅧⅨⅩ";
var c_half_romanum = ("I,II,III,IV,V,VI,VII,VIII,IX,X").split(",");		// 1文字で置き換えられないので、コンマで区切る
function convert_zenhan( str )
{
	var is_full_alpha=false, is_full_number=false, is_full_symbol=false, is_full_romanum=false;
	var i;
	if ( str == null )
		return null;
	// 変換する全角文字が含まれるか判別
	for ( i = 0; i < str.length; i++ )
	{
		var code = str.charCodeAt(i);
		if ( !is_full_symbol
			&& (code == 0x201d || code == 0x2019 || code == 0x3000		// ”’　
				||	code == 0x30fc || code == 0xff01					// ー！
				||	(0xff03<= code && code <= 0xff0f)					// ＃＄％＆＇（）＊＋，．－．
				||	(0xff1a<= code && code <= 0xff20)					// ：；＜＝＞？＠
				||	(0xff3b<= code && code <= 0xff40 && code != 0xff3c)	// ［］＾＿｀	但し＼は除く
				||	(0xff5b <= code && code <= 0xff5e)					// ｛｜｝～
				||	code == 0xff5e)										// ￥
			)
			is_full_symbol = true;
		else if (!is_full_number && 0xff10 <= code && code <= 0xff19)
			is_full_number = true;
		else if (!is_full_alpha && ((0xff21 <= code && code <= 0xff3a) || (0xff41 <= code && code <= 0xff5a)))
			is_full_alpha = true;
		else if (!is_full_romanum && 0x2160 <= code && code <= 0x2169)
			is_full_romanum = true;
	}
	var s1, s2;
	var sret = str;
	if ( is_full_alpha )
	{
		for ( i = 0; i < c_full_alpha.length; i++ )
		{
			s1 = c_full_alpha.substr(i,1);
			s2 = c_half_alpha.substr(i,1);
			sret = sret.replace(s1, s2);
		}
	}
	if ( is_full_number )
	{
		for ( i = 0; i < c_full_number.length; i++ )
		{
			s1 = c_full_number.substr(i,1);
			s2 = c_half_number.substr(i,1);
			sret = sret.replace(s1, s2);
		}
	}
	if ( is_full_symbol )
	{
		for ( i = 0; i < c_full_symbol.length; i++ )
		{
			s1 = c_full_symbol.substr(i,1);
			s2 = c_half_symbol.substr(i,1);
			sret = sret.replace(s1, s2);
		}
	}
	if ( is_full_romanum )
	{
		for ( i = 0; i < c_full_romanum.length; i++ )
		{
			s1 = c_full_romanum.substr(i,1);
			s2 = c_half_romanum[i];
			sret = sret.replace(s1, s2);
		}
	}
	return sret;
}

//**************************************************
// 文字列比較のための変換
// 	
// 【引数】
// 	str				文字列
//
// 【返り値】
// 変換された文字列
// 	全角／半角、大文字／小文字、空白の有無
// 	を全て同一視するため変換する
//**************************************************
function convert_str( str )
{
	var s_ret = str;

	s_ret	= convert_zenhan( s_ret );			// 全角→半角
	s_ret	= s_ret.toLowerCase( s_ret );		// 全部小文字に
	s_ret	= s_ret.replace( " ", "");			// 空白を無視

	return s_ret;
}

//**************************************************
// 
// 	
// 【引数】
// 	s_parent_dir	親フォルダ
// 	str				文字列(変換済み)
// 【返り値】
// strが含まれているまたはジャンル文字列(g_GenreStr)と一致するサブフォルダがあった場合、そのパスを返す。
//
// 但しJScriptは引数の参照渡しが出来ないので、strが含まれてるのかジャンル文字列と一致したのか判別したの
// かを判別するため、strが含まれてる場合には"<<"を先頭に付加して返す。
//**************************************************
function find_includestr_dir( s_parent_dir, str )
{
	var objFolder = g_objFileSys.GetFolder( s_parent_dir );
	var fc = new Enumerator(objFolder.SubFolders);
	var ret = null;

	for ( ; !fc.atEnd(); fc.moveNext() )
	{
		var a_ret = find_includestr_dir( fc.item().Path, str );
		var ret_name;
		if ( a_ret != null )
		{
			if 		( ret == null )
				ret = a_ret;
			else if ( a_ret.length > 2 && a_ret.substr(0,2) == "<<" )
			{
				// 最長一致のため、フォルダ名称を取り出して比較
				var a_ret_name;
				if ( ret != null )		ret_name = g_objFileSys.GetFileName(ret);
				else 					ret_name = "";
				if ( a_ret != null )	a_ret_name = g_objFileSys.GetFileName(a_ret);
				else 					a_ret_name = "";

				if ( a_ret_name.length > ret_name.length )
					ret = a_ret;
			}
		}
		var s_name = convert_str( fc.item().Name );
		if 		( str.indexOf(s_name) >= 0 )
		{
			if ( ret == null || ret.length < 2 || ret.substr(0,2) != "<<" )
				ret = "<<" + fc.item().Path;						// ジャンル名フォルダと区別するため"<<"を付ける
			else
			{
				// 最長一致のため、フォルダ名称を取り出して比較
				if ( ret != null )	ret_name = g_objFileSys.GetFileName(ret);
				else 				ret_name = "";
				if ( s_name.length > ret_name.length )
					ret = "<<" + fc.item().Path;					// ジャンル名フォルダと区別するため"<<"を付ける
			}
		}
		else if ( ret == null && s_name == g_GenreStr )			// ジャンル名フォルダの場合、
			ret = fc.item().Path;
	}
	return ret;
}

//**************************************************
// 保存フォルダ(prm_strTsStorePath)に移動(またはコピー)
// 	
// 【引数】
// 	src_fpath	移動/コピーするファイル
//
//**************************************************
function tsfile_storedir_copy( src_fpath )
{
	var cvt_src_fbase = convert_str( g_objFileSys.GetBaseName(src_fpath) );
	var dst_path;

	dst_path = find_includestr_dir( prm_strTsStorePath, cvt_src_fbase );
	if ( dst_path != null && dst_path.length > 2 && dst_path.substr(0,2) == "<<" )
		dst_path = dst_path.substr(2);		// << を消す
	if ( dst_path == null )
		dst_path = prm_strTsStorePath;
	if ( g_objFileSys.GetParentFolderName(src_fpath).toLowerCase() != dst_path.toLowerCase() )
		copy_file_auto_rename( src_fpath, dst_path );
}

function main_proc(s_tsfile, s_svcno)
{
	var ret;
	if ( !g_objFileSys.FileExists( s_tsfile ) )
	{
		WScript.Echo( "ファイルが存在しません" + s_tsfile );
		return -1;
	}
	else if ( g_objFileSys.GetExtensionName( s_tsfile ).toLowerCase() != "ts" )
	{
		WScript.Echo( "TSファイルを指定して下さい" + s_tsfile );
		return -2;
	}
	eit_txtout_procs( s_tsfile, s_svcno, g_objFileSys.GetParentFolderName(s_tsfile) );
	tsfile_storedir_copy( s_tsfile );
	return;
}

// global proc
//--------------------------------------------------
var strOrgTsFile;								// %1
var strSvsNo;									// %2
{
	strOrgTsFile = WScript.Arguments(0); 
	if ( WScript.Arguments.Length >= 2 )
		strSvsNo = WScript.Arguments(1);
	var date_1 = new Date();
	WScript.Echo("<<<<<< start "+date_1.toLocaleString() + " <<<<<<" );
	WScript.Echo("SVC:" + strSvsNo + " FILE:" + strOrgTsFile );
	main_proc(strOrgTsFile, strSvsNo);
	date_1 = new Date();
	WScript.Echo(">>>>>> end   "+date_1.toLocaleString() + " >>>>>>" );
}


