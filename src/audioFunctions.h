#ifndef _audioFunctions_H
#define _audioFunctions_H

#define BCK_PIN 26
#define WS_PIN 27
#define DOUT_PIN 25


#define MAX_AUDIO_CHANNELS 4
#define AUDIO_FX_CHANNEL1 0
#define AUDIO_FX_CHANNEL2 1
#define AUDIO_FX_CHANNEL3 2
#define AUDIO_BGMUS_CHANNEL 3
#define AUDIO_BUFFSIZE_SMPL 1024

#define I2S_DMA_BUFF_LENGTH 1024
#define I2S_DMA_BUFF_NB 2
#define I2S_SAMPLERATE 44100
#define I2S_PORTNB 0

extern SemaphoreHandle_t audioSemaphore; 

enum {
    AUDIO_CHANNEL_STOPPED=0,
    AUDIO_CHANNEL_ACTIVE
};

typedef struct {
    int16_t *src_data;
    int src_size;
    int current_pos;
    bool loop;
    int8_t loop_tgt;
    int8_t loop_cur;
    int8_t status;
} audio_channel_t;



void audio_init();
void audio_updateVolume();
void audio_startFx(char channel,int16_t *src_data,int src_size,char start_pos,int8_t loop_tgt);
bool audio_isChannelActive(char channel);
void audio_startVgm(const uint8_t *buffer, int size,int start_track,int max_duration);

#endif