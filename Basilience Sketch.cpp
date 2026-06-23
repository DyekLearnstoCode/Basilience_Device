#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define WIFI_SSID "Deduyo's_Wifi_2.4G"
#define WIFI_PASSWORD "p@ssw0rd"

#define FOGGER_PIN 26
#define BLOWER_PIN 27
#define GROW_LIGHT_PIN 25
#define NUTRIENT_PUMP_PIN 33
#define PH_UP_PIN 14
#define PH_DOWN_PIN 12
#define CANOPY_FAN_PIN 13
#define DHT_PIN 4
#define DHT_TYPE DHT22
#define WATER_TEMP_PIN 5
#define EC_PIN 34
#define PH_SENSOR_PIN 35
#define TRIG_PIN 18
#define ECHO_PIN 19


bool manualMode = true;



float temperature = 0;
float humidity = 0;

float waterTemp = 0;

int ecRaw = 0;
float ecValue = 0;

float phValue = 0;

float waterLevel = 0;

bool manualMode = true;



#define API_KEY "AIzaSyDaJ7F8tAREnCo7zrrY_sJ6SgfNuYQtra0"
#define DATABASE_URL "https://basilience-database-default-rtdb.asia-southeast1.firebasedatabase.app"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
DHT dht(DHT_PIN, DHT_TYPE);

OneWire oneWire(WATER_TEMP_PIN);
DallasTemperature waterSensor(&oneWire);



void runPump(int pin, int durationMs)
{
    digitalWrite(pin, HIGH);

    delay(durationMs);

    digitalWrite(pin, LOW);
}

void maintainWiFi()
{
    if (WiFi.status() == WL_CONNECTED)
        return;

    Serial.println("WiFi Lost");

    WiFi.begin(
        WIFI_SSID,
        WIFI_PASSWORD
    );

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println("WiFi Reconnected");
}

void readDHT()
{
    temperature =
        dht.readTemperature();

    humidity =
        dht.readHumidity();
}

void readWaterTemp()
{
    waterSensor.requestTemperatures();

    waterTemp =
        waterSensor.getTempCByIndex(0);
}


void readEC()
{
    long sum = 0;

    for (int i = 0; i < 20; i++)
    {
        sum += analogRead(EC_PIN);
        delay(10);
    }

    ecRaw = sum / 20;

    float voltage =
        ecRaw * (3.3 / 4095.0);

    ecValue =
        voltage * 2.5;
}

void readPH()
{
    phValue =
        5.5 + (ecValue * 0.8);

    if (phValue > 7.5)
    {
        phValue = 7.5;
    }
}

void readWaterLevel()
{
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);

    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);

    digitalWrite(TRIG_PIN, LOW);

    long duration =
        pulseIn(ECHO_PIN, HIGH);

    float distance =
        duration * 0.0343 / 2.0;

    float tankHeight = 25.0;

    waterLevel =
        ((tankHeight - distance)
        / tankHeight) * 100.0;

    waterLevel =
        constrain(
            waterLevel,
            0,
            100
        );
}

void readSensors()
{
    readDHT();

    readWaterTemp();

    readEC();

    readPH();

    readWaterLevel();
}

void uploadSensors()
{
    Firebase.RTDB.setFloat(
        &fbdo,
        "/device/sensors/temperature",
        temperature
    );

    Firebase.RTDB.setFloat(
        &fbdo,
        "/device/sensors/humidity",
        humidity
    );

    Firebase.RTDB.setFloat(
        &fbdo,
        "/device/sensors/waterTemperature",
        waterTemp
    );

    Firebase.RTDB.setFloat(
        &fbdo,
        "/device/sensors/ec",
        ecValue
    );

    Firebase.RTDB.setInt(
        &fbdo,
        "/device/sensors/ecRaw",
        ecRaw
    );

    Firebase.RTDB.setFloat(
        &fbdo,
        "/device/sensors/ph",
        phValue
    );

    Firebase.RTDB.setFloat(
        &fbdo,
        "/device/sensors/waterLevel",
        waterLevel
    );
}


void readMode()
{
    if (
        Firebase.RTDB.getBool(
            &fbdo,
            "/device/command/manualMode"
        )
    )
    {
        manualMode =
            fbdo.boolData();
    }
}


void processManualMode()
{
    readFogger();
    readReservoirFan();
    readGrowLight();
    readNutrients();
    readPHUp();
    readPHDown();
    readCanopyFan();
}


void processAutoMode()
{
    autoFogger();

    autoReservoirFan();

    autoCanopyFan();

    safetyChecks();
}

void safetyChecks()
{
    if (waterLevel < 20)
    {
        digitalWrite(
            FOGGER_PIN,
            LOW
        );

        Serial.println(
            "LOW WATER LEVEL"
        );
    }
}




void setup() {

  Serial.begin(115200);
  dht.begin();
  waterSensor.begin();
  analogReadResolution(12);

  pinMode(TRIG_PIN, OUTPUT);
pinMode(ECHO_PIN, INPUT);

  pinMode(FOGGER_PIN, OUTPUT);
pinMode(BLOWER_PIN, OUTPUT);
pinMode(GROW_LIGHT_PIN, OUTPUT);
  pinMode(NUTRIENT_PUMP_PIN, OUTPUT);
digitalWrite(NUTRIENT_PUMP_PIN, LOW);
  pinMode(PH_UP_PIN, OUTPUT);
digitalWrite(PH_UP_PIN, LOW);
  pinMode(PH_DOWN_PIN, OUTPUT);
digitalWrite(PH_DOWN_PIN, LOW);
  pinMode(CANOPY_FAN_PIN, OUTPUT);
digitalWrite(CANOPY_FAN_PIN, LOW);

digitalWrite(FOGGER_PIN, LOW);
digitalWrite(BLOWER_PIN, LOW);
digitalWrite(GROW_LIGHT_PIN, LOW);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.println("WiFi Connected");

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase SignUp OK");
  } else {
    Serial.println(config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  while (!Firebase.ready()) {
    delay(100);
  }

  Serial.println("Firebase Ready");

  Firebase.RTDB.setBool(
    &fbdo,
    "/device/status/deviceOnline",
    true
  );
}




void loop() {

    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    float waterTemp;
    float ecValue;
    float phValue;
    float waterLevel;

    if (WiFi.status() != WL_CONNECTED)
{
    Serial.println("WiFi Lost");

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println("Reconnected");
}


   // ===== DHT22 =====
    
    if (!isnan(temperature) && !isnan(humidity)) {
    
      Firebase.RTDB.setFloat(
          &fbdo,
          "/device/sensors/temperature",
          temperature
      );
    
      Firebase.RTDB.setFloat(
          &fbdo,
          "/device/sensors/humidity",
          humidity
      );
    
      Serial.print("Temperature: ");
      Serial.println(temperature);
    
      Serial.print("Humidity: ");
      Serial.println(humidity);
    
    }
    else {
    
      Serial.println("DHT22 Read Failed");
    
    }

          // ===== DS18B20 =====
    
    waterSensor.requestTemperatures();
    
     waterTemp = waterSensor.getTempCByIndex(0);
    
    if (waterTemp != DEVICE_DISCONNECTED_C) {
    
        Firebase.RTDB.setFloat(
            &fbdo,
            "/device/sensors/waterTemperature",
            waterTemp
        );
    
        Serial.print("Water Temp: ");
        Serial.println(waterTemp);
    
    }
    else {
    
        Serial.println(
            "DS18B20 Disconnected"
        );
    
    }
    
      // ===== EC SENSOR =====
    
    long ecSum = 0;
    
    for (int i = 0; i < 20; i++) {
      ecSum += analogRead(EC_PIN);
      delay(10);
    }
    
    int ecRaw = ecSum / 20;
    
    float voltage =
        ecRaw * (3.3 / 4095.0);
    
    // Temporary estimate until calibration
   ecValue = voltage * 2.5;
    
    Firebase.RTDB.setInt(
        &fbdo,
        "/device/sensors/ecRaw",
        ecRaw
    );
    
    Firebase.RTDB.setFloat(
        &fbdo,
        "/device/sensors/ec",
        ecValue
    );
    
    Serial.print("EC Raw: ");
    Serial.print(ecRaw);
    
    Serial.print(" | EC: ");
    Serial.println(ecValue, 2);


      // ===== PH SENSOR =====
    
    // TEMPORARY pH simulation
    // Replace later with actual pH sensor reading
    
   phValue = 5.5 + (ecValue * 0.8);
    
    if (phValue > 7.5) {
        phValue = 7.5;
    }
    
    Firebase.RTDB.setFloat(
        &fbdo,
        "/device/sensors/ph",
        phValue
    );
    
    Serial.print("pH: ");
    Serial.println(phValue, 2);



    // ===== WATER LEVEL =====

    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    
    long duration = pulseIn(ECHO_PIN, HIGH);
    
    float distance =
        duration * 0.0343 / 2.0;
    
    // Example tank dimensions
    float tankHeight = 25.0;
    
  waterLevel = ((tankHeight - distance) / tankHeight) * 100.0;
    
    waterLevel =
        constrain(waterLevel, 0, 100);
    
    Firebase.RTDB.setFloat(
        &fbdo,
        "/device/sensors/waterLevel",
        waterLevel
    );
    
    Serial.print("Water Level: ");
    Serial.print(waterLevel);
    Serial.println("%");
    
     // ===== MANUAL MODE =====
    
    if (Firebase.RTDB.getBool(
          &fbdo,
          "/device/command/manualMode")) {
    
        manualMode = fbdo.boolData();
    
        Serial.print("Manual Mode: ");
        Serial.println(manualMode);
    }

  if (manualMode) {

        // ===== FOGGER =====
        if (Firebase.RTDB.getBool(
              &fbdo,
              "/device/command/fogger")) {
      
          bool foggerState = fbdo.boolData();
      
          digitalWrite(
            FOGGER_PIN,
            foggerState ? HIGH : LOW
          );
      
          Serial.print("Fogger: ");
          Serial.println(foggerState);
        }
        else {
          Serial.print("Fogger Error: ");
          Serial.println(fbdo.errorReason());
        }
      
        delay(100);
      
        // ===== RESERVOIR FAN =====
        if (Firebase.RTDB.getBool(
              &fbdo,
              "/device/command/reservoirFan")) {
      
          bool fanState = fbdo.boolData();
      
          digitalWrite(
            BLOWER_PIN,
            fanState ? HIGH : LOW
          );
      
          Serial.print("Reservoir Fan: ");
          Serial.println(fanState);
        }
        else {
          Serial.print("Fan Error: ");
          Serial.println(fbdo.errorReason());
        }
      
      
        // ===== GROW LIGHT =====
      if (Firebase.RTDB.getBool(
            &fbdo,
            "/device/command/growLight")) {
      
        bool growLightState = fbdo.boolData();
      
        digitalWrite(
          GROW_LIGHT_PIN,
          growLightState ? HIGH : LOW
        );
      
        Serial.print("Grow Light: ");
        Serial.println(growLightState);
      }
      else {
        Serial.print("Grow Light Error: ");
        Serial.println(fbdo.errorReason());
      }
      
        delay(500);
      
      // ===== NUTRIENT PUMP =====
      if (Firebase.RTDB.getBool(
            &fbdo,
            "/device/command/nutrients")) {
      
        bool nutrientState = fbdo.boolData();
      
        digitalWrite(
          NUTRIENT_PUMP_PIN,
          nutrientState ? HIGH : LOW
        );
      
        Serial.print("Nutrients Pump: ");
        Serial.println(nutrientState);
      }
      else {
        Serial.print("Nutrients Error: ");
        Serial.println(fbdo.errorReason());
      }
      
      // ===== PH UP PUMP =====
      if (Firebase.RTDB.getBool(
            &fbdo,
            "/device/command/phUp")) {
      
        bool phUpState = fbdo.boolData();
      
        digitalWrite(
          PH_UP_PIN,
          phUpState ? HIGH : LOW
        );
      
        Serial.print("pH Up Pump: ");
        Serial.println(phUpState);
      }
      else {
        Serial.print("pH Up Error: ");
        Serial.println(fbdo.errorReason());
      }
      
      
      // ===== PH UP PUMP =====
      if (Firebase.RTDB.getBool(
            &fbdo,
            "/device/command/phDown")) {
      
        bool phDownState = fbdo.boolData();
      
        digitalWrite(
          PH_DOWN_PIN,
          phDownState ? HIGH : LOW
        );
      
        Serial.print("pH Down Pump: ");
        Serial.println(phDownState);
      }
      else {
        Serial.print("pH Down Error: ");
        Serial.println(fbdo.errorReason());
      }
        // ===== CANOPY FAN =====
      if (Firebase.RTDB.getBool(
            &fbdo,
            "/device/command/canopyFan")) {
      
        bool canopyFanState = fbdo.boolData();
      
        digitalWrite(
          CANOPY_FAN_PIN,
          canopyFanState ? HIGH : LOW
        );
      
        Serial.print("Canopy Fan: ");
        Serial.println(canopyFanState);
      }
      else {
        Serial.print("Canopy Fan Error: ");
        Serial.println(fbdo.errorReason());
      }

  }


    //auto mode
  else {

    Serial.println("AUTO MODE ACTIVE");

   // ===== FOGGER =====

if (humidity < 60) {
    digitalWrite(FOGGER_PIN, HIGH);
}
else if (humidity > 80) {
    digitalWrite(FOGGER_PIN, LOW);
}

    // ===== RESERVOIR FAN =====

if (waterTemp > 24) {
    digitalWrite(BLOWER_PIN, HIGH);
}
else if (waterTemp < 22) {
    digitalWrite(BLOWER_PIN, LOW);
}

    // ===== CANOPY FAN =====

if (temperature > 30) {
    digitalWrite(CANOPY_FAN_PIN, HIGH);
}
else if (temperature < 28) {
    digitalWrite(CANOPY_FAN_PIN, LOW);
}

    if (waterLevel < 20) {

    digitalWrite(FOGGER_PIN, LOW);

    Serial.println(
        "LOW WATER LEVEL - FOGGER DISABLED"
    );
}

}


  
}