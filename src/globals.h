#ifndef _globals_H
#define _globals_H

#define DBG_PRINT_RAMAVAIL {Serial.printf("Head: %dko/%dko | PSRAM: %dko/%dko\n", ESP.getFreeHeap()/1024,ESP.getHeapSize()/1024,ESP.getFreePsram()/1024,ESP.getPsramSize()/1024);Serial.flush();}

#endif
