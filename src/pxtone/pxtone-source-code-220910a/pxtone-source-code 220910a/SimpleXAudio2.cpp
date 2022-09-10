
#include <winerror.h>
#include <process.h>

#include "./SimpleXAudio2.h"

#define _BITPERSAMPLE16 16

static void _set_pcm_format_16bps( WAVEFORMATEX* p_fmt, int32_t ch_num, int32_t sps )
{
	p_fmt->cbSize          =               0;
	p_fmt->wFormatTag      = WAVE_FORMAT_PCM;
	p_fmt->nChannels       = (WORD )ch_num;
	p_fmt->nSamplesPerSec  = (DWORD)sps   ;
	p_fmt->wBitsPerSample  = (WORD )_BITPERSAMPLE16;
	p_fmt->nBlockAlign     = p_fmt->nChannels   * p_fmt->wBitsPerSample / 8;
	p_fmt->nAvgBytesPerSec = p_fmt->nBlockAlign * p_fmt->nSamplesPerSec;
}

void SimpleXA2_callback::OnBufferEnd( void * buf_cntx )
{
	SetEvent( h_bufend );	
}

SimpleXAudio2::SimpleXAudio2()
{
	_b_init         = false;

	_xa2            = NULL ;
	_voice_master   = NULL ;
	_xa2_src        = NULL ;

	_h_thrd         = NULL ;
	_thrd_id        =     0;

	InitializeCriticalSection( &_cs_order_stop ); _b_order_stop = false;
	InitializeCriticalSection( &_cs_working    ); _b_working    = false;

	_def_byte       =     0;

	_clbk           = NULL ;

	memset( _bufs, NULL, sizeof(_bufs) );
	memset( &_fmt,    0, sizeof(_fmt ) );

	_sampling_proc  = NULL;
	_sampling_user  = NULL;
}

bool SimpleXAudio2::_release()
{
	if( !_b_init ) return false;

	if( _xa2_src      ) _xa2_src->DestroyVoice     (); _xa2_src      = NULL;
	if( _voice_master ) _voice_master->DestroyVoice(); _voice_master = NULL;
	if( _xa2          ) _xa2->Release              (); _xa2          = NULL;

	SAFE_DELETE( _clbk );

	return true;
}

SimpleXAudio2::~SimpleXAudio2()
{
	_release();

	DeleteCriticalSection( &_cs_order_stop );
	DeleteCriticalSection( &_cs_working    );
}

bool SimpleXAudio2::init( int32_t ch_num, int32_t sps, float buf_sec )
{
	if( _b_init ) return false;
		
	bool b_ret = false;
	
	_def_byte = 0x00; // 16bit;


	UINT32 device_num =     0;

	if( _b_init ) return false;

	if( FAILED( XAudio2Create( &_xa2, 0, XAUDIO2_DEFAULT_PROCESSOR ) ) ) goto term;
	if( FAILED( _xa2->GetDeviceCount( &device_num ) ) ) goto term; // need include "$(DXSDK_DIR)Include;"
	if( device_num <= 0 ) goto term;
	if( FAILED( _xa2->CreateMasteringVoice( &_voice_master, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0, 0, NULL ) ) ) goto term;

	if( !(_clbk = new SimpleXA2_callback()) ) goto term;

	_set_pcm_format_16bps( &_fmt, ch_num, sps );

	if( FAILED( _xa2->CreateSourceVoice( &_xa2_src, &_fmt, 0, XAUDIO2_DEFAULT_FREQ_RATIO, _clbk, NULL, NULL ) ) ) goto term;

	_buf_sec  = buf_sec;
	_smp_num  = (int32_t)(buf_sec * sps);
	_buf_size = _smp_num * ch_num * _BITPERSAMPLE16 / 8;

	for( int i = 0; i < _BUFFER_COUNT; i++ )
	{
		if( !(  _bufs[ i ] = (uint8_t*)malloc( _buf_size )) ) goto term;
		memset( _bufs[ i ], _def_byte, _buf_size );
	}

	_b_init  = true;
	b_ret    = true;
term:

	if( !b_ret ) _release();
	return b_ret;
}

bool SimpleXAudio2::start( simplefunc_sample_pcm sampling_proc, void* sampling_user, int priority, const char* th_name )
{
	if( !_b_init ) return false;

	_sampling_proc = sampling_proc;
	_sampling_user = sampling_user;

	{
		bool b_working = false;
		EnterCriticalSection( &_cs_working );
		b_working = _b_working;
		LeaveCriticalSection( &_cs_working );
		if( b_working ) return false;
	}

	bool b_ret = false;

	EnterCriticalSection( &_cs_working );
	_b_working = true;
	LeaveCriticalSection( &_cs_working );

	if( !( _h_thrd = (HANDLE)_beginthreadex( NULL, 0, _proc_stream_static, reinterpret_cast<void*>(this), CREATE_SUSPENDED, &_thrd_id ) ) ) goto term;

	int prio = THREAD_PRIORITY_NORMAL;
	switch( priority )
	{
	case 4 : prio = THREAD_PRIORITY_HIGHEST     ; break;
	case 3 : prio = THREAD_PRIORITY_ABOVE_NORMAL; break;
	case 2 : prio = THREAD_PRIORITY_NORMAL      ; break;
	case 1 : prio = THREAD_PRIORITY_LOWEST      ; break;
	case 0 : prio = THREAD_PRIORITY_IDLE        ; break;
	}
	SetThreadPriority( _h_thrd, prio );

	if( ResumeThread( _h_thrd ) == -1 ) goto term;

	b_ret = true;
term:
	if( !b_ret )
	{
		EnterCriticalSection( &_cs_working );
		_b_working = false;
		LeaveCriticalSection( &_cs_working );
	}

	return b_ret;
}

unsigned SimpleXAudio2::_proc_stream_static( void* arg ){ return reinterpret_cast<SimpleXAudio2*>(arg)->_proc_stream(); }
bool     SimpleXAudio2::_proc_stream       ()
{
	bool b_ret   = false;
	bool b_break = false;
	int  buf_idx =     0;

	XAUDIO2_VOICE_STATE state = {0};

	if( FAILED( _xa2_src->Start( 0, 0 ) ) ) goto term;

	do
	{
		while( _xa2_src->GetState( &state ), state.BuffersQueued >= _BUFFER_COUNT - 1 )
		{
			WaitForSingleObject( _clbk->h_bufend, INFINITE );
		}

		EnterCriticalSection( &_cs_order_stop );
		if( _b_order_stop ) b_break = true;
		LeaveCriticalSection( &_cs_order_stop );

		{
			int            res_size =         0;
			int            req_size = _buf_size;
			XAUDIO2_BUFFER xabuf    =     { 0 };
			xabuf.AudioBytes = _buf_size;
			xabuf.pAudioData = _bufs[ buf_idx ];

			if( !_sampling_proc || !_sampling_proc( _sampling_user, _bufs[ buf_idx ], &res_size, &req_size ) )
			{
				memset( _bufs[ buf_idx ], _def_byte, _buf_size );
			}
			if( b_break )
				xabuf.Flags = XAUDIO2_END_OF_STREAM;
			_xa2_src->SubmitSourceBuffer( &xabuf );
		}

		buf_idx++; buf_idx %= _BUFFER_COUNT;
	}
	while( !b_break );

	while( _xa2_src->GetState( &state ), state.BuffersQueued > 0 )
	{
		WaitForSingleObject( _clbk->h_bufend, INFINITE );
	}
	b_ret = true;
term:
	return b_ret;
}

bool SimpleXAudio2::order_stop()
{
	if( !_b_init ) return false;

	EnterCriticalSection( &_cs_order_stop );
	_b_order_stop = true;
	LeaveCriticalSection( &_cs_order_stop );

	return true;
}

bool SimpleXAudio2::is_working()
{
	if( !_b_init ) return false;
	bool b_ret = false;
	if( !TryEnterCriticalSection( &_cs_working ) ) return true;
	b_ret = _b_working;
	LeaveCriticalSection( &_cs_working );
	return b_ret;
}

bool SimpleXAudio2::join( float timeout_sec )
{
	if( !_b_init ) return false;
	if( !_h_thrd ) return false;

	bool  b_ret = false;
	DWORD sec   =     0;

	if( timeout_sec < 0 ) sec = INFINITE;
	else                  sec = (DWORD)(timeout_sec * 1000);
	switch( WaitForSingleObject( _h_thrd, sec ) )
	{
	case WAIT_ABANDONED: break;
	case WAIT_OBJECT_0 : b_ret = true; break;
	case WAIT_TIMEOUT  : break;
	}
	if( b_ret ){ CloseHandle( _h_thrd ); _h_thrd = NULL; }
	return b_ret;
}
