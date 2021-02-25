#ifndef _vgmFunctions_H
#define _vgmFunctions_H

int vgmGen_init(uint8_t *vgmdata,int length,int sndbuffSmplSize);
int vgmGen_fillbufferMono(int16_t *buffer,int smplLength);
void vgmGen_setVolume(int vol);
void vgmGen_close();

#endif