#if defined(RTMIDI)
#include "RtMidi.h"
#elif defined(OGG)
#include "ogg/ogg.h"
#elif defined(VORBISFILE)
#include "vorbis/vorbisfile.h"
#endif

int main()
{
  return 0;
}
