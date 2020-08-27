#include <OneWire.h>
#include <DallasTemperature.h>

#define HEATER_RELAY_PIN 10
#define LED_PIN 13
#define MOTION_SENSOR_PIN 2
#define TEMPERATURE_SENSOR_DIGITAL_PIN 3
#define TEMPERATURE_MIN_C 32
#define TEMPERATURE_MAX_C 55
#define TEMPERATURE_MIN_DIFF_C 0.5
#define TEMPERATURE_MIN_TIME_BETWEEN_READS 3000 // 3 seconds
#define HEATER_MIN_CYCLE_TIME 180000 // 3 minutes


// Set up OneWire instance to communicate with our OneWire temperature device
OneWire temperatureSensorOneWire(TEMPERATURE_SENSOR_DIGITAL_PIN);

// Set up our sensor
DallasTemperature temperatureSensor(&temperatureSensorOneWire);

boolean heaterOn = false;
unsigned long heaterPreviousCycleTimestamp = 0;
unsigned long totalHeaterOnCycles = 0;
unsigned long totalHeaterOnTime = 0;
unsigned long totalHeaterOffTime = 0;
volatile bool motionDetected = false;

unsigned long previousTemperatureTimestamp = 0;
float previousTemperature = 0;

void setup(void) {
  // start our temperature sensor
  temperatureSensor.begin();
  pinMode(HEATER_RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(MOTION_SENSOR_PIN, INPUT);
  attachInterrupt(0, motionDetectedIsr, CHANGE);
  Serial.begin(9600);
}

void motionDetectedIsr() {
  motionDetected = true;
}

void turnHeaterOn() {
  heaterOn = true;
  digitalWrite(HEATER_RELAY_PIN, HIGH);
  digitalWrite(LED_PIN, HIGH);
}


void turnHeaterOff() {
  heaterOn = false;
  digitalWrite(HEATER_RELAY_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
}

void loop() {
  // get the current temperature (in Celsius)
  temperatureSensor.requestTemperatures();
  float currentTemperature = temperatureSensor.getTempCByIndex(0);

  // get the current time since we booted
  unsigned long currentTimestamp = millis();
  
  // is the heater is on
  if (heaterOn) {
    // decide if we should turn the heater off
    if ((currentTemperature > TEMPERATURE_MAX_C) || // have we reached the maximum desired temperature OR
        (
          (currentTimestamp - heaterPreviousCycleTimestamp > HEATER_MIN_CYCLE_TIME) && // has the heater been running for at least its cycle AND 
          (
            (currentTemperature > TEMPERATURE_MIN_C) || // have we reached the minimum desired temperature OR
            (currentTemperature - previousTemperature < TEMPERATURE_MIN_DIFF_C) // has the temperature stopped increasing
          )
        )
      ) {
      // turn the heater off
      turnHeaterOff();
      // aggregate heater's total on-time
      totalHeaterOnTime += currentTimestamp - heaterPreviousCycleTimestamp;
      // remember when we last cycled the heater
      heaterPreviousCycleTimestamp = currentTimestamp;
      // reset the motion detection trigger
      motionDetected = false;
    }
    
  } else {
    // decide if we should turn the heater on
    if ((currentTimestamp - heaterPreviousCycleTimestamp > HEATER_MIN_CYCLE_TIME) && // has the heater been off for at least its cycle AND
        (currentTemperature < TEMPERATURE_MIN_C) && // we are below the minimum temperature threshold AND
        motionDetected // we recently detected motion
       ) {
      // turn the heater on
      turnHeaterOn();
      // aggregate heater's total on-cycles
      totalHeaterOnCycles++;
      // aggregate heater's total off-time
      totalHeaterOffTime += currentTimestamp - heaterPreviousCycleTimestamp;
      // remember when we last cycled the heater
      heaterPreviousCycleTimestamp = currentTimestamp;
    }
  }

  // update the previous temperature (add a delay in order to minimize the noise)
  if (currentTimestamp - previousTemperatureTimestamp >= TEMPERATURE_MIN_TIME_BETWEEN_READS) {
    previousTemperature = currentTemperature;
    previousTemperatureTimestamp = currentTimestamp;
  }

  delay(500);
}
