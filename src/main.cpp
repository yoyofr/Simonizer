#define BUTTON_LONG_PRESS_THRESHOLD 512
#define SIMON_MAX_LEVEL 10
#define SIMON_DEFAULT_VOLUME 2
#define SIMON_DEFAULT_HIGHSCORE 0
#define SIMON_MAX_SEQUENCE 64
#define SIMON_NB_PLAY_BUTTONS 6


#define VBAT_IN 35

#define BTN1_LED 23
#define BTN2_LED 21
#define BTN3_LED 18
#define BTN4_LED 12//14//17
#define BTN5_LED 4
#define BTN6_LED 2


#define BTN1_IN 22
#define BTN2_IN 19
#define BTN3_IN 5
#define BTN4_IN 34//12 //16
#define BTN5_IN 0
#define BTN6_IN 15
#define BTNCMD_IN 13


#define FORMAT_LITTLEFS_IF_FAILED false

enum {
  SIMON_MODE_WELCOME_ANIM,
  SIMON_MODE_ATTRACT,
  SIMON_MODE_IDLE,
  SIMON_MODE_STD_GAME,
  SIMON_MODE_STD_LEVEL_CLEAR,
  SIMON_MODE_STD_FAILED,
  SIMON_MODE_STD_GAMEOVER,
  SIMON_MODE_LOST,
  SIMON_MODE_END_HIGHSCORE,
  SIMON_MODE_DEBUG_BUTTON=99
};

#define SIMON_MODE_STARTMODE SIMON_MODE_WELCOME_ANIM //SIMON_MODE_DEBUG_BUTTON

enum {
  SIMON_GAME_TURN_SIMON,
  SIMON_GAME_TURN_PLAYER
};
enum {
  SIMON_LEDANIM_WELCOME,
  SIMON_LEDANIM_LOST,
  SIMON_LEDANIM_NEWHIGHSCORE,
  SIMON_LEDANIM_ENDNEWHIGHSCORE
};



#include <Arduino.h>
#include "globals.h"
#include <Wire.h>
//#include <U8x8lib.h>
#include <U8g2lib.h>

char btn_led_gpio[] PROGMEM={BTN1_LED,BTN2_LED,BTN3_LED,BTN4_LED,BTN5_LED,BTN6_LED};

#include <FS.h>
#include <LITTLEFS.h>
#include "audioFunctions.h"


//U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(/* clock=*/ 32, /* data=*/ 33, /* reset=*/ U8X8_PIN_NONE);   // OLEDs without Reset of the Display
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ 32, /* data=*/ 33, /* reset=*/ U8X8_PIN_NONE);   // All Boards without Reset of the Display


extern const uint8_t music_hs_start[] asm("_binary_progdata_tetris_gb_final_nsf_start");
extern const uint8_t music_hs_end[]   asm("_binary_progdata_tetris_gb_final_nsf_end");
extern const uint8_t music_smb_start[] asm("_binary_progdata_smb_nsf_start");
extern const uint8_t music_smb_end[]   asm("_binary_progdata_smb_nsf_end");

extern const uint8_t music_test1_start[] asm("_binary_progdata_ym2612_vgm_start");
extern const uint8_t music_test1_end[]   asm("_binary_progdata_ym2612_vgm_end");
extern const uint8_t music_test2_start[] asm("_binary_progdata_sn76489_vgm_start");
extern const uint8_t music_test2_end[]   asm("_binary_progdata_sn76489_vgm_end");
extern const uint8_t music_test3_start[] asm("_binary_progdata_test8_vgm_start");
extern const uint8_t music_test3_end[]   asm("_binary_progdata_test8_vgm_end");
extern const uint8_t music_test4_start[] asm("_binary_progdata_test9_vgm_start");
extern const uint8_t music_test4_end[]   asm("_binary_progdata_test9_vgm_end");



extern const uint8_t beep1_raw_start[] asm("_binary_progdata_beep1_raw_start");
extern const uint8_t beep1_raw_end[] asm("_binary_progdata_beep1_raw_end");
extern const uint8_t beep2_raw_start[] asm("_binary_progdata_beep2_raw_start");
extern const uint8_t beep2_raw_end[] asm("_binary_progdata_beep2_raw_end");
extern const uint8_t beep3_raw_start[] asm("_binary_progdata_beep3_raw_start");
extern const uint8_t beep3_raw_end[] asm("_binary_progdata_beep3_raw_end");
extern const uint8_t beep4_raw_start[] asm("_binary_progdata_beep4_raw_start");
extern const uint8_t beep4_raw_end[] asm("_binary_progdata_beep4_raw_end");
extern const uint8_t beep5_raw_start[] asm("_binary_progdata_beep5_raw_start");
extern const uint8_t beep5_raw_end[] asm("_binary_progdata_beep5_raw_end");
extern const uint8_t beep6_raw_start[] asm("_binary_progdata_beep6_raw_start");
extern const uint8_t beep6_raw_end[] asm("_binary_progdata_beep6_raw_end");



const uint8_t *btn_beep_data[] PROGMEM={beep1_raw_start,beep2_raw_start,beep3_raw_start,beep4_raw_start,beep5_raw_start,beep6_raw_start};
const int btn_beep_data_size[] PROGMEM={beep1_raw_end-beep1_raw_start,beep2_raw_end-beep2_raw_start,beep3_raw_end-beep3_raw_start,beep4_raw_end-beep4_raw_start,beep5_raw_end-beep5_raw_start,beep6_raw_end-beep6_raw_start};

const char *btn_beep_mp3file[] PROGMEM={"/beep1.mp3","/beep2.mp3","/beep3.mp3","/beep4.mp3","/beep5.mp3","/beep6.mp3"};
int btn_playingBtn;
char btn_playingBtn_channel;
int btn_pressed=-1;
int btn_lastPressed=-1;


int btn_cmd_pressed;

const char *message_buttonOK[] PROGMEM={"Oui","C'est bien","Bravo","Continue","Pas mal","C'est ca","Tu y es presque","Yes","Super"};
const char *message_buttonKO[] PROGMEM={"Non","Dommage","Perdu","Entraine toi","Bouh!","Et non!","Pas de bol"};
const char *message_nextSequence[]={"Voila la suite","Ca va se corser","Reste concentré","Attention","Tu fatigues ?","Encore un de plus"};
int simon_gameSpeed[SIMON_MAX_LEVEL] PROGMEM={800,700,600,500,400,300,200,150,100,50};


const float simon_sndVolTable[] PROGMEM={0,64,128,255};
const char *simon_sndVolLabel[] PROGMEM={"SON OFF","SON MIN","SON MOYEN","SON MAX"};
char simon_sndVolume;

char player_sequenceTarget[SIMON_MAX_SEQUENCE];
char player_currentSequenceIndex,player_targetSequenceIndex;
char player_level,player_turn,player_highscore;
char player_newhighscore;



char simon_mainMode=SIMON_MODE_STARTMODE;
long simon_currentModeElapsed=0;
void simon_stdGameTurn(int player_buttonPressed);


int display_width,display_height;
enum {
  MSG_ALIGN_LEFT=0,
  MSG_ALIGN_CENTER=1,
  MSG_ALIGN_RIGHT=2
};




void button_stopCurrent();
void display_message(char line,const char *msg,char alignement);
void display_splashScreen();
void display_clear();
void display_score();
void display_highscore();
void display_gameover();
void display_update();
void display_clearLeds();
void display_batteryLevel();

void set_buttonLED(int btn_index,bool btn_led_on);

void simon_startGame();
void simon_EndNewHighscore();
void simon_HighscoreBeaten();
void simon_Lost();


void display_animateLed(char anim_index,bool reset_cnt) {
  static int cnt=0;
  if (reset_cnt) cnt=0;
  switch (anim_index) {
    case SIMON_LEDANIM_WELCOME:
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
      break;
    case SIMON_LEDANIM_LOST:
      if (cnt<64) {
        for (int i=0;i<SIMON_NB_PLAY_BUTTONS;i++) {
          set_buttonLED(i,1);
        }
      } else if (cnt<64*2) {
        for (int i=0;i<SIMON_NB_PLAY_BUTTONS;i++) {
          set_buttonLED(i,(((cnt-64)*SIMON_NB_PLAY_BUTTONS/64)>=i?0:1));
        }
      }
      break;
    case SIMON_LEDANIM_NEWHIGHSCORE:
      if (cnt<64) {
        for (int i=0;i<SIMON_NB_PLAY_BUTTONS;i++) {
          set_buttonLED(i,((cnt-64)/16)&1);
        }
      } else if (cnt<64*2+6) {
      } else if (cnt<64*4) {
        for (int i=0;i<SIMON_NB_PLAY_BUTTONS;i++) {
          set_buttonLED(i,((cnt-64*2-6)/16)&1);
        }
      }
      break;
    case SIMON_LEDANIM_ENDNEWHIGHSCORE:
      if (cnt<64) {
        for (int i=0;i<SIMON_NB_PLAY_BUTTONS;i++) {
          set_buttonLED(i,0);
        }
      } else if (cnt<64*2) {
        for (int i=0;i<SIMON_NB_PLAY_BUTTONS;i++) {
          set_buttonLED(i,(((cnt-64)*SIMON_NB_PLAY_BUTTONS/64)>=i?1:0));
        }
      } else if (cnt<64*4) {
        for (int i=0;i<SIMON_NB_PLAY_BUTTONS;i++) {
          set_buttonLED(i,((cnt-64*2)/8)&1);
        }
      }
      break;
  }
  cnt++;
}

void simon_HighscoreBeaten() {
  audio_startVgm(music_smb_start, music_smb_end-music_smb_start,31,1000,0);
  
  display_clear();
  display_highscore();
  display_batteryLevel();
  display_message(3,"Highscore battu!",MSG_ALIGN_CENTER);
  display_score();
  display_update();

  display_animateLed(SIMON_LEDANIM_NEWHIGHSCORE,true);
  while (audio_isChannelActive(AUDIO_BGMUS_CHANNEL)) {    
    vTaskDelay(10);
    display_animateLed(SIMON_LEDANIM_NEWHIGHSCORE,false);
  }
  display_clearLeds();
}

void simon_welcomeMusic() {
  //audio_startVgm(music_smb_start, music_smb_end-music_smb_start,25,1200,0);
  audio_startVgm(music_test4_start, music_test4_end-music_test4_start,25,1200,1);
}

void simon_EndNewHighscore() {  
  audio_startVgm(music_hs_start, music_hs_end-music_hs_start,4,60000,0);

  display_clear();
  display_highscore();
  display_batteryLevel();
  display_score();
  display_message(3,"Nouveau highscore!",MSG_ALIGN_CENTER);
  display_update();
  
  display_animateLed(SIMON_LEDANIM_ENDNEWHIGHSCORE,true);  
}

void simon_Lost() {
  audio_startVgm(music_smb_start, music_smb_end-music_smb_start,14,3000,0);

  display_clear();
  display_highscore();
  display_batteryLevel();
  display_score();
  display_gameover();
  display_message(4,message_buttonKO[random(sizeof(message_buttonKO)/sizeof(char*))],MSG_ALIGN_CENTER);        
  display_update();

  display_animateLed(SIMON_LEDANIM_LOST,true);  
}

void set_buttonLED(int btn_index,bool btn_led_on) {
  if ((btn_index<0)||(btn_index>SIMON_NB_PLAY_BUTTONS-1)) return;
  if (btn_led_on) digitalWrite(btn_led_gpio[btn_index],HIGH);
  else digitalWrite(btn_led_gpio[btn_index],LOW);
}

void play_buttonBeep(int btn_index) {
  audio_startFx(AUDIO_FX_CHANNEL1,(int16_t*)btn_beep_data[btn_index],btn_beep_data_size[btn_index],0,0);
  btn_playingBtn_channel=AUDIO_FX_CHANNEL1;
  button_stopCurrent();
  btn_playingBtn=btn_index;  
}

void button_playBtn(int btn_index) {
  play_buttonBeep(btn_index);
  set_buttonLED(btn_index,1);
}

void button_stopCurrent() {
  if (btn_playingBtn>=0) {
    set_buttonLED(btn_playingBtn,false);          
    btn_playingBtn=-1;
  }
}



void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  esp_random();

  simon_sndVolume=SIMON_DEFAULT_VOLUME;
  btn_cmd_pressed=0;
  btn_playingBtn=-1;
  player_highscore=SIMON_DEFAULT_HIGHSCORE;

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
  pinMode(BTNCMD_IN, INPUT_PULLUP);	
  
  u8g2.begin();
  //u8g2.setPowerSave(0);

  audio_init();
  audio_updateVolume(simon_sndVolTable[simon_sndVolume]);
  
  u8g2.setFont(u8g2_font_profont10_tf);//u8g2_font_ncenB08_tr);
  u8g2.setFontMode(0);
  display_width=u8g2.getDisplayWidth();
  display_height=u8g2.getDisplayHeight();

  display_clear();  
  display_splashScreen();
  display_update();

  delay(1000);
  display_clear();
  display_highscore();
  display_batteryLevel();
  display_update();  

  simon_welcomeMusic();
}


void display_clear() {
  u8g2.clearBuffer();  
}

void display_update() {
  u8g2.sendBuffer();
}

void display_splashScreen() {
  u8g2.drawStr((display_width-u8g2.getStrWidth("SIMONizer"))>>1,4*7+7,"SIMONizer");    
}

void display_gameover() {
  u8g2.drawStr((display_width-u8g2.getStrWidth("GAME OVER"))>>1,2*7+7,"GAME OVER");  
}

void display_clearLeds() {
  for (int i=0;i<SIMON_NB_PLAY_BUTTONS;i++) {
    set_buttonLED(i,0);
  }
}
void display_highscore() {
  char strtmp[32];
  u8g2.setCursor(0,0);
  sprintf(strtmp,"HIGHSCORE: %d",player_highscore);   
  u8g2.drawStr(0,7,strtmp);
  
}

void display_message(char line,const char *msg,char alignement) {
  int w;
  switch (alignement) {
    case 1: //center
      w=u8g2.getStrWidth(msg);
      u8g2.drawStr((display_width-w)>>1,(line+1)*7,msg);
      break;
    case 2: //right
      w=u8g2.getStrWidth(msg);
      u8g2.drawStr(display_width-w,(line+1)*7,msg);  
      break;
    case 0://left
    default:
      u8g2.drawStr(0,(line+1)*7,msg);  
      break;
  }
}

void display_score() {
  char strtmp[32];
  sprintf(strtmp,"Score: %d",player_targetSequenceIndex-1);
  u8g2.drawStr(0,7*7,strtmp);
}

void simon_gameEvents(){
  
}

void input_check() {
  
  btn_lastPressed=btn_pressed;

  btn_pressed=-1;
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

  if (digitalRead(BTNCMD_IN)==0) {
    btn_cmd_pressed++;
    if (btn_cmd_pressed>BUTTON_LONG_PRESS_THRESHOLD) {
      //reset
      btn_cmd_pressed=0;
      display_clear();    
      display_message(3,"BYE BYE!",MSG_ALIGN_CENTER);
      display_update();
      delay(1000);
      ESP.restart();
    }
  } else {
    if (btn_cmd_pressed) {
      //cmd => change volume
      simon_sndVolume++;
      if (simon_sndVolume>=sizeof(simon_sndVolTable)/sizeof(float)) simon_sndVolume=0;
      audio_updateVolume(simon_sndVolTable[simon_sndVolume]);
      display_clear();    
      display_message(3,simon_sndVolLabel[simon_sndVolume],MSG_ALIGN_CENTER);
      display_update();
      delay(300);      
    }
    btn_cmd_pressed=0;
  }
}

void simon_stdGameTurn(int player_buttonPressed) {
  if (player_turn==SIMON_GAME_TURN_SIMON) {
    ////////////////////////////
    //play new sequence    
    ////////////////////////////
    display_clear();
    display_highscore();
    display_batteryLevel();
    display_message(3,message_nextSequence[random(sizeof(message_nextSequence)/sizeof(char*))],MSG_ALIGN_CENTER);
    display_score();
    display_update();
    for (int i=0;i<player_targetSequenceIndex;i++) {      
      delay(simon_gameSpeed[player_level]);
      button_stopCurrent();
      button_playBtn(player_sequenceTarget[i]);      
    }
    ////////////////////////////////////////////////////////
    //restart player position to beginning of sequence
    //now it is player turn
    ////////////////////////////////////////////////////////
    player_currentSequenceIndex=0;    
    player_turn=SIMON_GAME_TURN_PLAYER;
    display_clear();
    display_highscore();
    display_batteryLevel();
    display_message(3,"A ton tour",MSG_ALIGN_CENTER);
    display_score();
    display_update();
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
        if (player_currentSequenceIndex==player_targetSequenceIndex) {
          if (player_highscore<player_targetSequenceIndex) {
            player_highscore=player_targetSequenceIndex;
            if (!player_newhighscore) simon_HighscoreBeaten();
            player_newhighscore=player_highscore;            
          }          
          player_targetSequenceIndex++;          
          player_turn=SIMON_GAME_TURN_SIMON;          
          display_clear();
          display_highscore();
          display_batteryLevel();
          display_message(3,message_buttonOK[random(sizeof(message_buttonOK)/sizeof(char*))],MSG_ALIGN_CENTER);
          display_score();
          display_update();
          delay(500);          
        } else {
          display_clear();
          display_highscore();
          display_batteryLevel();
          display_score();
          display_update();
        }
      } else {
        ////////////////////////////////////////////////////////
        //if not, game is lost, go back to welcome screen
        ////////////////////////////////////////////////////////                
        if (player_newhighscore) {
          simon_mainMode=SIMON_MODE_END_HIGHSCORE;
          simon_EndNewHighscore();
        } else {
          simon_mainMode=SIMON_MODE_LOST;
          simon_Lost();
        }
      }
    }
  }
}

void simon_startGame() {
  simon_mainMode=SIMON_MODE_STD_GAME;
  simon_currentModeElapsed=0;

  display_clear();
  display_highscore();
  display_batteryLevel();
  display_message(3,"Preparation",MSG_ALIGN_CENTER);
  display_update();
  for (int i=0;i<SIMON_MAX_SEQUENCE;i++) player_sequenceTarget[i]=random(SIMON_NB_PLAY_BUTTONS);
  player_currentSequenceIndex=0;        
  player_targetSequenceIndex=1;
  player_level=0;
  player_newhighscore=0;
  player_turn=SIMON_GAME_TURN_SIMON;
}

#define BATTERY_READ_NB_SAMPLES 16
int bat_reads[BATTERY_READ_NB_SAMPLES];
int bat_read_cnt=0;

void display_batteryLevel() {
  long bat_read_avg;

  //read a new value and add it in the array
  bat_reads[bat_read_cnt++]=analogRead(VBAT_IN);
  
  if (bat_read_cnt>=BATTERY_READ_NB_SAMPLES) bat_read_cnt=0;
  //compute sum
  bat_read_avg=0;
  for (int i=0;i<BATTERY_READ_NB_SAMPLES;i++) {
    bat_read_avg+=bat_reads[i];
  }
  //map to 0/100
  char strtmp[16];
  float val_bat=map(bat_read_avg,0,4095*BATTERY_READ_NB_SAMPLES,0,330)*2.195/100.0f;
  sprintf(strtmp,"%.2fv",val_bat);
  u8g2.drawStr(display_width-u8g2.getStrWidth(strtmp),7,strtmp);  
}

void loop() {
  static int cnt=0;
  static long lastMillis=millis();
  long curMillis=millis();
  cnt++;

  if (!(cnt&2047)) DBG_PRINT_RAMAVAIL
  
 
  input_check();

   /////////////////////////////////
  // Game Logic
  ///////////////////////////////
  //check if current button should be stopped
  if (btn_playingBtn>=0) {
    //a button is playing
    if (!audio_isChannelActive(btn_playingBtn_channel)) button_stopCurrent();
  }
  
  simon_currentModeElapsed+=curMillis-lastMillis;
  
  switch (simon_mainMode) {
    case SIMON_MODE_DEBUG_BUTTON:
        if ((btn_pressed>=0)&&(btn_pressed!=btn_lastPressed)) button_playBtn(btn_pressed);
        break;
    case SIMON_MODE_WELCOME_ANIM:
    // WELCOME Animation, stop at 1st press
      if ((btn_pressed>=0)&&(btn_pressed!=btn_lastPressed)) {  
        audio_stopVgm();    
        display_clearLeds();
        button_stopCurrent();
        simon_startGame();        

        delay(500);
                
        display_clear();
        display_highscore();
        display_batteryLevel();
        display_update();
      } else {            
        if (!(cnt&3)) display_animateLed(SIMON_LEDANIM_WELCOME,false);
        if ((cnt&127)==0) {
          display_clear();
          display_highscore();
          display_batteryLevel();
          display_message(3,"APPUIE POUR",MSG_ALIGN_CENTER);
          display_message(5,"JOUER",MSG_ALIGN_CENTER);        
          display_update();
        } else if ((cnt&127)==127-32)  {
          display_clear();
          display_highscore();
          display_batteryLevel();
          display_update();        
        }
      }
      break;
    case SIMON_MODE_LOST:      
      if (((btn_pressed>=0)&&(btn_pressed!=btn_lastPressed))||(!audio_isChannelActive(AUDIO_BGMUS_CHANNEL))) {
        // Jingle finished - go back to welcome anim
        audio_stopVgm();
        display_clearLeds();
        simon_mainMode=SIMON_MODE_WELCOME_ANIM;
      } else display_animateLed(SIMON_LEDANIM_LOST,false);      
      break;
    case SIMON_MODE_END_HIGHSCORE:
      if (((btn_pressed>=0)&&(btn_pressed!=btn_lastPressed))||(!audio_isChannelActive(AUDIO_BGMUS_CHANNEL))) {
        // Jingle finished - go to lost anim
        audio_stopVgm();
        display_clearLeds();
        simon_mainMode=SIMON_MODE_LOST;
        simon_Lost();
      } else {        
        display_animateLed(SIMON_LEDANIM_ENDNEWHIGHSCORE,false);      
      }
      break;
    case SIMON_MODE_STD_GAME:
      //process standard game turn
        if (btn_pressed==btn_lastPressed) simon_stdGameTurn(-1); // if long_press, pass -1 to avoid double registration of button press
        else simon_stdGameTurn(btn_pressed);
      break;
    default:
      break;
  }
 

  simon_gameEvents();

  delay(5);
}