#define BTN1_LED 23
#define BTN2_LED 21
#define BTN3_LED 18
#define BTN4_LED 17
#define BTN5_LED 4
#define BTN6_LED 2


#include <Arduino.h>

#include <Wire.h>
#include <U8x8lib.h>

#define SIM_NB_PLAY_BUTTONS 6
char buttons_led_gpio[] PROGMEM={BTN1_LED,BTN2_LED,BTN3_LED,BTN4_LED,BTN5_LED,BTN6_LED};

#include <WiFi.h>
#include <FS.h>
#include <SD.h>
#include "AudioFileSourcePROGMEM.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

AudioGeneratorMP3 *mp3;
AudioFileSourcePROGMEM *file;
AudioOutputI2S *out;

U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(/* clock=*/ 32, /* data=*/ 33, /* reset=*/ U8X8_PIN_NONE);   // OLEDs without Reset of the Display

extern const uint8_t beep1_mp3_start[] asm("_binary_src_data_beep1_mp3_start");
extern const uint8_t beep1_mp3_end[] asm("_binary_src_data_beep1_mp3_end");
extern const uint8_t beep2_mp3_start[] asm("_binary_src_data_beep2_mp3_start");
extern const uint8_t beep2_mp3_end[] asm("_binary_src_data_beep2_mp3_end");
extern const uint8_t beep3_mp3_start[] asm("_binary_src_data_beep3_mp3_start");
extern const uint8_t beep3_mp3_end[] asm("_binary_src_data_beep3_mp3_end");
extern const uint8_t beep4_mp3_start[] asm("_binary_src_data_beep4_mp3_start");
extern const uint8_t beep4_mp3_end[] asm("_binary_src_data_beep4_mp3_end");
extern const uint8_t beep5_mp3_start[] asm("_binary_src_data_beep5_mp3_start");
extern const uint8_t beep5_mp3_end[] asm("_binary_src_data_beep5_mp3_end");
extern const uint8_t beep6_mp3_start[] asm("_binary_src_data_beep6_mp3_start");
extern const uint8_t beep6_mp3_end[] asm("_binary_src_data_beep6_mp3_end");

const uint8_t *buttons_beep_data[] PROGMEM={beep1_mp3_start,beep2_mp3_start,beep3_mp3_start,beep4_mp3_start,beep5_mp3_start,beep6_mp3_start};
const int buttons_beep_data_size[] PROGMEM={beep1_mp3_end-beep1_mp3_start,beep2_mp3_end-beep2_mp3_start,beep3_mp3_end-beep3_mp3_start,beep4_mp3_end-beep4_mp3_start,beep5_mp3_end-beep5_mp3_start,beep6_mp3_end-beep6_mp3_start};

TaskHandle_t task_audio;
SemaphoreHandle_t audioSemaphore; 
void task_audioLoop(void *parameter);


void task_audioLoop(void *parameter) {
  for (;;) {
    vTaskDelay(10/portTICK_PERIOD_MS);

    xSemaphoreTake(audioSemaphore, portMAX_DELAY);
    if (mp3) {
      if (mp3->isRunning()) {
        if (!mp3->loop()) mp3->stop();
      } else {
        file->close();
        delete file;
        delete mp3;
        file=NULL;
        mp3=NULL;
      }
    }
    xSemaphoreGive(audioSemaphore);
    
  }
}

void set_buttonLED(int btn_index,bool btn_led_on) {
  if ((btn_index<0)||(btn_index>SIM_NB_PLAY_BUTTONS-1)) return;
  if (btn_led_on) digitalWrite(buttons_led_gpio[btn_index],HIGH);
  else digitalWrite(buttons_led_gpio[btn_index],LOW);
}

void play_buttonBeep(int btn_index) {
  if ((btn_index<0)||(btn_index>SIM_NB_PLAY_BUTTONS-1)) return;

  xSemaphoreTake(audioSemaphore, portMAX_DELAY);
  if (mp3) {
    mp3->stop();
    file->close();
    delete file;
    delete mp3;
    file=NULL;
    mp3=NULL;
  }

  file = new AudioFileSourcePROGMEM( buttons_beep_data[btn_index], buttons_beep_data_size[btn_index] );
  mp3 = new AudioGeneratorMP3();
  mp3->begin(file, out);
  xSemaphoreGive(audioSemaphore);
}

void play_buttonLEDBeep(int btn_index) {
  set_buttonLED(btn_index,1);
  play_buttonBeep(btn_index);
  delay(500);
  set_buttonLED(btn_index,0);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  
  pinMode(BTN1_LED, OUTPUT);
  pinMode(BTN2_LED, OUTPUT);
  pinMode(BTN3_LED, OUTPUT);
  pinMode(BTN4_LED, OUTPUT);
  pinMode(BTN5_LED, OUTPUT);
  pinMode(BTN6_LED, OUTPUT);
  //digitalWrite(10, 0);
  //digitalWrite(9, 0);		
  
  u8x8.begin();
  u8x8.setPowerSave(0);

  audioLogger = &Serial;
  out = new AudioOutputI2S(0,0,16,0);
  out->SetPinout(26,27,25);
  out->SetGain(0.04f);
  
  audioSemaphore = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(
    task_audioLoop, /* Function to implement the task */
    "Task audio", /* Name of the task */
    4096,  /* Stack size in words */
    NULL,  /* Task input parameter */
    9,  /* Priority of the task */
    &task_audio,  /* Task handle. */
    1); /* Core where the task should run */
}

void loop() {
  static int cnt=0;
  static int i=0;
  cnt++;

  if ((cnt%50)==0) {
    u8x8.clearDisplay();
    u8x8.setFont(u8x8_font_chroma48medium8_r);
    u8x8.drawString(rand()%16,rand()%16,"Hello World!");  
  }

  if ((cnt%50)==0) {
    play_buttonLEDBeep((i++)%SIM_NB_PLAY_BUTTONS);//random(SIM_NB_PLAY_BUTTONS));
  }

  delay(10);  
}