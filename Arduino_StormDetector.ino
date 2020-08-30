/*
  Station Meteo Pro - module pyranomètre
  avec : 
     - Arduino Uno
     - Module Grove
     - Shield Ethernet
     - AS3935

Cablage AS3935 : 
VCC (V) – 3V
GND(G) – GND
SCL (CL) – i2c
SDA (DA) – i2c
MI – /
IRQ – D4
SI – 3V
CS – /
  
  Source :     https://www.sla99.fr
  Site météo : https://www.meteo-four38.fr
  Date : 2020-08-30

  Changelog : 
  30/08/2020  v1    xx initiale

*/

#include <SPI.h>
#include <Wire.h>
#include "SparkFun_AS3935.h"
#include <Ethernet.h>

// 0x03 is default, but the address can also be 0x02, or 0x01.
// Adjust the address jumpers on the underside of the product. 
#define AS3935_ADDR 0x03 
#define INDOOR 0x12 
#define OUTDOOR 0xE
#define LIGHTNING_INT 0x08
#define DISTURBER_INT 0x04
#define NOISE_INT 0x01

#define LEDBLEU 8
#define LEDVERTE 7
#define LEDROUGE 9

unsigned long delaireseau = 60000L;    //1 min
unsigned long delaiorage =  1800000L;   //30min

unsigned long previousMillis=   0;
unsigned long previousMillis2=   0;


// If you're using I-squared-C then keep the following line. Address is set to
// default. 
SparkFun_AS3935 lightning(AS3935_ADDR);

// Interrupt pin for lightning detection 
const int lightningInt = 4; 

// Values for modifying the IC's settings. All of these values are set to their
// default values. 
byte noiseFloor = 2;
byte watchDogVal = 2;
byte spike = 2;
byte lightningThresh = 1; 

// This variable holds the number representing the lightning or non-lightning
// event issued by the lightning detector. 
byte intVal = 0; 

byte mac[6] = { 0xBE, 0xEF, 0x00, 0xFD, 0xb7, 0x92 }; //mac address de la carte Ethernet Arduino
char macstr[18];

char server[] = "192.168.1.2";  //IP du synology
EthernetClient client;          //client pour appeler le webservice sur le synology

String KEY_WS="134567654345670012";   //clé d'appel du webservice

void setup()
{
  // When lightning is detected the interrupt pin goes HIGH.
  pinMode(lightningInt, INPUT); 
  pinMode(LEDBLEU, OUTPUT);
  pinMode(LEDVERTE, OUTPUT);
  pinMode(LEDROUGE, OUTPUT); 

  Serial.begin(9600); 
  digitalWrite(LEDROUGE, HIGH);
  digitalWrite(LEDVERTE, HIGH);
  digitalWrite(LEDBLEU, HIGH);
  delay(500);
  digitalWrite(LEDROUGE, LOW);
  digitalWrite(LEDVERTE, LOW);
  digitalWrite(LEDBLEU, LOW);
  delay(500);
  
    
  Serial.println("Starting network");
  //Démarrage du réseau
  //Si OK : clignotement de la led NETWORK 1 fois
  //si KO : on laisse allumé la led ERROR et la led NETWORK
  if(!Ethernet.begin(mac)){
      Serial.println("Network failed");
    digitalWrite(LEDROUGE, HIGH);
    digitalWrite(LEDVERTE, HIGH);
  }
  else{     
      Serial.println(Ethernet.localIP());
      Serial.println(Ethernet.gatewayIP());
      digitalWrite(LEDVERTE, HIGH);
      delay(500);
      digitalWrite(LEDVERTE, LOW);
  } 

  
  Serial.println("AS3935 Franklin Lightning Detector"); 

  Wire.begin(); // Begin Wire before lightning sensor. 
  if( !lightning.begin() ){ // Initialize the sensor. 
    Serial.println ("Lightning Detector did not start up, freezing!"); 
    while(1); 
  }
  else
    Serial.println("Schmow-ZoW, Lightning Detector Ready!\n");
  // "Disturbers" are events that are false lightning events. If you find
  // yourself seeing a lot of disturbers you can have the chip not report those
  // events on the interrupt lines. 
  lightning.maskDisturber(true); 
  int maskVal = lightning.readMaskDisturber();
  Serial.print("Are disturbers being masked: "); 
  if (maskVal == 1)
    Serial.println("YES"); 
  else if (maskVal == 0)
    Serial.println("NO"); 

  // The lightning detector defaults to an indoor setting (less
  // gain/sensitivity), if you plan on using this outdoors 
  // uncomment the following line:
  //lightning.setIndoorOutdoor(OUTDOOR); 
  int enviVal = lightning.readIndoorOutdoor();
  Serial.print("Are we set for indoor or outdoor: ");  
  if( enviVal == INDOOR )
    Serial.println("Indoor.");  
  else if( enviVal == OUTDOOR )
    Serial.println("Outdoor.");  
  else 
    Serial.println(enviVal, BIN); 
 
  // Noise floor setting from 1-7, one being the lowest. Default setting is
  // two. If you need to check the setting, the corresponding function for
  // reading the function follows.    

  lightning.setNoiseLevel(noiseFloor);  

  int noiseVal = lightning.readNoiseLevel();
  Serial.print("Noise Level is set at: ");
  Serial.println(noiseVal);

  // Watchdog threshold setting can be from 1-10, one being the lowest. Default setting is
  // two. If you need to check the setting, the corresponding function for
  // reading the function follows.    

  lightning.watchdogThreshold(watchDogVal); 

  int watchVal = lightning.readWatchdogThreshold();
  Serial.print("Watchdog Threshold is set to: ");
  Serial.println(watchVal);

  // Spike Rejection setting from 1-11, one being the lowest. Default setting is
  // two. If you need to check the setting, the corresponding function for
  // reading the function follows.    
  // The shape of the spike is analyzed during the chip's
  // validation routine. You can round this spike at the cost of sensitivity to
  // distant events. 

  lightning.spikeRejection(spike); 

  int spikeVal = lightning.readSpikeRejection();
  Serial.print("Spike Rejection is set to: ");
  Serial.println(spikeVal);

  // This setting will change when the lightning detector issues an interrupt.
  // For example you will only get an interrupt after five lightning strikes
  // instead of one. Default is one, and it takes settings of 1, 5, 9 and 16.   
  // Followed by its corresponding read function. Default is zero. 

  lightning.lightningThreshold(lightningThresh); 

  uint8_t lightVal = lightning.readLightningThreshold();
  Serial.print("The number of strikes before interrupt is triggerd: "); 
  Serial.println(lightVal); 

  // When the distance to the storm is estimated, it takes into account other
  // lightning that was sensed in the past 15 minutes. If you want to reset
  // time, then you can call this function. 

  //lightning.clearStatistics(); 

  // The power down function has a BIG "gotcha". When you wake up the board
  // after power down, the internal oscillators will be recalibrated. They are
  // recalibrated according to the resonance frequency of the antenna - which
  // should be around 500kHz. It's highly recommended that you calibrate your
  // antenna before using these two functions, or you run the risk of schewing
  // the timing of the chip. 

  //lightning.powerDown(); 
  //delay(1000);
  //if( lightning.wakeUp() ) 
   // Serial.println("Successfully woken up!");  
  //else 
    //Serial.println("Error recalibrating internal osciallator on wake up."); 
  
  // Set too many features? Reset them all with the following function.
  lightning.resetSettings();
  
}

void loop()
{
  
  unsigned long currentMillis = millis(); // read time passed
  if (currentMillis - previousMillis > delaiorage){
    previousMillis = millis();
    digitalWrite(LEDBLEU, LOW); 
  }
  if (currentMillis - previousMillis2 > delaireseau){
    previousMillis2 = millis(); 
     if (client.connect(server, 81)) { 
        digitalWrite(LEDVERTE, HIGH);      
        delay(200);
        digitalWrite(LEDVERTE, LOW);
        
      } 
      else {
        digitalWrite(LEDROUGE, HIGH);
        digitalWrite(LEDVERTE, HIGH);
       Serial.println("connection failed KO synology");
      }
  }
  if(digitalRead(lightningInt) == HIGH){
    // Hardware has alerted us to an event, now we read the interrupt register
    // to see exactly what it is. 
    intVal = lightning.readInterruptReg();
    if(intVal == NOISE_INT){
      Serial.println("Noise."); 
    }
    else if(intVal == DISTURBER_INT){
      Serial.println("Disturber."); 
    }
    else if(intVal == LIGHTNING_INT){
      Serial.println("Lightning Strike Detected!"); 
      digitalWrite(LEDBLEU, HIGH); 
      // Lightning! Now how far away is it? Distance estimation takes into
      // account previously seen events. 
      byte distance = lightning.distanceToStorm(); 
      Serial.print("Approximately: "); 
      Serial.print(distance); 
      Serial.println("km away!"); 

      // "Lightning Energy" and I do place into quotes intentionally, is a pure
      // number that does not have any physical meaning. 
      long lightEnergy = lightning.lightningEnergy(); 
      Serial.print("Lightning Energy: "); 
      Serial.println(lightEnergy); 

     // String lineToWrite = '1;'+String(distance);
      if (client.connect(server, 81)) { 
        digitalWrite(LEDVERTE, HIGH);      
        Serial.println("connected");
        client.println("GET /stationmeteo/storm.php?key="+KEY_WS+"&storm=1&distance="+distance+"&energy="+lightEnergy+" HTTP/1.1");
        client.println("Host: 192.168.1.2");
        client.println("Connection: close");
        client.println();
        digitalWrite(LEDVERTE, LOW);
        digitalWrite(LEDROUGE, LOW);
        
      } 
      else {
        digitalWrite(LEDROUGE, HIGH);
        digitalWrite(LEDVERTE, HIGH);
       Serial.println("connection failed KO synology");
      }
      
      

    }
  }
 //Affichage de la réponse du serveur dans le moniteur série si on est en débug
    if(client.available()) {
      char c = client.read();
      Serial.print(c);
    }
    if(!client.connected()) {
      client.stop();
    }  
}
