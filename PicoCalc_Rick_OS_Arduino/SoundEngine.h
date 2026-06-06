#ifndef SOUND_ENGINE_H
#define SOUND_ENGINE_H

enum SoundMusicMode {
	SOUND_MUSIC_AMBIENT = 0,
	SOUND_MUSIC_TETRIS = 1
};

void sound_begin();
void sound_update();
void sound_set_music_enabled(bool enabled);
void sound_set_music_mode(SoundMusicMode mode);

void sound_play_ui_tick();
void sound_play_ui_confirm();
void sound_play_success();
void sound_play_error();
void sound_play_tetris_lock();
void sound_play_tetris_line();
void sound_play_tetris_game_over();

#endif // SOUND_ENGINE_H
