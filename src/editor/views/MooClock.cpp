#include "MooClock.h"

MooClock::MooClock(PxtoneClient *client)
    : QObject(client),
      m_client(client),
      m_prev_clock(0),
      m_prev_moo_clock(0),
      m_this_seek(0),
      m_clock(0),
      m_animation(new Animation(this)),
      m_this_seek_caught_up(false) {
  // Ticking is controlled by 1 animation, vs. recomputing [now] every time it's
  // called, because different views call [now] possibly at different times.
  connect(m_animation, &Animation::valueChanged, this, &MooClock::tick);
  connect(m_client->controller(), &PxtoneController::seeked, [this](int clock) {
    m_this_seek = clock;
    m_this_seek_caught_up = false;
    m_prev_clock = 0;
  });
}

void MooClock::tick() {
  int clock = m_client->moo()->get_now_clock();

  // Some really hacky magic to get the playhead smoother given that
  // there's a ton of buffering that makes it hard to actually tell where the
  // playhead is accurately. We do this:
  // 1. Subtract the buffer duration.
  // 2. Track how long since the last (laggy) clock update and add.
  // 3. Works most of the time, but leads to a wrong clock after a seek or at
  // the end of a song, because it'll put the playhead before the repeat or
  // the seek position.
  // 4. To account for this, if we're before the current seek position, clamp
  // to seek position. Also track if we've looped and render at the end of
  // song instead of before repeat if so.
  // 5. The tracking if we've looped or not is also necessary to tell if we've
  // actually caught up to a seek.

  // Additionally, to cover midi input, we can no longer tolerate the cursor
  // flicking around occasionally. So, now we only allow nonmonotonic movement
  // if the moo time changed.

  int bytes_per_second = 4 * 44100;  // bytes in sample * bytes per second
  if (m_prev_moo_clock != clock) {
    m_prev_moo_clock = clock;
    timeSinceLastClock.restart();
  }

  int64_t offset_from_buffer =
      m_client->bufferSize();  // - m_audio_output->bytesFree()

  double estimated_buffer_offset =
      -offset_from_buffer / double(bytes_per_second);
  if (!m_client->isPlaying())
    timeSinceLastClock.restart();
  else
    estimated_buffer_offset += timeSinceLastClock.elapsed() / 1000.0;
  clock += (last_clock() - repeat_clock()) * m_client->moo()->num_loop;

  const pxtnMaster *master = m_client->pxtn()->master;
  clock += std::min(estimated_buffer_offset, 0.0) * master->get_beat_tempo() *
           master->get_beat_clock() / 60;
  // Forces monotonicity
  if (clock < m_prev_clock)
    clock = m_prev_clock;
  else
    m_prev_clock = clock;

  if (clock >= m_this_seek) m_this_seek_caught_up = true;
  if (!m_this_seek_caught_up) clock = m_this_seek;

  m_clock = clock;
}

int MooClock::now() const {
  return MasterExtended::wrapClock(m_client->pxtn()->master, m_clock);

  // 2021-01-18: I'm not actually sure when this case will happen. So I've
  // commented it out.
  //
  // // Because of offsetting it might seem like even though we've repeated the
  // // clock is before [repeat_clock]. So fix it here.
  // if (m_client->moo()->num_loop > 0 && clock < repeat_clock())
  //  clock += last_clock() - repeat_clock();
}

int MooClock::nowNoWrap() const { return m_clock; }

int MooClock::last_clock() const {
  return MasterExtended::last_clock(m_client->pxtn()->master);
}
int MooClock::repeat_clock() const {
  return MasterExtended::repeat_clock(m_client->pxtn()->master);
}

bool MooClock::has_last() const {
  return m_client->pxtn()->master->get_last_meas() > 0;
}
