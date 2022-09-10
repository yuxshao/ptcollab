
#define _CRT_SECURE_NO_WARNINGS

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdio.h>
#include <cstdint>

#include <shlwapi.h>
#pragma comment(lib,"shlwapi")

#include <commctrl.h>
#pragma comment(lib,"comctl32")

#pragma comment( lib, "winmm" )
#pragma comment( lib, "imm32" )
#include              <imm.h>

#include <vector>

using namespace std;

#include <tchar.h>

#include "./pxtone/pxtnService.h"
#include "./pxtone/pxtnError.h"

#include "./SimpleXAudio2.h"

static const TCHAR* _app_name = _T("pxtone-play-sample");

#include <CommDlg.h>

#define _CHANNEL_NUM           2
#define _SAMPLE_PER_SECOND 44100
#define _BUFFER_PER_SEC    (0.3f)


// I/O..
static bool _pxtn_r( void* user,      void* p_dst, int size, int num )
{
	int i = fread( p_dst, size, num, (FILE*)user );
	if( i < num ) return false;
	return true;
}

static bool _pxtn_w( void* user,const void* p_dst, int size, int num )
{
	int i = fwrite( p_dst, size, num, (FILE*)user );
	if( i < num ) return false;
	return true;
}

static bool _pxtn_s( void* user, int mode, int size )
{
	if( fseek( (FILE*)user, size, mode ) ) return false;
	return true;
}

static bool _pxtn_p( void* user, int32_t* p_pos )
{
	int i = ftell( (FILE*)user );
	if( i < 0 ) return false;
	*p_pos = i;
	return true;
}

static bool _SelFile_OpenLoad( HWND hwnd, TCHAR *path_get, const TCHAR *path_def_dir, const TCHAR *title, const TCHAR *ext, const TCHAR *filter )
{
	OPENFILENAME ofn = {0};

	ofn.lStructSize     = sizeof(OPENFILENAME);
	ofn.hwndOwner       = hwnd        ;
	ofn.lpstrFilter     = filter      ;
	ofn.lpstrFile       = path_get    ;
	ofn.nMaxFile        = MAX_PATH    ;
	ofn.lpstrFileTitle  = NULL        ;
	ofn.nMaxFileTitle   =            0;
	ofn.lpstrInitialDir = path_def_dir;
	ofn.lpstrTitle      = title       ;
	ofn.Flags           = OFN_FILEMUSTEXIST|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST;
	ofn.lpstrDefExt     = ext         ;

	if( GetOpenFileName( &ofn ) ) return true;

	return false;
}


static bool _load_ptcop( pxtnService* pxtn, const TCHAR* path_src, pxtnERR* p_pxtn_err )
{
	bool           b_ret     = false;
//	pxtnDescriptor desc;
	pxtnERR        pxtn_err  = pxtnERR_VOID;
	FILE*          fp        = NULL;
	int32_t        event_num =    0;

	if( !( fp = _tfopen( path_src, _T("rb") ) ) ) goto term;
//	if( !desc.set_file_r( fp ) ) goto term;

	pxtn_err = pxtn->read       ( fp ); if( pxtn_err != pxtnOK ) goto term;
	pxtn_err = pxtn->tones_ready(    ); if( pxtn_err != pxtnOK ) goto term;

	b_ret = true;
term:

	if( fp     ) fclose( fp );
	if( !b_ret ) pxtn->evels->Release();

	if( p_pxtn_err ) *p_pxtn_err = pxtn_err;

	return b_ret;
}

// Shift-JIS to UNICODE.
static bool _sjis_to_wide( const char*    p_src, wchar_t** pp_dst, int32_t* p_dst_num  )
{
	bool     b_ret     = false;
	wchar_t* p_wide    = NULL ;
	int      num_wide  =     0;

	if( !p_src    ) return false;
	if( p_dst_num ) *p_dst_num = 0;
	*pp_dst = NULL;

	// to UTF-16
	if( !(num_wide = ::MultiByteToWideChar( CP_ACP, 0, p_src, -1, NULL, 0 ) ) ) goto term; 
	if( !(  p_wide = (wchar_t*)malloc( num_wide * sizeof(wchar_t) ) )         ) goto term;
	memset( p_wide, 0,                 num_wide * sizeof(wchar_t) );
	if( !MultiByteToWideChar( CP_ACP, 0, p_src, -1, p_wide, num_wide )        ) goto term;

	if( p_dst_num ) *p_dst_num = num_wide - 1; // remove last ' '
	*pp_dst = p_wide;

	b_ret = true;
term:
	if( !b_ret ) free( p_wide );

	return b_ret;
}

static bool _sampling_func( void *user, void *buf, int *p_res_size, int *p_req_size )
{
	pxtnService* pxtn = static_cast<pxtnService*>( user );

	if( !pxtn->Moo( buf, *p_req_size ) ) return false;
	if( p_res_size ) *p_res_size = *p_req_size;

	return true;
}

int WINAPI WinMain( HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nCmd )
{
	InitCommonControls();
	if( FAILED( CoInitializeEx( NULL, COINIT_MULTITHREADED ) ) ) return 0;

	bool           b_ret    = false;
	pxtnService*   pxtn     = NULL ;
	pxtnERR        pxtn_err = pxtnERR_VOID;
	SimpleXAudio2* xa2      = NULL ;
	TCHAR          path_src[ MAX_PATH ] = {0};

	static const TCHAR* title  = _T("load ptcop");
	static const TCHAR* ext    = _T("ptcop"     );
	static const TCHAR* filter =
		_T("ptcop {*.ptcop}\0*.ptcop*\0"   )
		_T("pttune {*.pttune}\0*.pttune*\0")
		_T("All files {*.*}\0*.*\0\0"      );

	// INIT PXTONE.
//	pxtn = new pxtnService();
	pxtn = new pxtnService( _pxtn_r, _pxtn_w, _pxtn_s, _pxtn_p );

	pxtn_err = pxtn->init(); if( pxtn_err != pxtnOK ) goto term;
	if( !pxtn->set_destination_quality( _CHANNEL_NUM, _SAMPLE_PER_SECOND ) ) goto term;

	// SELECT MUSIC FILE.
	if( !_SelFile_OpenLoad( NULL, path_src, NULL, title, ext, filter ) )
	{
		b_ret = true;
		goto term;
	};

	// LOAD MUSIC FILE.
	if( !_load_ptcop( pxtn, path_src, &pxtn_err ) ) goto term;

	// PREPARATION PLAYING MUSIC.
	{
		int32_t smp_total = pxtn->moo_get_total_sample();

		pxtnVOMITPREPARATION prep = {0};
		prep.flags          |= pxtnVOMITPREPFLAG_loop;
		prep.start_pos_float =     0;
		prep.master_volume   = 0.80f;

		if( !pxtn->moo_preparation( &prep ) ) goto term;
	}

	// INIT XAudio2.
	xa2 = new SimpleXAudio2();
	if( !xa2->init( _CHANNEL_NUM, _SAMPLE_PER_SECOND, _BUFFER_PER_SEC ) ) goto term;

	// START XAudio2.
	xa2->start( _sampling_func, pxtn, 0, NULL );

	// GET MUSIC NAME AND MESSAGE-BOX.
	{
		TCHAR         text[ 250 ] = { 0 };
		TCHAR*        name_t      = NULL ;
		const TCHAR*  p_name      = NULL ;
		int32_t       buf_size    =   0  ;

#ifdef UNICODE
		if( _sjis_to_wide( pxtn->text->get_name_buf( &buf_size ), &name_t, NULL ) )
		{
			p_name = name_t;
		}
		else
		{
			p_name = _T("none");
		}
#else
		if( !( p_name = pxtn->text->get_name_buf( &buf_size ) ) ) p_name = PathFindFileName( path_src );
#endif

		_stprintf_s( text, 250, _T("file: %s\r\n") _T("name: %s"), PathFindFileName( path_src ), p_name );

		if( name_t ) free( name_t );
		MessageBox( NULL, text, _app_name, MB_OK );
	}

	b_ret = true;
term:

	if( !b_ret )
	{
		// ERROR MESSAGE.
		TCHAR err_msg[ 100 ] = {0};
		_stprintf_s( err_msg, 100, _T("ERROR: pxtnERR[ %s ]"), pxtnError_get_string( pxtn_err ) );
		MessageBox( NULL, err_msg, _app_name, MB_OK );
	}

	// RELEASE XAudio2.
	if( xa2 )
	{
		xa2->order_stop();
		for( int i = 0; i < 20; i++ ){ if( !xa2->is_working() ) break; Sleep( 100 ); }
		xa2->join( 3 );
		SAFE_DELETE( xa2 );
	}

	SAFE_DELETE( pxtn );
	return 1;
}
