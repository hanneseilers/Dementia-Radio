/*
 * Playing files from SD Card (or web stream, if wanted)
 * Requires ESP32 libraries installed: https://github.com/espressif/arduino-esp32/releases/latest/
 */
#include <WiFi.h>
#include <Ticker.h>
#include <SD.h>
#include <FS.h>


/*
 * Slightly modified version of I2S library: https://github.com/schreibfaul1/ESP32-audioI2S
 * Replace original src files with the ones attached to this project
 */
#include <Audio.h>


// ---- CONFIG ----

//#define WLAN_ON

#define WLAN_SSID               "VK Werkstatt"
#define WLAN_PW                 "2x3m4wu3m9!"
#define WIFI_CONNECT_MAX_TRIES  5

#define FILES_MAX_SKIP          3
#define FILES_PLAY_STARTING     0
#define VOLUME_TICK_MS          100
#define VOLUME_START            21

#define SWITCH_ON_TICK_MS       100

#define ADC_RESOLUTION  12      // BIT
#define ADC_MAX         4096.0  // 2 ^ADC_RESOLUTION (double!)
// ---- CONFIG END ----


// Digital I/O used
#define SD_CS          5
#define SPI_MOSI      23
#define SPI_MISO      19
#define SPI_SCK       18

#define I2S_DOUT      25
#define I2S_BCLK      27
#define I2S_LRC       26

#define ADC_VOLUME    35
#define SWITCH_ON     14



Audio audio;
File root;
int nFile;
Ticker tickerVolume;
Ticker tickerSwitch;
uint8_t lastVolume;
bool deviceOn;





/*
 * SETUP
 */
void setup() {
  deviceOn = false;
  pinMode(SWITCH_ON, INPUT);
  
  // Serial.println("");
  Serial.begin(115200);
  Serial.println("starting setup");

  // configure sleep & wakeup
  esp_sleep_enable_ext0_wakeup( GPIO_NUM_14, 1);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON); // all RTC Peripherals are powered

  // if wakeup was caused by switch on
  if( getWakeupReason() ){

    // configure sd card
    Serial.print("enable SD card ...");
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    if( !SD.begin(SD_CS) ){
      Serial.println("[ERR]: Cannot access SD card!");
    } else {
      Serial.println(" done.");
    }
  
    // try connecting to wifi
    #ifdef WLAN_ON
      WiFi.disconnect();
      WiFi.mode(WIFI_STA);
      WiFi.begin(WLAN_SSID, WLAN_PW);
  
      Serial.print("connecting to wifi ...");
      uint8_t n = 0;
      while( WiFi.status() != WL_CONNECTED && n < WIFI_CONNECT_MAX_TRIES ){
        Serial.print("...");
        n++;
        delay(1000);
      }
      if( WiFi.status() != WL_CONNECTED ){
        Serial.println(" done.");
      } else {
        Serial.println(" not connected!");
        Serial.println( WiFi.status() );
      }
    #else
      Serial.println("-- WiFi disabled");
    #endif
  
    // enable audio
    Serial.println("init audio");
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    lastVolume = VOLUME_START;
    audio.setVolume(lastVolume);
  
    // ADC init
    analogReadResolution(ADC_RESOLUTION);

    Serial.println("> starting.");
    Serial.flush(); 
    deviceOn = true;
    startPlaying();    
    
    
  } else {
    gotoSleep();
  }
}

/*
 * MAIN LOOP
 */
void loop() {
  audio.loop();
}

/**
 * Set ESP into deep sleep mode
 */
void gotoSleep(){
  Serial.println("---- Going to sleep ----");
  Serial.flush(); 
  WiFi.disconnect();
  esp_deep_sleep_start();
}

/*
 * Returns true if wakeup raised by switched on or other than timer or power up, false otherwise.
 */
bool getWakeupReason(){

  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason){
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); return true;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); return true;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); return false;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); return true;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); return true;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;    
  }

  return false;
  
}

/*
 * Starts playing audio files
 */
void startPlaying(){
  // play audio
  nFile = FILES_PLAY_STARTING;
  playNextFile();

  // call ticker
  checkVolume();
  checkSwitch();
}

/*
 * Stops playing files
 */
void stopPlaying(){
  Serial.println("-- stopped playing --");
  audio.stopSong();
  deviceOn = false;
  gotoSleep();
}



/*
 * Calls once the volume ticker
 */
void checkVolume(){
  if( deviceOn ){
    tickerVolume.once_ms(VOLUME_TICK_MS, onVolume);
  }
}

/*
 * Calls once ther switch ticker
 */
void checkSwitch() {
  if( deviceOn ){
    tickerSwitch.once_ms(SWITCH_ON_TICK_MS, onSwitch);
  }
}


/*
 * Volume ticker callback
 * Checks if volume level changed
 */
void onVolume() {
  double adcValue = analogRead(ADC_VOLUME);
  //Serial.println( adcValue );
  uint8_t volume = ( (adcValue / ADC_MAX) * 20 ) + 1;
  
  if( (lastVolume >= volume && lastVolume-volume > 1) || (volume > lastVolume && volume-lastVolume > 1) ){
  //if( lastVolume != volume ){
    audio.setVolume( volume );
    Serial.print("new volume ");
    Serial.println(volume);
    lastVolume = volume;    
  }
  
  checkVolume();
}


/*
 * Switch ticker callback
 * Checks if device is switched on/off
 */
void onSwitch() {
  if( !digitalRead(SWITCH_ON) ){
    stopPlaying();
  } else {
    checkSwitch();
  }
}

/*
 * Playing next file from SD card
*/
void playNextFile(){
  uint8_t skip = 0;

  // skip missing files
  while( skip < FILES_MAX_SKIP ){
    
    String filename = "/" + String(nFile) + ".mp3";
    Serial.println("> trying to find " + filename);
  
    if( !SD.exists(filename) ){

      // try next file number
      skip++;
      nFile++;
      Serial.println("[ERR]: no file named " + filename );
      
    } else {

      // open file
      File entry = SD.open( filename, FILE_READ );
      nFile++;

      // print file info
      Serial.print( filename );
      Serial.print( "\t- size[kB]: ");
      Serial.println(entry.size() / 1024.0, DEC);
  
      // play file
      audio.connecttoSD( filename );
  
      entry.close();
      break;
    
    }

  }
  
}

/*
 * Displays general audio info
 */
void audio_info(const char *info){
  Serial.print("[info]: "); Serial.println(info);
}

/*
 * Displays ID3 tags infos
 */
void audio_id3data(const char *info){  //id3 metadata
    Serial.print("[id3data]:");Serial.println(info);
}

/*
 * Called, if end of file is reached
 */
void audio_eof_mp3(const char *info){  //end of file
    Serial.print("[eof_mp3]: ");Serial.println(info);
    playNextFile();
}
