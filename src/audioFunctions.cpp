#include <Arduino.h>
#include "audioFunctions.h"

#include "driver/i2s.h"

int audio_vol=64;

// VGM & NSF decoder
#include "vgm_file.h"
#include "vgmFunctions.h"

VgmFile *m_vgm;
char m_vgm_opened;

int16_t sndBuffer[MAX_AUDIO_CHANNELS][AUDIO_BUFFSIZE_SMPL]; //mono
int16_t sndBufferI2S[AUDIO_BUFFSIZE_SMPL*2]; //stereo
byte *sndBufferI2ScurrentPos;
size_t audio_bytes_written;

TaskHandle_t task_audio;
SemaphoreHandle_t audioSemaphore; 

void task_audioLoop(void *parameter);

audio_channel_t audio_channels[MAX_AUDIO_CHANNELS];


// Volume: 0 / 255
void audio_updateVolume(int vol) {
    xSemaphoreTake(audioSemaphore, portMAX_DELAY);
    audio_vol=vol;
    xSemaphoreGive(audioSemaphore);
}

void audio_stopVgm() {
    switch (m_vgm_opened) {
        case 1:
            Serial.println("stop vgm");
            m_vgm_opened=0;
            m_vgm->close();
            delete m_vgm;
            m_vgm=NULL;
        break;
        case 2:
            Serial.println("stop vgmGen");
            vgmGen_close();
            m_vgm_opened=0;
        break;
    }        
}


void audio_startVgm(const uint8_t *buffer, int size,int start_track,int max_duration,char isGen) {    
    xSemaphoreTake(audioSemaphore, portMAX_DELAY);
    audio_stopVgm();
    if (isGen) {
        vgmGen_init((uint8_t*)buffer,size,AUDIO_BUFFSIZE_SMPL);
        vgmGen_setVolume(16);
        m_vgm_opened=2;
    } else {
        m_vgm = new VgmFile();    
        m_vgm->setSampleFrequency(I2S_SAMPLERATE);
        m_vgm->open( buffer, size );    
        if (start_track>=0) m_vgm->setTrack(start_track);
        if (max_duration>0) m_vgm->setMaxDuration(max_duration);
        m_vgm->setFading( true );    
        m_vgm->setVolume( 10 ); //10%        
        m_vgm_opened=1;
    }
    audio_channels[AUDIO_BGMUS_CHANNEL].status=AUDIO_CHANNEL_ACTIVE;
    xSemaphoreGive(audioSemaphore);
}

void audio_init() {
    // Audio channels & snd buffers
    for (int i=0;i<MAX_AUDIO_CHANNELS;i++) {
        memset(sndBuffer[i],0,AUDIO_BUFFSIZE_SMPL*2);

        memset(&(audio_channels[i]),0,sizeof(audio_channel_t));
    }
    sndBufferI2ScurrentPos=(byte*)&sndBufferI2S;

    // VGM
    m_vgm_opened=0;    

    // Set I2S Audio config
    i2s_config_t i2s_config_dac = {
          .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
          .sample_rate = I2S_SAMPLERATE,
          .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
          .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
          .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
          .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // lowest interrupt priority
          .dma_buf_count = I2S_DMA_BUFF_NB,
          .dma_buf_len = I2S_DMA_BUFF_LENGTH,
          .use_apll = 1 // Use audio PLL
    };
    // Install I2S Driver
    if (i2s_driver_install((i2s_port_t)I2S_PORTNB, &i2s_config_dac, 0, NULL) != ESP_OK) {
        Serial.println("Pb init i2s audio\n");
    }
    // Set pins
    i2s_pin_config_t pins = {
        .bck_io_num = BCK_PIN,
        .ws_io_num = WS_PIN,
        .data_out_num = DOUT_PIN,
        .data_in_num = I2S_PIN_NO_CHANGE};
    i2s_set_pin((i2s_port_t)I2S_PORTNB, &pins);

    i2s_zero_dma_buffer((i2s_port_t)I2S_PORTNB);


  
    audioSemaphore = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(
        task_audioLoop, /* Function to implement the task */
        "Task audio", /* Name of the task */
        8192,  /* Stack size in words */
        NULL,  /* Task input parameter */
        1,  /* Priority of the task */
        &task_audio,  /* Task handle. */
    0); /* Core where the task should run */
}

//fill audio channels with current active fx/music
void audio_processChannels() {
    for (int i=0;i<MAX_AUDIO_CHANNELS;i++) {
        if (audio_channels[i].status==AUDIO_CHANNEL_ACTIVE) {
            int to_copy=AUDIO_BUFFSIZE_SMPL*2;
            byte *sndPtr=(byte*)(sndBuffer[i]);
            
            while (to_copy) {
                if (audio_channels[i].status==AUDIO_CHANNEL_STOPPED) {
                    //////////////////////////
                    // Channel stopped
                    memset(sndPtr,0,to_copy);
                    to_copy=0;
                } else if (i==AUDIO_BGMUS_CHANNEL) {
                    //////////////////////////
                    // VGM
                    int bytes_available=0;
                    switch (m_vgm_opened) {
                        case 1:
                            bytes_available = m_vgm->decodePcmMono(sndPtr, to_copy);
                            break;
                        case 2:
                            bytes_available = vgmGen_fillbufferMono((int16_t *)sndPtr,to_copy);
                            break;
                    }
                    if ( bytes_available <= 0 ) {                        
                        audio_stopVgm();
                        audio_channels[AUDIO_BGMUS_CHANNEL].status=AUDIO_CHANNEL_STOPPED;
                    }
                    sndPtr+=bytes_available;
                    to_copy-=bytes_available;
                } else if (audio_channels[i].loop) {
                    //////////////////////////
                    // FX - loop
                    if (audio_channels[i].current_pos<audio_channels[i].src_size) {
                        //copy available data, not exceeding buffer size
                        int bytes_available=audio_channels[i].src_size-audio_channels[i].current_pos;
                        if (bytes_available>to_copy) {
                            memcpy(sndPtr,(char*)(audio_channels[i].src_data)+audio_channels[i].current_pos,to_copy);                            
                            audio_channels[i].current_pos+=to_copy;
                            to_copy=0;
                        } else {                        
                            memcpy(sndPtr,(char*)(audio_channels[i].src_data)+audio_channels[i].current_pos,bytes_available);
                            audio_channels[i].current_pos+=bytes_available;
                            if (audio_channels[i].current_pos>=audio_channels[i].src_size) {
                                //reached end
                                audio_channels[i].current_pos=0;
                                if (audio_channels[i].loop_tgt>0) {
                                    //not infinite loop
                                    audio_channels[i].loop_cur++;
                                    if (audio_channels[i].loop_cur>audio_channels[i].loop_tgt) {
                                        //loop target reached, stop looping
                                        audio_channels[i].status=AUDIO_CHANNEL_STOPPED;
                                    }
                                }
                            }
                            to_copy-=bytes_available;
                            sndPtr+=bytes_available;                            
                        }                    
                    }
                } else {
                    //////////////////////////
                    // FX - No loop
                    if (audio_channels[i].current_pos<audio_channels[i].src_size) {
                        //copy available data, not exceeding buffer size
                        int bytes_available=audio_channels[i].src_size-audio_channels[i].current_pos;
                        if (bytes_available>to_copy) {
                            memcpy(sndPtr,(char*)(audio_channels[i].src_data)+audio_channels[i].current_pos,to_copy);
                            audio_channels[i].current_pos+=to_copy;
                            to_copy=0;
                        } else {                            
                            memcpy(sndPtr,(char*)(audio_channels[i].src_data)+audio_channels[i].current_pos,bytes_available);
                            audio_channels[i].current_pos+=bytes_available;
                            if (audio_channels[i].current_pos>=audio_channels[i].src_size) {
                                //reached end
                                audio_channels[i].current_pos=0;
                                //loop target reached, stop looping
                                audio_channels[i].status=AUDIO_CHANNEL_STOPPED;
                            }
                            to_copy-=bytes_available;
                            sndPtr+=bytes_available;
                        }
                    }
                }
            }
        } else memset((byte*)(sndBuffer[i]),0,AUDIO_BUFFSIZE_SMPL*2);
    }
}

void audio_processBuffers() {
    esp_err_t ret;
    if (sndBufferI2ScurrentPos==(byte*)sndBufferI2S) {
        //last buffer was processed, initiate a new one
        int16_t *sndPtr=sndBufferI2S;
        for (int i=0;i<AUDIO_BUFFSIZE_SMPL;i++) {        
            *sndPtr++=((int)(sndBuffer[0][i]+sndBuffer[1][i])*audio_vol)>>9;
            *sndPtr++=((int)(sndBuffer[2][i]+sndBuffer[3][i])*audio_vol)>>9;
        }
        ret=i2s_write((i2s_port_t)I2S_PORTNB, (const char *)sndBufferI2ScurrentPos, AUDIO_BUFFSIZE_SMPL*2*2, &audio_bytes_written, 0);
        if (audio_bytes_written<AUDIO_BUFFSIZE_SMPL*2*2) {
            sndBufferI2ScurrentPos+=audio_bytes_written;
        }
        audio_processChannels();
    } else {
        //retry last buffer
        int remainingBytes=AUDIO_BUFFSIZE_SMPL*2*2-(sndBufferI2ScurrentPos-(byte*)sndBufferI2S);
        ret=i2s_write((i2s_port_t)I2S_PORTNB, (const char *)sndBufferI2ScurrentPos, remainingBytes, &audio_bytes_written, 0);
        if (audio_bytes_written==remainingBytes) {
            sndBufferI2ScurrentPos=(byte*)sndBufferI2S;
        } else {
            sndBufferI2ScurrentPos+=audio_bytes_written;
        }
    }        
}

// size should be in bytes, data expected mono 16bits signed pcm
void audio_startFx(char channel,int16_t *src_data,int src_size,char start_pos,int8_t loop_tgt) {
    audio_channels[channel].src_data=src_data;
    audio_channels[channel].src_size=src_size;
    audio_channels[channel].current_pos=start_pos;
    if (loop_tgt) {
        audio_channels[channel].loop=true;
        audio_channels[channel].loop_tgt=loop_tgt;        
        audio_channels[channel].loop_cur=0;
    } else audio_channels[channel].loop=false;
        
    audio_channels[channel].status=AUDIO_CHANNEL_ACTIVE;
}

bool audio_isChannelActive(char channel) {
    return (audio_channels[channel].status==AUDIO_CHANNEL_ACTIVE);
}

void task_audioLoop(void *parameter) {
  int snd_active;
  for (;;) {
    vTaskDelay(1/portTICK_PERIOD_MS);

    xSemaphoreTake(audioSemaphore, portMAX_DELAY);

    audio_processBuffers();
        
    xSemaphoreGive(audioSemaphore);    
  }
}
