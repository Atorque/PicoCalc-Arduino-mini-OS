#include "SoundEngine.h"

#include <Arduino.h>
#include "pwm_sound.h"

// Sample rate: 133 MHz / 6.05 clkdiv / 250 wrap = 87,934 Hz
#define MUSIC_SAMPLE_RATE 87934
#define NUM_VOICES 5
#define V_MELODY 0
#define V_BASS 1
#define V_ARPEGGIO 2
#define V_PAD 3
#define V_SFX 4

struct MusicVoice {
  uint32_t period;
  uint32_t on_samples;
  uint32_t phase;
  uint16_t amplitude;
};

static volatile MusicVoice voices[NUM_VOICES];
static int music_slice = 0;

#define AUTOPAN_STEP_SAMPLES 1400U
#define AUTOPAN_MIN_GAIN 72U
#define AUTOPAN_SWEEP_GAIN 160U

static volatile uint16_t autopan_phase = 0;
static volatile uint16_t autopan_counter = 0;

static const uint8_t VOICE_PAN_L[NUM_VOICES] = {
  96,
  240,
  220,
  128,
  190
};
static const uint8_t VOICE_PAN_R[NUM_VOICES] = {
  240,
  96,
  120,
  128,
  190
};

struct SfxEnvelope {
  uint16_t freq;
  uint8_t duty;
  uint16_t amp;
  uint32_t endAt;
  bool active;
};

static volatile SfxEnvelope sfx = { 0, 0, 0, 0, false };
static volatile bool music_enabled = false;
static volatile SoundMusicMode music_mode = SOUND_MUSIC_AMBIENT;

static void voice_set(int v, uint16_t freq, uint8_t duty_pct, uint16_t amp) {
  voices[v].period = 0;
  if (freq == 0 || duty_pct == 0) return;
  uint32_t p = MUSIC_SAMPLE_RATE / freq;
  if (p == 0) p = 1;
  voices[v].phase = 0;
  voices[v].on_samples = (p * duty_pct) / 100;
  voices[v].amplitude = amp;
  voices[v].period = p;
}

static void voice_off(int v) {
  voices[v].period = 0;
}

static void apply_sfx_if_needed() {
  if (!sfx.active) return;
  if (millis() >= sfx.endAt) {
    sfx.active = false;
    voice_off(V_SFX);
    return;
  }
  voice_set(V_SFX, sfx.freq, sfx.duty, sfx.amp);
}

static void music_pwm_irq_handler() {
  pwm_clear_irq(music_slice);
  if (++autopan_counter >= AUTOPAN_STEP_SAMPLES) {
    autopan_counter = 0;
    if (++autopan_phase >= 512) autopan_phase = 0;
  }

  uint8_t autopan = (autopan_phase < 256) ? (uint8_t)autopan_phase : (uint8_t)(511 - autopan_phase);
  uint32_t mix_l = 0;
  uint32_t mix_r = 0;

  for (int i = 0; i < NUM_VOICES; i++) {
    if (voices[i].period == 0) continue;
    if (++voices[i].phase >= voices[i].period) {
      voices[i].phase = 0;
    }

    if (voices[i].phase < voices[i].on_samples) {
      uint32_t amp = voices[i].amplitude;
      uint8_t pan_l = VOICE_PAN_L[i];
      uint8_t pan_r = VOICE_PAN_R[i];

      if (i == V_MELODY || i == V_ARPEGGIO) {
        uint8_t p = (i == V_MELODY) ? autopan : (uint8_t)(255 - autopan);
        pan_l = (uint8_t)(AUTOPAN_MIN_GAIN + ((((uint16_t)(255 - p)) * AUTOPAN_SWEEP_GAIN) >> 8));
        pan_r = (uint8_t)(AUTOPAN_MIN_GAIN + ((((uint16_t)p) * AUTOPAN_SWEEP_GAIN) >> 8));
      }

      mix_l += (amp * pan_l) >> 8;
      mix_r += (amp * pan_r) >> 8;
    }
  }

  if (mix_l > 250) mix_l = 250;
  if (mix_r > 250) mix_r = 250;
  pwm_set_chan_level(music_slice, PWM_CHAN_A, (uint16_t)mix_l);
  pwm_set_chan_level(music_slice, PWM_CHAN_B, (uint16_t)mix_r);
}

static const uint16_t NOTE_FREQ[60] = {
  65, 69, 73, 78, 82, 87, 92, 98, 104, 110, 117, 123,
  131, 139, 147, 156, 165, 175, 185, 196, 208, 220, 233, 247,
  262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 466, 494,
  523, 554, 587, 622, 659, 698, 740, 784, 831, 880, 932, 988,
  1047, 1109, 1175, 1245, 1319, 1397, 1480, 1568, 1661, 1760, 1865, 1976
};

#define NI(oct, semi) (((oct) - 2) * 12 + (semi))

struct Chord {
  uint8_t bass;
  uint8_t arp[3];
  uint8_t pad;
  uint8_t mel[6];
};

static const Chord CHORDS[4] = {
  { NI(2, 0), { NI(3, 0), NI(3, 4), NI(3, 7) }, NI(4, 0), { NI(4, 0), NI(4, 2), NI(4, 4), NI(4, 7), NI(4, 9), NI(5, 0) } },
  { NI(2, 7), { NI(3, 7), NI(3, 11), NI(4, 2) }, NI(4, 7), { NI(4, 7), NI(4, 9), NI(4, 11), NI(5, 2), NI(5, 4), NI(5, 7) } },
  { NI(2, 9), { NI(3, 9), NI(4, 0), NI(4, 4) }, NI(4, 9), { NI(4, 9), NI(4, 11), NI(5, 0), NI(5, 4), NI(5, 7), NI(5, 9) } },
  { NI(2, 5), { NI(3, 5), NI(3, 9), NI(4, 0) }, NI(4, 5), { NI(4, 5), NI(4, 7), NI(4, 9), NI(5, 0), NI(5, 2), NI(5, 5) } },
};

static uint32_t mrand_state = 0xDEADBEEF;
static inline uint32_t mrand() {
  mrand_state ^= mrand_state << 13;
  mrand_state ^= mrand_state >> 17;
  mrand_state ^= mrand_state << 5;
  return mrand_state;
}

void sound_begin() {
  music_slice = pwm_gpio_to_slice_num(AUDIO_PIN_L);
  for (int i = 0; i < NUM_VOICES; i++) {
    voices[i].period = 0;
    voices[i].on_samples = 0;
    voices[i].phase = 0;
    voices[i].amplitude = 0;
  }
  init_pwm(music_pwm_irq_handler);
}

void sound_set_music_enabled(bool enabled) {
  music_enabled = enabled;
  if (!enabled) {
    // Keep SFX voice alive, but silence all music voices immediately.
    voice_off(V_MELODY);
    voice_off(V_BASS);
    voice_off(V_ARPEGGIO);
    voice_off(V_PAD);
  }
}

void sound_set_music_mode(SoundMusicMode mode) {
  music_mode = mode;
}

static int8_t note_char_to_index(char ch, uint8_t octave) {
  char n = (char)tolower((unsigned char)ch);
  int semi = -1;
  if (n == 'c') semi = 0;
  else if (n == 'd') semi = 2;
  else if (n == 'e') semi = 4;
  else if (n == 'f') semi = 5;
  else if (n == 'g') semi = 7;
  else if (n == 'a') semi = 9;
  else if (n == 'b') semi = 11;
  if (semi < 0) return -1;

  int idx = ((int)octave - 2) * 12 + semi;
  if (idx < 0 || idx >= 60) return -1;
  return (int8_t)idx;
}

static void run_tetris_music() {
  static uint32_t nextStepAt = 0;
  static uint8_t measure = 0;
  static uint8_t pos = 0;
  static bool track5Active = false;
  static bool track4Active = false;
  static bool track3Active = false;
  static int8_t n5 = -1;
  static int8_t n4 = -1;
  static int8_t n3 = -1;
  static uint8_t hold5 = 0;
  static uint8_t hold4 = 0;
  static uint8_t hold3 = 0;

  static const char* TRACK5[29] = {
    "e-----c-d-edc---------c-e-",
    "--d-c-------c-d---e---c---",
    "------------d---d-f-a---g-",
    "f-e---e-c-e---d-c-------c-",
    "d---e---c---------------e-",
    "----c-d-edc---------c-e---",
    "d-c-------c-d---e---c-----",
    "----------d---d-f-a---g-f-",
    "e---e-c-e---d-c-------c-d-",
    "--e---c---------------c---",
    "----c-------d-------------",
    "--c-----------------------",
    "--------c-------c-------d-",
    "--------------c---c---e---",
    "e---e---------------e-----",
    "c-d-edc---------c-c---d-c-",
    "------c-d---c---c---------",
    "------d---d-f-c---g-f-e---",
    "e-c-e---d-c-------c-d---c-",
    "--c---------------e-----c-",
    "d-edc---------c-c---d-c---",
    "----c-d---c---c-----------",
    "----d---d-f-c---g-f-e---e-",
    "c-e---d-c-------c-d---c---",
    "c-------------------------",
    "-",
    "-",
    "-",
    "-"
  };

  static const char* TRACK4[29] = {
    "----b---------b-a---a-----",
    "------b---bb--------------",
    "a---a---------------------",
    "------------------b---bb--",
    "------------a---a---------",
    "--b---------b-a---a-------",
    "----b---b---------------a-",
    "--a-----------------------",
    "----------------b---b-----",
    "----------a---a-----------",
    "----a-------b-------G-----",
    "--a-------e-------e-------",
    "G---------------a-------b-",
    "------G-------a-----------",
    "--------------------b---G-",
    "a-b---a-G-e---e-a-----b-a-",
    "G-e-GG--b-------a---e---e-",
    "------f---f-a-----b-a-g---",
    "g-e-g---f-e-G-e-GG--b-----",
    "--a---e---e-------b---G-a-",
    "b---a-G-e---e-a-----b-a-G-",
    "e-GG--b-------a---e---e---",
    "----f---f-a-----b-a-g---g-",
    "e-g---f-e-G-e-GG--b-------",
    "a---e---e-------c-------c-",
    "------d---------------c---",
    "-",
    "--c-------c-------d-------",
    "--------c---c---e---e---e-"
  };

  static const char* TRACK3[29] = {
    "-",
    "-",
    "-",
    "-",
    "-",
    "-",
    "-",
    "-",
    "-",
    "-",
    "-",
    "-",
    "-",
    "-",
    "-",
    "-",
    "-",
    "-",
    "-",
    "-",
    "-",
    "-",
    "-",
    "-",
    "------------------------a-",
    "------b-------G-------a---",
    "----e-------e-------G-----",
    "----------a-------b-------",
    "G-------a-----------------"
  };

  static const uint16_t TICK_MS = 110;

  uint32_t now = millis();
  if (now < nextStepAt) return;
  nextStepAt = now + TICK_MS;

  const char* p5 = TRACK5[measure];
  const char* p4 = TRACK4[measure];
  const char* p3 = TRACK3[measure];
  uint8_t len5 = (uint8_t)strlen(p5);
  uint8_t len4 = (uint8_t)strlen(p4);
  uint8_t len3 = (uint8_t)strlen(p3);
  uint8_t mlen = len5;
  if (len4 > mlen) mlen = len4;
  if (len3 > mlen) mlen = len3;
  if (mlen == 0) mlen = 1;

  char c5 = (pos < len5) ? p5[pos] : '-';
  char c4 = (pos < len4) ? p4[pos] : '-';
  char c3 = (pos < len3) ? p3[pos] : '-';

  uint8_t nextMeasure = measure;
  uint8_t nextPos = (uint8_t)(pos + 1);
  if (nextPos >= mlen) {
    nextPos = 0;
    nextMeasure = (uint8_t)((measure + 1) % 29);
  }

  const char* np5 = TRACK5[nextMeasure];
  const char* np4 = TRACK4[nextMeasure];
  const char* np3 = TRACK3[nextMeasure];
  char nc5 = (nextPos < strlen(np5)) ? np5[nextPos] : '-';
  char nc4 = (nextPos < strlen(np4)) ? np4[nextPos] : '-';
  char nc3 = (nextPos < strlen(np3)) ? np3[nextPos] : '-';

  int8_t n;
  n = note_char_to_index(c5, 5);
  if (n >= 0) {
    n5 = n;
    track5Active = true;
    hold5 = 0;
  } else if (!track5Active) {
    n5 = -1;
  } else {
    if (hold5 < 250) hold5++;
  }

  n = note_char_to_index(c4, 4);
  if (n >= 0) {
    n4 = n;
    track4Active = true;
    hold4 = 0;
  } else if (!track4Active) {
    n4 = -1;
  } else {
    if (hold4 < 250) hold4++;
  }

  n = note_char_to_index(c3, 3);
  if (n >= 0) {
    n3 = n;
    track3Active = true;
    hold3 = 0;
  } else if (!track3Active) {
    n3 = -1;
  } else {
    if (hold3 < 250) hold3++;
  }

  bool nextHas5 = note_char_to_index(nc5, 5) >= 0;
  bool nextHas4 = note_char_to_index(nc4, 4) >= 0;
  bool nextHas3 = note_char_to_index(nc3, 3) >= 0;

  if (n5 >= 0) {
    uint16_t amp5 = 66;
    uint8_t duty5 = 48;
    if (hold5 > 0) {
      uint16_t dec = (uint16_t)(hold5 * 4);
      amp5 = (dec >= 24) ? 42 : (uint16_t)(66 - dec);
      duty5 = 44;
      if (nextHas5) {
        // Small breath before next attack to reduce mechanical transitions.
        if (amp5 > 6) amp5 -= 6;
        duty5 = 36;
      }
    }
    voice_set(V_MELODY, NOTE_FREQ[n5], duty5, amp5);
  } else {
    voice_off(V_MELODY);
  }

  int8_t bass = n3;
  if (bass < 0 && n4 >= 12) bass = (int8_t)(n4 - 12);
  if (bass >= 0) {
    uint16_t ampB = 52;
    if (hold3 > 0) {
      uint16_t dec = (uint16_t)(hold3 * 3);
      ampB = (dec >= 18) ? 34 : (uint16_t)(52 - dec);
      if (nextHas3 && ampB > 4) ampB -= 4;
    }
    voice_set(V_BASS, NOTE_FREQ[bass], 42, ampB);
  } else {
    voice_off(V_BASS);
  }

  if (n4 >= 0) {
    uint16_t amp4 = 34;
    uint8_t duty4 = 40;
    if (hold4 > 0) {
      uint16_t dec = (uint16_t)(hold4 * 2);
      amp4 = (dec >= 12) ? 22 : (uint16_t)(34 - dec);
      duty4 = 36;
      if (nextHas4) {
        if (amp4 > 3) amp4 -= 3;
        duty4 = 30;
      }
    }
    voice_set(V_PAD, NOTE_FREQ[n4], duty4, amp4);
    int8_t arp = (n4 + 12 < 60) ? (int8_t)(n4 + 12) : n4;
    uint16_t arpAmp = (hold4 > 0) ? 16 : 22;
    voice_set(V_ARPEGGIO, NOTE_FREQ[arp], 50, arpAmp);
  } else {
    voice_off(V_PAD);
    voice_off(V_ARPEGGIO);
  }

  pos++;
  if (pos >= mlen) {
    pos = 0;
    measure = (uint8_t)((measure + 1) % 29);
    track5Active = false;
    track4Active = false;
    track3Active = false;
    hold5 = 0;
    hold4 = 0;
    hold3 = 0;
    n5 = -1;
    n4 = -1;
    n3 = -1;
  }
}

void sound_update() {
  static uint32_t chord_start = 0;
  static uint32_t chord_dur = 0;
  static uint8_t chord_idx = 0;
  static uint32_t arp_next = 0;
  static uint8_t arp_step = 0;
  static uint32_t mel_next = 0;
  static uint32_t mel_off_time = 0;
  static bool mel_active = false;
  static float tempo_scale = 1.0f;
  static uint32_t tempo_change_at = 0;

  uint32_t now = millis();

  apply_sfx_if_needed();

  if (!music_enabled) {
    voice_off(V_MELODY);
    voice_off(V_BASS);
    voice_off(V_ARPEGGIO);
    voice_off(V_PAD);
    delay(4);
    return;
  }

  if (music_mode == SOUND_MUSIC_TETRIS) {
    run_tetris_music();
    delay(4);
    return;
  }

  if (now >= tempo_change_at) {
    tempo_scale = 0.12f + (float)(mrand() % 13) * 0.48f;
    tempo_change_at = now + 5000 + (mrand() % 25001);
  }

  if (now - chord_start >= chord_dur) {
    for (int i = 0; i < 4; i++) voice_off(i);
    delay(20);
    chord_idx = (uint8_t)(mrand() % 4);
    chord_start = millis();
    chord_dur = (uint32_t)((1600 + (mrand() % 1000)) * tempo_scale);
    arp_step = 0;
    arp_next = chord_start;
    mel_next = chord_start + (uint32_t)((mrand() % 300) * tempo_scale);
    mel_active = false;
    const Chord &ch = CHORDS[chord_idx];
    voice_set(V_BASS, NOTE_FREQ[ch.bass], 25, 55);
    voice_set(V_PAD, NOTE_FREQ[ch.pad], 12, 28);
  }

  const Chord &ch = CHORDS[chord_idx];
  now = millis();

  if (mel_active && now >= mel_off_time) {
    voice_off(V_MELODY);
    mel_active = false;
    mel_next = now + (uint32_t)((25 + (mrand() % 75)) * tempo_scale);
  }

  if (!mel_active && now >= mel_next) {
    uint8_t ni = ch.mel[mrand() % 6];
    if (mrand() % 5 == 0) {
      ni += 12;
      if (ni >= 60) ni -= 12;
    }
    voice_set(V_MELODY, NOTE_FREQ[ni], 50, 45);
    mel_active = true;
    mel_off_time = now + (uint32_t)((80 + (mrand() % 220)) * tempo_scale);
  }

  if (now >= arp_next) {
    uint8_t ni = ch.arp[arp_step % 3];
    if (arp_step % 6 >= 3) {
      ni += 12;
      if (ni >= 60) ni -= 12;
    }
    voice_set(V_ARPEGGIO, NOTE_FREQ[ni], 50, 38);
    arp_step++;
    arp_next = now + (uint32_t)((130 + (mrand() % 100)) * tempo_scale);
  }

  delay(4);
}

static void sfx_start(uint16_t freq, uint8_t duty, uint16_t amp, uint16_t ms) {
  noInterrupts();
  sfx.freq = freq;
  sfx.duty = duty;
  sfx.amp = amp;
  sfx.endAt = millis() + ms;
  sfx.active = true;
  interrupts();
}

void sound_play_ui_tick() {
  sfx_start(988, 45, 70, 35);
}

void sound_play_ui_confirm() {
  sfx_start(1319, 50, 85, 55);
}

void sound_play_success() {
  sfx_start(1760, 50, 95, 90);
}

void sound_play_error() {
  sfx_start(220, 55, 90, 85);
}

void sound_play_tetris_lock() {
  sfx_start(659, 45, 80, 28);
}

void sound_play_tetris_line() {
  sfx_start(1319, 50, 95, 75);
}

void sound_play_tetris_game_over() {
  sfx_start(165, 60, 95, 180);
}
