#define BTN1_LED 23
#define BTN2_LED 21
#define BTN3_LED 18
#define BTN4_LED 17
#define BTN5_LED 4
#define BTN6_LED 2


#define BTN1_IN 22
#define BTN2_IN 19
#define BTN3_IN 5
#define BTN4_IN 16
#define BTN5_IN 0
#define BTN6_IN 15
#define BTNRESET_IN 13

#define FORMAT_LITTLEFS_IF_FAILED false

#include <Arduino.h>

#include <Wire.h>
#include <U8x8lib.h>

#define SIMON_MAX_SEQUENCE 64
#define SIMON_NB_PLAY_BUTTONS 6
char buttons_led_gpio[] PROGMEM={BTN1_LED,BTN2_LED,BTN3_LED,BTN4_LED,BTN5_LED,BTN6_LED};

#include <WiFi.h>
#include <FS.h>
#include <LITTLEFS.h>
#include <SD.h>
#include "AudioFileSourceFS.h"
#include "AudioFileSourcePROGMEM.h"
#include "AudioGeneratorMP3.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"

AudioGeneratorMP3 *mp3;
AudioGeneratorMP3 *mp3_beep;
AudioFileSourceFS *file;
AudioFileSourceFS *fileMP3;
AudioOutputI2S *out;

U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(/* clock=*/ 32, /* data=*/ 33, /* reset=*/ U8X8_PIN_NONE);   // OLEDs without Reset of the Display

//extern const uint8_t music_mp3_start[] asm("_binary_src_data_music_mp3_start");
//extern const uint8_t music_mp3_end[] asm("_binary_src_data_music_mp3_end");

//const uint8_t *buttons_beep_data[] PROGMEM={beep1_wav_start,beep2_wav_start,beep3_wav_start,beep4_wav_start,beep5_wav_start,beep6_wav_start};
//const int buttons_beep_data_size[] PROGMEM={beep1_wav_end-beep1_wav_start,beep2_wav_end-beep2_wav_start,beep3_wav_end-beep3_wav_start,beep4_wav_end-beep4_wav_start,beep5_wav_end-beep5_wav_start,beep6_wav_end-beep6_wav_start};

const char *buttons_beep_mp3file[] PROGMEM={"/beep1.mp3","/beep2.mp3","/beep3.mp3","/beep4.mp3","/beep5.mp3","/beep6.mp3"};
int button_playingBtn;

#define SIMON_MAX_LEVEL 10

int simon_gameSpeed[SIMON_MAX_LEVEL]={800,700,600,500,400,300,200,150,100,50};

char player_sequenceTarget[SIMON_MAX_SEQUENCE];
char player_currentSequenceIndex,player_targetSequenceIndex;
char player_level,player_turn,player_highscore;


#define SIMON_WELCOME_ANIM_TIMEOUT 5*1000
enum {
  SIMON_MODE_WELCOME_ANIM,
  SIMON_MODE_ATTRACT,
  SIMON_MODE_IDLE,
  SIMON_MODE_STD_GAME,
  SIMON_MODE_STD_LEVEL_CLEAR,
  SIMON_MODE_STD_FAILED,
  SIMON_MODE_STD_GAMEOVER
};
enum {
  SIMON_GAME_TURN_SIMON,
  SIMON_GAME_TURN_PLAYER
};

char simon_mainMode=SIMON_MODE_WELCOME_ANIM;
long simon_currentModeElapsed=0;
void simon_stdGameTurn(int player_buttonPressed);

TaskHandle_t task_audio;
SemaphoreHandle_t audioSemaphore; 
void task_audioLoop(void *parameter);

void button_stopCurrent();
void display_message(char *msg);
void simon_startGame();

void set_buttonLED(int btn_index,bool btn_led_on) {
  if ((btn_index<0)||(btn_index>SIMON_NB_PLAY_BUTTONS-1)) return;
  if (btn_led_on) digitalWrite(buttons_led_gpio[btn_index],HIGH);
  else digitalWrite(buttons_led_gpio[btn_index],LOW);
}

void play_buttonBeep(int btn_index) {
  if ((btn_index<0)||(btn_index>SIMON_NB_PLAY_BUTTONS-1)) return;

  xSemaphoreTake(audioSemaphore, portMAX_DELAY);
  button_stopCurrent();
  if (mp3_beep) {    
    mp3_beep->stop();
    file->close();
    delete file;
    delete mp3_beep;
    file=NULL;
    mp3_beep=NULL;    
  }
  file = new AudioFileSourceFS(LITTLEFS,buttons_beep_mp3file[btn_index]);// PROGMEM( buttons_beep_data[btn_index], buttons_beep_data_size[btn_index] );
  mp3_beep = new AudioGeneratorMP3() ;//WAV();
  mp3_beep->begin(file, out);
  xSemaphoreGive(audioSemaphore);
  button_playingBtn=btn_index;
}

void button_playBtn(int btn_index) {
  //Do not repeat same button if already taken into account (wait for end of mp3_beep/led off)
  //if (btn_index==button_playingBtn) return; 
  set_buttonLED(btn_index,1);
  play_buttonBeep(btn_index);
}

void button_stopCurrent() {
  if (button_playingBtn>=0) {
    set_buttonLED(button_playingBtn,false);          
    button_playingBtn=-1;
  }
}

void task_audioLoop(void *parameter) {
  for (;;) {
    vTaskDelay(5/portTICK_PERIOD_MS);

    xSemaphoreTake(audioSemaphore, portMAX_DELAY);
    if (mp3_beep) {
      if (mp3_beep->isRunning()) {
        if (!mp3_beep->loop()) {          
          mp3_beep->stop();          
        }        
      } else {
        file->close();
        delete file;
        delete mp3_beep;
        file=NULL;
        mp3_beep=NULL;
        button_stopCurrent();
      }
    }
    if (mp3) {
      if (mp3->isRunning()) {
        if (!mp3->loop()) mp3->stop();
      } else {
        fileMP3->close();
        delete fileMP3;
        delete mp3;
        fileMP3=NULL;
        mp3=NULL;
      }
    }
    xSemaphoreGive(audioSemaphore);
    
  }
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  esp_random();

  button_playingBtn=-1;
  player_highscore=0;

  if(!LITTLEFS.begin(FORMAT_LITTLEFS_IF_FAILED)){
        Serial.println("LITTLEFS Mount Failed");
        return;
  }
  
  pinMode(BTN1_LED, OUTPUT);
  pinMode(BTN2_LED, OUTPUT);
  pinMode(BTN3_LED, OUTPUT);
  pinMode(BTN4_LED, OUTPUT);
  pinMode(BTN5_LED, OUTPUT);
  pinMode(BTN6_LED, OUTPUT);

  pinMode(BTN1_IN, INPUT_PULLUP);
  pinMode(BTN2_IN, INPUT_PULLUP);
  pinMode(BTN3_IN, INPUT_PULLUP);
  pinMode(BTN4_IN, INPUT_PULLUP);
  pinMode(BTN5_IN, INPUT_PULLUP);
  pinMode(BTN6_IN, INPUT_PULLUP);
  pinMode(BTNRESET_IN, INPUT_PULLUP);	
  
  u8x8.begin();
  //u8x8.setPowerSave(0);

  audioLogger = &Serial;
  out = new AudioOutputI2S(0,0,8,0);
  out->SetPinout(26,27,25);
  out->SetGain(0.26);
  
  mp3=NULL;
  //fileMP3 = new AudioFileSourceFS(LITTLEFS, "/music.mp3" );
  //mp3 = new AudioGeneratorMP3();
  //mp3->begin(fileMP3, out);
  
  audioSemaphore = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(
    task_audioLoop, /* Function to implement the task */
    "Task audio", /* Name of the task */
    4096,  /* Stack size in words */
    NULL,  /* Task input parameter */
    1,  /* Priority of the task */
    &task_audio,  /* Task handle. */
    0); /* Core where the task should run */

  u8x8.setFont(u8x8_font_chroma48medium8_r);
    
  display_message("Ok go");
}

void display_clearLeds() {
  for (int i=0;i<SIMON_NB_PLAY_BUTTONS;i++) {
    set_buttonLED(i,0);
  }
}

void display_welcomeAnim() {
  static int cnt=0;

  //simon_currentModeElapsed    
  if (cnt<10*(SIMON_NB_PLAY_BUTTONS<<4)) {
    for (int i=0;i<SIMON_NB_PLAY_BUTTONS;i++) {
      set_buttonLED(i,(i==((cnt>>4)%SIMON_NB_PLAY_BUTTONS)?1:0));
    }  
  } else if (cnt<20*(SIMON_NB_PLAY_BUTTONS<<4)) {
    for (int i=0;i<SIMON_NB_PLAY_BUTTONS;i++) {
      set_buttonLED(i,((SIMON_NB_PLAY_BUTTONS-i-1)==((cnt>>4)%SIMON_NB_PLAY_BUTTONS)?1:0));
    }  
  } else if (cnt<30*(SIMON_NB_PLAY_BUTTONS<<4)){
    for (int i=0;i<SIMON_NB_PLAY_BUTTONS;i++) {
      set_buttonLED(i,((cnt>>6)&1?1:0));
    }  
  } else cnt=0;
  cnt++;
  
}

void display_message(char *msg) {
  u8x8.clear();
  u8x8.setCursor(0,0);
  u8x8.print("SIMONizer");
  u8x8.setCursor(0,1);
  u8x8.printf("Highscore: %d",player_highscore);
  u8x8.setCursor(0,3);
  u8x8.print(msg);  
}

void input_check() {
  int btn_pressed=-1;
  static int btn_lastPressed=-1;
  if (digitalRead(BTN1_IN)==0) {
    btn_pressed=0;
  } else if (digitalRead(BTN2_IN)==0) {
    btn_pressed=1;
  } else if (digitalRead(BTN3_IN)==0) {
    btn_pressed=2;
  } else if (digitalRead(BTN4_IN)==0) {
    btn_pressed=3;
  } else if (digitalRead(BTN5_IN)==0) {
    btn_pressed=4;
  } else if (digitalRead(BTN6_IN)==0) {
    btn_pressed=5;
  }

  if (digitalRead(BTNRESET_IN)==0) {
    display_message("BYE BYE!");
    delay(1000);
    ESP.restart();
  }

/////////////////////////////////
// Game Logic
///////////////////////////////
static long lastMillis=millis();
long currentMillis=millis();
  
  simon_currentModeElapsed+=currentMillis-lastMillis;
  switch (simon_mainMode) {
    case SIMON_MODE_WELCOME_ANIM:
    // WELCOME Animation, stop at 1st press or after SIMON_WELCOME_ANIM_TIMEOUT seconds
      if (btn_pressed>=0) {      
        display_clearLeds();
        button_stopCurrent();        
        simon_startGame();
        button_stopCurrent();
        delay(1000);
      }
      break;
    case SIMON_MODE_STD_GAME:
    //process standard game turn
      if (btn_pressed==btn_lastPressed) simon_stdGameTurn(-1); // if long_press, pass -1 to avoid double registration of button press
      else simon_stdGameTurn(btn_pressed);
      break;
  }
  lastMillis=currentMillis;
  
  /*if (btn_pressed==btn_lastPressed) {
    //Long press
  } else {
    if (btn_pressed>=0) {
      //a button has been pressed
      button_playBtn(btn_pressed);    
    }
  }*/
  btn_lastPressed=btn_pressed;
}

void simon_stdGameTurn(int player_buttonPressed) {
  if (player_turn==SIMON_GAME_TURN_SIMON) {
    ////////////////////////////
    //play new sequence    
    ////////////////////////////
    display_message("Sois attentif");
    for (int i=0;i<player_targetSequenceIndex;i++) {      
      button_playBtn(player_sequenceTarget[i]);
      delay(simon_gameSpeed[player_level]);
      button_stopCurrent();
    }
    ////////////////////////////////////////////////////////
    //restart player position to beginning of sequence
    //now it is player turn
    ////////////////////////////////////////////////////////
    player_currentSequenceIndex=0;    
    player_turn=SIMON_GAME_TURN_PLAYER;
    display_message("A toi de jouer");
  } else if (player_turn==SIMON_GAME_TURN_PLAYER) {
    ////////////////////////////////////////////////////////
    //player turn
    ////////////////////////////////////////////////////////
    if (player_buttonPressed>=0) {
      ///////////////////////////
      // Play pushed button 
      ///////////////////////////
      button_playBtn(player_buttonPressed);
      ////////////////////////////////////////////////////////
      // now check button / sequence
      ////////////////////////////////////////////////////////
      if (player_buttonPressed==player_sequenceTarget[player_currentSequenceIndex]) {
        ////////////////////////////////////////////////////////
        //if expected button, progress in sequence
        ////////////////////////////////////////////////////////
        player_currentSequenceIndex++;
        if (player_currentSequenceIndex==1) display_message("Oui, continue"); //1st correct button
        if (player_currentSequenceIndex==player_targetSequenceIndex) {
          if (player_highscore<player_targetSequenceIndex) player_highscore=player_targetSequenceIndex;
          player_targetSequenceIndex++;
          
          player_turn=SIMON_GAME_TURN_SIMON; 
          display_message("Bravo");
          delay(1000);
        }
      } else {
        ////////////////////////////////////////////////////////
        //if not, game is lost, go back to welcome screen
        ////////////////////////////////////////////////////////                
        simon_mainMode=SIMON_MODE_WELCOME_ANIM;
        display_message("Perdu");
        delay(1000);
      }
    }
  }
}

void simon_startGame() {
  simon_mainMode=SIMON_MODE_STD_GAME;
  simon_currentModeElapsed=0;

  display_message("C'est parti");
  
  for (int i=0;i<SIMON_MAX_SEQUENCE;i++) player_sequenceTarget[i]=random(SIMON_NB_PLAY_BUTTONS);
  player_currentSequenceIndex=0;        
  player_targetSequenceIndex=1;
  player_level=0;
  player_turn=SIMON_GAME_TURN_SIMON;
}

void loop() {
  static int cnt=0;
  cnt++;

  input_check();

  switch (simon_mainMode) {
    case SIMON_MODE_WELCOME_ANIM:
      display_welcomeAnim();
      break;
    default:
      break;
  }

  delay(10);  
}