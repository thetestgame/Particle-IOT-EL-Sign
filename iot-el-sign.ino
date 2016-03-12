/*
The MIT License (MIT)

Copyright (c) 2016 Jordan Maxwell

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "MQTT/MQTT.h"

/***********************************
 * general settings definitions
 * *********************************/ 

#define POWER_PIN 0
int powered = 1;
int powerLevel = 0;
int maxPower = 255;

/***********************************
 * mode settings definitions
 * *********************************/ 

#define SOLID 0
#define PULSING 1
#define UNKNOWNMODE 2
int activeMode = 0;

/***********************************
 * pulse mode settings definitions
 * *********************************/ 

int pulseDelay = 30;
int pulseMinSpeed = 10; //prevents the pulseDelay from being below this value.
int pulseIncrement = 5;
int powerFlow = 1;

/***********************************
 * particle settings definitions
 * *********************************/ 

// PARTICLE RESTFUL API

//Enable/Disable Particle cloud breakout api
bool particleApi = true; 

/***********************************
 * MQTT settings definitions
 * *********************************/ 
 
//Enable/Disable MQTT functionality 
bool mqttEnabled = true;
char* mqttHost = "";
int mqttPort = 1883;
String mqttUsername = "";
String mqttPassword = "";
//MQTT feeds require mqttEnable to be set to true
String mqttPowerFeed = "";
String mqttModeFeed = "";
String mqttLevelFeed = "";

//Do not change. MQTT client declaration
void MQTTcallback(char* topic, byte* payload, unsigned int length);
MQTT client(mqttHost, mqttPort, MQTTcallback);

/**************************************
 * startup and loop functions *
 * ************************************/

void setup() {
    Serial.begin(9600);
    pinMode(POWER_PIN, OUTPUT);
    initMQTT();
    initParticleVariables();
}

void loop() {
    if (client.isConnected()) {
        client.loop(); 
    } else {
        initMQTT();
    }

    if (particleApi) {
        Particle.process();
    }    
    
    if (powered == 0) {
        setBrightness(0);
        analogWrite(POWER_PIN, 0);
    } else {
        analogWrite(POWER_PIN, powerLevel);
    }
    publishMQTT(mqttLevelFeed, String(powerLevel));

    processActiveMode();
}

/**************************************
 *   EL lighting control functions    *
 * ************************************/

bool setBrightness(int level) {
    if (level > maxPower)
        return false;
    powerLevel = level;
    return true;
}

bool setMaxBrightness(int level) {
    if (level > 255)
        return false;
    maxPower = level;
    return true;
}

/***************************************
 * lighting modes and switch functions *
 * *************************************/

void switchMode(int mode) {
    bool different = (mode != activeMode);
    bool valid = false;
    if (mode > 0 && mode < UNKNOWNMODE) 
        valid = true;
    if (different && valid) {
        activeMode = mode;
        publishMQTT(mqttModeFeed, String(mode));
    }
}

void processActiveMode() {
  switch(activeMode) {
      
      case SOLID:
        setBrightness(255);
        powerFlow = 0;
        break;
      case PULSING:
        if (powerFlow == 1) 
            setBrightness(powerLevel + pulseIncrement);
        else 
            setBrightness(powerLevel - pulseIncrement);
            
        if (powerLevel >= maxPower)
            powerFlow = 0;
        
        if (powerLevel <= 0) 
            powerFlow = 1;
        delay(pulseDelay);
        break;
      default:
       switchMode(SOLID);
       break;
  }  
}

/***********************************
 * debug functions and definitions *
 * *********************************/
 
void debug(String message) {
#ifdef SERIAL_DEBUG
    Serial.print(message);
#endif
}

void debugln(String message) {
#ifdef SERIAL_DEBUG
    Serial.println(message);
#endif
}

/**************************************
 * MQTT API functions and definitions *
 * ************************************/

void initMQTT() {
    if (mqttEnabled) {
        client.connect(mqttHost, mqttUsername, mqttPassword);   
        if (client.isConnected()) {
            MQTTSubscribe(mqttPowerFeed);
            MQTTSubscribe(mqttModeFeed);
            MQTTSubscribe(mqttLevelFeed);
        } else {
            debugln("Failed to initalize MQTT services!");
        }
    }
}

void MQTTSubscribe(String feed) {
    if (feed != "") {
        client.subscribe(feed);
    }
}

void MQTTcallback(char* topic, byte* payload, unsigned int length) {
    char p[length + 1];
    memcpy(p, payload, length);
    p[length] = NULL;
    String message(p);
    debugln("Received MQTT data (" + message + ") from " + topic);
    String feed(topic);
    if (feed.equals(mqttPowerFeed)) {
        if (message.equals("ON")) 
            powered = 1;
        else
            powered = 0;
    }
    
    if (feed.equals(mqttModeFeed)) {
        int number = message.toInt();
        if (number > 0 && number < UNKNOWNMODE) {
            switchMode(number);
        }
    }
    
    if (feed.equals(mqttLevelFeed) && activeMode != PULSING) {
        int number = message.toInt();
        
    }
}

bool publishMQTT(String feed, String payload) {
    if (feed == "")
        return false;
    if (!mqttEnabled) 
        return false;
    if (!client.isConnected()) {
        debugln("Failed to publish to mqtt feed (" + feed + "). No MQTT connection.");
        return false;
    }
    client.publish(feed, payload);
    return true;
}

/******************************************
 * Particle API functions and definitions *
 * ***************************************/
 
//initializes the shared variables and functions that are accessible through the particle API
void initParticleVariables() {
    if (particleApi) {
        debugln("Initializing Particle cloud callbacks...");
        
        //Particle Cloud API variable defintions
        Particle.variable("powered", &powered, INT);
        Particle.variable("powerLevel", &powerLevel, INT);
        Particle.variable("mode", &activeMode, INT);
        Particle.variable("maxPower", &maxPower, INT);
        
        //Particle Cloud API function callback defintions
        Particle.function("runCommand", (int (*)(String)) particleCommand);
    }
    
    if (particleApi)
        debugln("Particle cloud ready!");
    Particle.connect();
}

int particleCommand(String incoming) {
    int returning = 0;
    String data = "";
    debugln("Received incoming REST command packet (" + incoming + ")");
    if (incoming.substring(1,8) == "setPower") {
        data = incoming.substring(9);
        if (data == "true") {
            powered = 1;
            returning = 1;
        } else if (data == "false") {
            powered = 0;
            returning = 1;
        } else {
            debugln("Invalid setPower argument (" + data + ")");
        }
    } else if (incoming.substring(1, 7) == "setMode") {
        data = incoming.substring(8);
        int number = data.toInt();
        if (number >= UNKNOWNMODE || number < 0) {
            debugln("Invalid mode id (" + data + ")");
            return 0;
        }
        switchMode(number);
        returning = 1;
    } else if (incoming.substring(1, 13) == "setPulseSpeed") {
        data = incoming.substring(14);
        int number = data.toInt();
        if (number < pulseMinSpeed) {
            debugln("Invalid pulse speed! Pulse speed cannot be lower then (" + String(pulseMinSpeed) + ")");
            return 0;
        }
        pulseDelay = number;
        returning = 1;
    } else if (incoming.substring(1, 16) == "setMaxBrightness") {
        data = incoming.substring(17);
        int number = data.toInt();
        if (number > 255) {
            debugln("Invalid max brightness setting! Max brightness cannot be higher then 255");
            return 0;
        }
        setMaxBrightness(number);
        returning = 1;
    } else if (incoming.substring(1, 13) == "setBrightness") {
        data = incoming.substring(14);
        int number = data.toInt();
        if (number > maxPower) {
            debugln("Invalid brightness setting! Brightness cannot be higher then max brightness! (" + String(maxPower) + ")");
            return 0;
        }
        setBrightness(number);
        return 1;
    } else {
        debugln("Invalid command packet!");
    }
    return returning;
}
