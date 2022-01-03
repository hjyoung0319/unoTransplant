#include <Arduino.h>
#include <water/ec_sensor.h>
#include <AnalogPHMeter.h>
#include <water/water_level_sensor.h>
#include <water/water_temp_sensor.h>

#include <control/nutrient.h>
#include <control/solenoid_valve.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <Arduino.h>
#include <math.h>
#include <EEPROM.h>


// Define pins for sensors
#define WATER_LEVEL_HIGH_PIN 13
#define WATER_LEVEL_LOW_PIN 12
#define LED_RELAY_PIN 9
#define NUTRIENT_RELAY_PIN 8 
#define SOLENOID_RELAY_PIN 7 

// Create instance from the sensor classes
LiquidCrystal_I2C lcd(0x27,20,4); //
AnalogPHMeter pHSensor(A1);
ECSensor eCSensor; // Tx: 5, Rx: 4
WaterTemperatureSensor waterTemperatureSensor; // D6
WaterLevelSensor waterLevelSensor(WATER_LEVEL_LOW_PIN, WATER_LEVEL_HIGH_PIN);
SolenoidValve solenoidValve(SOLENOID_RELAY_PIN);
Nutrient nutrient(NUTRIENT_RELAY_PIN);


/*
  This part define constant for controlling ec value
*/

#define EC_LOW_BOUNDARY 1.8
#define EC_HIGH_BOUNDARY 2.2
#define EC_STANDARD 2.0

//Aim for EC
float goalEC = 2;

//Flag for valve
bool isOpen = false;
bool isError = false;
bool isControlEC = false;
unsigned int waterCnt;
const int MAX_WATER_LOOPS = 10; 

//Time value
extern volatile unsigned long timer0_millis;
unsigned long previousVal = 0;
unsigned long cnt= 86400000; // 1000 * 60 * 60 * 24 (24hour)
//UnoTime
unsigned long unoTime;
int unoSec;
bool isTurnLED = false;

//Change Value :
//Input current time 
int hour =  0; 
int min  =  0; 
int sec  =  0;
//Input Turn On and Off Time
int turnOnLEDtime= 0; 
int turnOffLEDtime = 18; 

//Define pHCalibrationValueAddress
unsigned int pHCalibrationValueAddress = 0;


void millisDelay(int delayTime)
{
  unsigned long currentMillis = millis();

  while (millis() < currentMillis + delayTime)
  {
    unoTime = millis();
    if (unoTime - previousVal >= 1000)
    {
      unoSec++;
      previousVal = millis();
    }
    if (sec + unoSec == 60)
    {
      min++;
      unoSec = 0;
      sec = 0;
      if (min % 20 == 0)
      {
        isControlEC = true;
      }
    }

    if (min == 60)
    {
      hour++;
      min = 0;
    }
  }
}

/*
//   Controlling Water And Nutrient
*/

void controlEc()
{
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Control EC");
  float ecValue = eCSensor.getEC();
  float difference;

  if (ecValue < EC_LOW_BOUNDARY && waterLevelSensor.getWaterLevel() != WATER_LEVEL_LOW)
  {
    difference = ecValue - EC_STANDARD;
    if (difference > 1.6)
    {
      nutrient.open();
      millisDelay(10000);
      nutrient.close();
    }
    else if (difference > 1.0)
    {
      nutrient.open();
      millisDelay(5000);
      nutrient.close();
    }
    else
    {
      nutrient.open();
      millisDelay(3000);
      nutrient.close();
    }
  }

  else if (ecValue > EC_HIGH_BOUNDARY && waterLevelSensor.getWaterLevel() != WATER_LEVEL_HIGH)
  {
    difference = ecValue - EC_STANDARD;
    if (difference > 1.0)
    {
      solenoidValve.open();
      millisDelay(10000);
      solenoidValve.close();
    }
    else
    {
      solenoidValve.open();
      millisDelay(3000);
      solenoidValve.close();
    }
  }
  else
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Not Enough Water!!");
  }
}


void controlWater(){
  WaterLevel current = waterLevelSensor.getWaterLevel();

    if(current == WATER_LEVEL_LOW && !isOpen && !isError)
    {
      solenoidValve.open();
      isOpen =true;
    }
    else if((isError || current != WATER_LEVEL_LOW) && isOpen)
    {
      solenoidValve.close();
      isOpen = false;
      waterCnt = 0;
    }

    if(isOpen){
      waterCnt++;
      if(waterCnt > MAX_WATER_LOOPS) isError = true;
    }
  }

void setup() {
  // put your setup code here, to run once:
  lcd.init();
  lcd.backlight();
  lcd.print("Setting Start");
  Serial.begin(9600);
  Wire.begin();
  eCSensor.begin();
  struct PHCalibrationValue pHCalibrationValue;
  EEPROM.get(pHCalibrationValueAddress, pHCalibrationValue);
  pHSensor.initialize(pHCalibrationValue);
  pinMode(LED_RELAY_PIN, OUTPUT);
  // Check Control Sensor
  solenoidValve.open();
  delay(500);
  solenoidValve.close();

  nutrient.open();
  delay(500);
  nutrient.close();
  //initialize water count

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Setting Done!");
  
}

void loop() {
  // put your main code here, to run repeatedly:
  
  // Current Time Setting For LED 
  unoTime = millis();
  if(unoTime - previousVal >= 1000)
  {
    unoSec++;
    previousVal = unoTime;

  }
  if(sec + unoSec == 60){
    min++;
    unoSec = 0;
    sec=0;
    if (min % 20 == 0)
    {
      isControlEC = true;
    }
  }

  if(min == 60){
    hour++;
    min = 0;
  }
  

  if( hour < turnOnLEDtime && hour >= turnOffLEDtime && isTurnLED == true){
    //LED OFF
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Turn OFF LED"); 
    digitalWrite(LED_RELAY_PIN,HIGH);
    isTurnLED = false;
  } 
  if( (hour >= turnOnLEDtime || hour < turnOffLEDtime) && isTurnLED == false){
    //LED ON
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Turn ON LED"); 
    digitalWrite(LED_RELAY_PIN,LOW);
    
    isTurnLED = true;
  } 

  if ((hour*3600000)+ (min*60000)+ unoTime > cnt)
  {
    timer0_millis=0;
    sec = 0;
    hour = 0;
    min = 0;
  }

  /*
  //   This step aim for measuring water circumstance 
  */
 
  float currentWaterTemp =roundf(waterTemperatureSensor.getWaterTemperature()*1000)/ float (1000);
  float currentPHAvg =pHSensor.singleReading().getpH();
  WaterLevel currentWaterLevel =waterLevelSensor.getWaterLevel();
  float currentEC = eCSensor.getEC();

  /*
  //   LCD Print current value
  */
  
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("WaterTemp:    " + String(currentWaterTemp) + 'C');
  lcd.setCursor(0,1);
  if(isError) lcd.print("**** Push Reset ****");
  else lcd.print("WaterLevel:     "+ String(waterLevelSensor.printWaterLevel(currentWaterLevel)));
  lcd.setCursor(0,2);
  lcd.print("PH:             " + String(currentPHAvg));
  lcd.setCursor(0,3);
  lcd.print("EC:"+ String(currentEC) + "  GoalEC:" + String(goalEC));

  millisDelay(3000);

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("EC Control After");
  lcd.setCursor(0,1);
  lcd.print("              " + String(20-(min%20)) + " Min");

  millisDelay(3000);

  
  // /*
  // //   This step aim for controlling water
  // */

  controlWater();

  // /*
  // //   This step aim for controlling EC ( 1 Loop = 5 Seconds, 60,000 Loops = 5 Minutes)
  // */

  if (min % 20 == 0 && isControlEC)
      {
        controlEc();

        isControlEC = false;
      }
 
}