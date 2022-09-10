// '16/10/18 SimpleXAudio2 from (pxwXA2stream + pxwXaudio2).

#ifndef SimpleXAudio2_H
#define SimpleXAudio2_H


#include <comdef.h>
#include <XAudio2.h>

#include <cstdint>

typedef bool ( *simplefunc_sample_pcm )( void *user, void *buf, int *p_res_size, int *p_req_size );

#ifndef SAFE_DELETE
#define SAFE_DELETE(p) { if( p ){ delete( p ); p = NULL; } }
#endif

class SimpleXA2_callback : public IXAudio2VoiceCallback
{
public:
	HANDLE h_bufend;

	 SimpleXA2_callback(): h_bufend( CreateEvent( NULL, FALSE, FALSE, NULL ) ){}
	~SimpleXA2_callback(){ CloseHandle( h_bufend ); }

	void STDMETHODCALLTYPE OnBufferEnd( void * buf_cntx );
	void STDMETHODCALLTYPE OnStreamEnd               (){}
    void STDMETHODCALLTYPE OnVoiceProcessingPassEnd  (){}
    void STDMETHODCALLTYPE OnVoiceProcessingPassStart( UINT32 req_smp              ){}
    void STDMETHODCALLTYPE OnBufferStart             ( void* buf_cntx              ){}
    void STDMETHODCALLTYPE OnLoopEnd                 ( void* buf_cntx              ){}
    void STDMETHODCALLTYPE OnVoiceError              ( void* buf_cntx, HRESULT err ){}
};

class SimpleXAudio2
{
private:

	enum
	{
		_BUFFER_COUNT = 3,
	};

	bool                    _b_init       ;
	
					        
	HANDLE                  _h_thrd       ;
	unsigned int            _thrd_id      ;
	CRITICAL_SECTION        _cs_order_stop; bool _b_order_stop;
	CRITICAL_SECTION        _cs_working   ; bool _b_working   ;


	IXAudio2*               _xa2          ;
	IXAudio2MasteringVoice* _voice_master ;

	SimpleXA2_callback*     _clbk;

	uint8_t*                _bufs[ _BUFFER_COUNT ];
			               
	float                   _buf_sec      ;
	int32_t                 _smp_num      ;
	int32_t                 _buf_size     ;
	uint8_t                 _def_byte     ;
									      
	IXAudio2SourceVoice*    _xa2_src      ;
	WAVEFORMATEX            _fmt          ;
						    
	simplefunc_sample_pcm   _sampling_proc;
	void*                   _sampling_user;

	static unsigned __stdcall _proc_stream_static( void* arg );
	bool        _proc_stream       ();
	bool        _release           ();
public:

	 SimpleXAudio2();
    ~SimpleXAudio2();

	bool init      ( int32_t ch_num, int32_t sps, float buf_sec );
	bool start     ( simplefunc_sample_pcm sampling_proc, void* sampling_user, int priority, const char* th_name );
	bool order_stop();
	bool is_working();
	bool join      ( float timeout_sec );
};

#endif
