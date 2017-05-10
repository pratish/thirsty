// This #include statement was automatically added by the Particle IDE.
#include "constants.h"
#include <ThingSpeak.h>

#include "HX711ADC.h"
#include <neopixel.h>

#define DATA_PIN D1
#define SCK_PIN D0

const float CALIBRATION_FACTOR =  11590.00;  

TCPClient client;

// ThigsSpeak configuration
const int THINGSPEAK_DELAY = 16000;
const unsigned int myChannelNumber = 260929;
const char * myWriteAPIKey = "9N70BV37CGPGAZR7";
 
// NeoPixel configuration
#define PIXEL_PIN D2
#define PIXEL_COUNT 12
#define PIXEL_TYPE WS2812B


// Timer to track when the system started;
unsigned long start_time = 0;  

static unsigned int notification_counter = 10;
static unsigned int led_counter = 0;


// Interrupt pin used to tare the system.
#define TAREINTERRUPT_PIN D4
// Interrupt pin used to start the system.
#define STARTINTERRUPT_PIN D3

// Software debouncer timer.
unsigned long button_time = 0;  
unsigned long last_button_time = 0; 

static float oldWeight = 0.0f;
static float newWeight = -99.0f;
static bool start = false;
static bool startInstruction = false;
static bool tareInstruction = false;

// Create a timer to send data to ThingSpeak.
Timer timer(THINGSPEAK_DELAY, sendData);

HX711ADC scale(DATA_PIN, SCK_PIN);
Adafruit_NeoPixel strip(PIXEL_COUNT, PIXEL_PIN, PIXEL_TYPE);
uint32_t red = strip.Color(255, 0, 0);
uint32_t yellow = strip.Color(255, 255, 0);
uint32_t black = strip.Color(0,0,0);
uint32_t blue = strip.Color(30,144,255);

static bool lifted = false;
static float drank = 0.0f;
static float total_drank = 0.0f;
static unsigned long last_drank = 0;
// Drink timer set to 30 seconds, 30 / 12 (pixel) = 2.5
const int DRINK_TIMER = 2500;

Timer drink_timer(DRINK_TIMER, changeNotification);

void sendData() {
  Serial.println("Sending data to thingspeak");
  //Write to ThingsSPeak
  ThingSpeak.setField(1, oldWeight);
  ThingSpeak.setField(2, drank);
  ThingSpeak.setField(3, total_drank);

  // Write the fields that you've set all at once.
  ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);  
}

void startSystem() {
  button_time = millis();
  
  if (button_time - last_button_time > 500)
  {
    startInstruction = !startInstruction;
    last_button_time = button_time;
  }
}

void tareSystem() {
  button_time = millis();

  if (button_time - last_button_time > 500)
  {
    tareInstruction = true;
    last_button_time = button_time;
  }
}

void stopSystem() {
  start = false; 
  
  // Stop the running timer and stop sending data to ThingSpeak.
  timer.stop();
  drink_timer.stop();
  changeLed(yellow);
}

void changeNotification() {
  notification_counter += 255/strip.numPixels();
  if (notification_counter >= 255) {
      notification_counter = 255;
  }  
  strip.setPixelColor(led_counter, red);
  strip.setBrightness(notification_counter);
  strip.show();
  
  Serial.println(notification_counter);

  if (led_counter < strip.numPixels()) {
      led_counter++;
  }
}
//=============================================================================================
//                         SETUP
//=============================================================================================
void setup() {
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  
  Serial.begin(9600);
  
  scale.set_scale(CALIBRATION_FACTOR);
  scale.tare(); //Reset the scale to 0
  
  // Initialize ThingsSpeak client
  ThingSpeak.begin(client);
  
  attachInterrupt(STARTINTERRUPT_PIN, startSystem, RISING);
  attachInterrupt(TAREINTERRUPT_PIN, tareSystem, RISING);

  changeLed(yellow);
  
  strip.show();
}

//=============================================================================================
//                         LOOP
//=============================================================================================
void loop() {
  long current_time = millis();
  if (start) {
    evaluateWeight();
  } else {
      Serial.println("System isn't evaluating your drinking habbits");
      delay(1000);
  }
  
  if (startInstruction) {
    oldWeight = scale.get_units(10);
    start = true;
    total_drank = 0;
    Serial.println("System started!");
    start_time = current_time;
    timer.start();
    drink_timer.start();
    startInstruction = false;
    uint16_t i;
    changeLed(black);
  } else {
    //Serial.println("System stoping because start instruction is false.");
    //stopSystem();
  }
  
  if (tareInstruction) {
    scale.tare();  //Reset the scale to zero
    Serial.println("Scale set to 0!");
    tareInstruction = false;
    
    stopSystem();
    changeLed(blue);
  }
  
  // Track when to stop sending data to ThingSpeak
  if (current_time - start_time > 480000) {
      Serial.println("You are done with your work day, stopping the system now.");
      stopSystem();
  }
}

double evaluateWeight() {
    double currentWeight = scale.get_units(10);
    
    if (currentWeight == oldWeight) {
        Serial.println("Weight hasn't changed!");
        lifted = false;
        return false;
    }
    
    if (!lifted && oldWeight > MINIMUM_WEIGHT && currentWeight <= MINIMUM_WEIGHT) {
        lifted = true;
        changeLed(black);
        drank = 0;
        drink_timer.stop();
        timer.stop();
        Serial.println("Detected a lift");
    }
    if (lifted && currentWeight > MINIMUM_WEIGHT) {
        currentWeight = getWeight();
        Serial.println("Put the bottle back!");
        drank = oldWeight - currentWeight;        
        total_drank += drank;
        oldWeight = currentWeight;
        Serial.print("You drank ");
        Serial.print(drank, 2);
        Serial.println(" oz of water!");
        lifted = false;
        last_drank = millis();
        notification_counter = 10;
        led_counter = 0;
        timer.start();
        drink_timer.start();
        sendData();
    }
    return true;
}

double getWeight() {
    delay(2000);
    return scale.get_units(10);
}

void changeLed(uint32_t color) {
    for(int i=0; i<strip.numPixels(); i++) {
          strip.setPixelColor(i, color);
    }
    strip.show();
}

