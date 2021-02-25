#include <Arduino.h>
#include "globals.h"
#include "vgmFunctions.h"

#include "ym2612.hpp"
extern "C" {
#include "sn76489.h"
}
uint8_t *vgm;
uint32_t vgmpos = 0x40;
bool vgmend = false;
uint32_t vgmloopoffset;
uint32_t datpos;
uint32_t pcmpos;
uint32_t pcmoffset;

uint32_t clock_sn76489;
uint32_t clock_ym2612;

SN76489_Context *sn76489;

int **buflr;
int vgmGen_vol=32; //0-255


uint8_t get_vgm_ui8()
{
    return vgm[vgmpos++];
}

uint16_t get_vgm_ui16()
{
    return get_vgm_ui8() + (get_vgm_ui8() << 8);
}

uint32_t get_vgm_ui32()
{
    return get_vgm_ui8() + (get_vgm_ui8() << 8) + (get_vgm_ui8() << 16) + (get_vgm_ui8() << 24);
}

uint16_t parse_vgm()
{
    uint8_t command;
    uint16_t wait = 0;
    uint8_t reg;
    uint8_t dat;

    command = get_vgm_ui8();
    switch (command) {
        case 0x50:
            dat = get_vgm_ui8();
            SN76489_Write(sn76489, dat);
            break;
        case 0x52:
        case 0x53:
            reg = get_vgm_ui8();
            dat = get_vgm_ui8();
            YM2612_Write(0 + ((command & 1) << 1), reg);
            YM2612_Write(1 + ((command & 1) << 1), dat);
            break;
        case 0x61:
            wait = get_vgm_ui16();
            break;
        case 0x62:
            wait = 735;
            break;
        case 0x63:
            wait = 882;
            break;
        case 0x66:
            if(vgmloopoffset == 0) {
                vgmend = true;
            } else {
                vgmpos = vgmloopoffset;
            }
            break;
        case 0x67:
            get_vgm_ui8(); // 0x66
            get_vgm_ui8(); // 0x00 data type
            datpos = vgmpos + 4;
            vgmpos += get_vgm_ui32(); // size of data, in bytes
            break;
        case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
        case 0x78: case 0x79: case 0x7a: case 0x7b: case 0x7c: case 0x7d: case 0x7e: case 0x7f:
            wait = (command & 0x0f) + 1;
            break;
        case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x86: case 0x87:
        case 0x88: case 0x89: case 0x8a: case 0x8b: case 0x8c: case 0x8d: case 0x8e: case 0x8f:
            wait = (command & 0x0f);
            YM2612_Write(0, 0x2a);
            YM2612_Write(1, vgm[datpos + pcmpos + pcmoffset]);
            pcmoffset++;
            break;
        case 0xe0:
            pcmpos = get_vgm_ui32();
            pcmoffset = 0;
            break;
        default:
            printf("unknown cmd at 0x%x: 0x%x\n", vgmpos, vgm[vgmpos]);
            vgmpos++;
            break;
    }

	return wait;
}

void vgmGen_setVolume(int vol) {
    vgmGen_vol=vol;
}

// The setup routine runs once when M5Stack starts up
int vgmGen_init(uint8_t *vgmdata,int length,int sndbuffSmplSize) {
    // Load vgm data
    vgm = vgmdata;

    // read vgm header
    vgmpos = 0x0C; clock_sn76489 = get_vgm_ui32();
    vgmpos = 0x2C; clock_ym2612 = get_vgm_ui32();
    vgmpos = 0x1c; vgmloopoffset = get_vgm_ui32();
    vgmpos = 0x34; vgmpos = 0x34 + get_vgm_ui32();

    if(clock_ym2612 == 0) clock_ym2612 = 7670453;
    if(clock_sn76489 == 0) clock_sn76489 = 3579545;

    printf("clock_sn76489 : %d\n", clock_sn76489);
    printf("clock_ym2612 : %d\n", clock_ym2612);
    printf("vgmpos : %x\n", vgmpos);

    // init sound chip
    sn76489 = SN76489_Init(clock_sn76489, 44100);
    SN76489_Reset(sn76489);
    YM2612_Init(clock_ym2612, 44100, 0);    

    // malloc sound buffer
    

    buflr = (int **)malloc(sizeof(int *) * 2);
    buflr[0] = (int *)malloc(sndbuffSmplSize * sizeof(int));
    if(buflr[0] == NULL) printf("pcm buffer0 alloc fail.\n");
    buflr[1] = (int *)malloc(sndbuffSmplSize * sizeof(int));
    if(buflr[1] == NULL) printf("pcm buffer1 alloc fail.\n");

    return 0;
}

// The loop routine runs over and over again forever
int vgmGen_fillbufferMono(int16_t *buffer,int bytesLength) {    
    static int32_t last_frame_size=0;
    int32_t update_frame_size;
    int32_t smpl;
    int16_t *sndPtr=buffer;
    int buffer_to_fill=bytesLength/2;
        
    while (buffer_to_fill) {

        //Is end reached ?
        if (vgmend) return 0;

        //Do we need new frame data
        if (!last_frame_size) last_frame_size = parse_vgm();

        if(last_frame_size > buffer_to_fill) {
            update_frame_size = buffer_to_fill;
        } else {
            update_frame_size = last_frame_size;
        }
        // get sampling
        SN76489_Update(sn76489, (int **)buflr, update_frame_size);
        YM2612_Update((int **)buflr, update_frame_size);
        YM2612_DacAndTimers_Update((int **)buflr, update_frame_size);
        for(uint32_t i = 0; i < update_frame_size; i++) {
            smpl=((buflr[0][i]+buflr[1][i])*vgmGen_vol)>>9;
            if (smpl<-32768) smpl=-32768;
            else if (smpl>32767) smpl=32767;
            *sndPtr++=smpl;            
        }
        last_frame_size -= update_frame_size;
        buffer_to_fill-= update_frame_size;
    }
    return bytesLength-buffer_to_fill*2;
}

void vgmGen_close() {
    YM2612_End();
    SN76489_Shutdown(sn76489);
    free(buflr[0]);
    free(buflr[1]);
    free(buflr);    
}
