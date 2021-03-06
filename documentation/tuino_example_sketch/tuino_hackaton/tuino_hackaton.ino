/* _____  _____  __  __             _____  _____ 
  / ____||_   _||  \/  |    /\     / ____||_   _|
 | |  __   | |  | \  / |   /  \   | (___    | |  
 | | |_ |  | |  | |\/| |  / /\ \   \___ \   | |  
 | |__| | _| |_ | |  | | / ____ \  ____) | _| |_ 
  \_____||_____||_|  |_|/_/    \_\|_____/ |_____|
  (c) 2018 GIMASI SA                                               

 * tuino_hackaton.ino
 *
 *  Created on: March 15th, 2018
 *      Author: Massimo Santoli
 *      Brief: Example Sketch to use NBIoT GMX-NB Module on T-Mobile Austria Network
 *      Version: 1.0
 *
 *      License: it's free - do whatever you want! ( provided you leave the credits)
 *
 */
 
#include "gmx_nbiot.h"
#include "SeeedOLED.h"
#include "display_utils.h"
#include <Wire.h>

long int timer_period_to_tx = 20000;
long int timer_millis_tx = 0;

unsigned char devAddress[8] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

unsigned char  txBuffer[256];

int tempSensorPin = A0;

// Temperature Sensor constants
const int B = 4275;               // B value of the thermistor
const int R0 = 100000;          // R0 = 100k

#define TEMPERATURE_SAMPLING    (20)
#define THERMOSTAT_HYSTERESIS   (1.0f)

float current_temperature = 0.0f;
float temp_temperature = 0;
int temperature_counts = 0;



float readTemp() {
  int a = analogRead(tempSensorPin );

  float R = 1023.0 / ((float)a) - 1.0;
  R = 100000.0 * R;

  float temperature = 1.0 / (log(R / 100000.0) / B + 1 / 298.15) - 273.15; //convert to temperature via datasheet ;

  return temperature;
}

void setup() {

  String version;
  String imei;
  char string[32];
  byte join_status;
  int join_wait=0;
  unsigned char imei_buffer[32];

  Wire.begin();
  Serial.begin(9600);
  Serial.println("Starting");

  SeeedOled.init();  //initialze SEEED OLED display

  SeeedOled.clearDisplay();          //clear the screen and set start position to top left corner
  SeeedOled.setNormalDisplay();      //Set display to normal mode (i.e non-inverse mode)
  SeeedOled.setHorizontalMode();           //Set addressing mode to Page Mode

  // Show Splash Screen on OLED
  splashScreen();
  delay(5000);
  
  // Init NB IoT board
  gmxNB_init("34.248.246.47","7070");
  


  gmxNB_getVersion(version);
  Serial.println("GMXNB Version:"+version);

  gmxNB_getIMEI(imei);
  Serial.println("GMXNB IMEI:"+imei);

  imei = "0"+imei;
  imei.toCharArray(imei_buffer, imei.length()); 

  for (int i=0;i<8;i++)
  {
    uint8_t high,low;

    high = (imei_buffer[i*2]-0x30) * 16;
    low =  (imei_buffer[(i*2)+1]-0x30);

    devAddress[i] = high + low;
  }

  
  gmxNB_start(); 

  SeeedOled.clearDisplay();
  SeeedOled.setTextXY(0, 0);
  SeeedOled.putString("Attaching...");


  while((join_status = gmxNB_isNetworkJoined()) != NB_NETWORK_JOINED) {
    gmxNB_Led2(GMXNB_LED_ON);
    delay(500);
    gmxNB_Led2(GMXNB_LED_OFF);
    Serial.print("Waiting to connect:");
    Serial.println(join_wait);

    SeeedOled.setTextXY(1, 0);
    sprintf(string, "Attempt: %d", join_wait);
    SeeedOled.putString(string);
    
    join_wait++;
    
    delay(2500);
  }
  
  Serial.println("Connected!!!");
  gmxNB_Led2(GMXNB_LED_ON);

  SeeedOled.setTextXY(2, 0);
  SeeedOled.putString("Attached!");
  
  // Init Temperature
  current_temperature = readTemp();

  delay(2000);
  SeeedOled.clearDisplay();
}


// Ubirch packet Functions

void sendUbirchPacket(char *data_to_send)
{
    unsigned char ubirch_packet[512];
    char payload[500];
    int payload_len;
    
    String nbiot_payload;
    char string_byte[3];
    
    ubirch_packet[0] = 0x00;
    ubirch_packet[1] = 0xCE;
    
    
    ubirch_packet[2] = devAddress[4];
    ubirch_packet[3] = devAddress[5];
    ubirch_packet[4] = devAddress[6];
    ubirch_packet[5] = devAddress[7];

    ubirch_packet[6] = 0xD9;

    Serial.println("Sending Data to Ubirch");
    strcpy(payload,data_to_send);
    Serial.println(payload);
    payload_len = strlen( payload );

    Serial.print("Payload Len:");
    Serial.println(payload_len);
    
    ubirch_packet[7] = payload_len;

    for(int i=0;i<payload_len;i++ )
      ubirch_packet[8+i] = payload[i];

  /* 
   *  Debug
   */

 for (int i=0;i<payload_len+8;i++)
  {  
    sprintf(string_byte,"%02x",ubirch_packet[i]);
    Serial.print((char)ubirch_packet[i]);
    Serial.print("=");
    Serial.print(ubirch_packet[i],HEX);
    Serial.print("/");
    Serial.println(string_byte);
    
    
    
    nbiot_payload = nbiot_payload + string_byte;
  } 

  Serial.print("NBIoT Payload:");
  Serial.println(nbiot_payload);
  
  /*
   * End Debug
   */

   
    if ( gmxNB_TXData(nbiot_payload) == GMXNB_OK )
      Serial.println("TX OK");
    else
      Serial.println("TX KO");


 }


void sendUbirchInt(char *key, int value)
{
   char payload[500];
   sprintf(payload,"{\"%s\":%d}",key,value);

   sendUbirchPacket(payload);
}


void sendUbirchText(char *key, char *string)
{
   char payload[500];
   sprintf(payload,"{\"%s\":\"%s\"}",key,string);

   sendUbirchPacket(payload);

}


void loop() {

  long int delta_tx;
  int temperature_int;
  char nb_iot_data[32];
  char value[256];

  delta_tx = millis() - timer_millis_tx;

  if ( delta_tx > timer_period_to_tx) {
   
    temperature_int = current_temperature * 100;
    Serial.println(temperature_int);
    Serial.println("TX DATA");

    displayTX(true);
    sendUbirchInt( "temperature", temperature_int );

    timer_millis_tx = millis();

    // flash LED
    gmxNB_Led3(GMXNB_LED_ON);
    delay(200);
    gmxNB_Led3(GMXNB_LED_OFF);

    delay(500);
    displayTX(false);
    
   }
   else
  {
    displayTime2TX(timer_period_to_tx - delta_tx);
  }

  displayTemp(current_temperature );


  temp_temperature += readTemp();
  temperature_counts ++;

  if ( temperature_counts >= TEMPERATURE_SAMPLING )
  {
    current_temperature = temp_temperature / TEMPERATURE_SAMPLING;

    temperature_counts = 0;
    temp_temperature = 0;
  }
}
