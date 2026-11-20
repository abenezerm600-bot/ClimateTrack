
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// LCD configuration
LiquidCrystal_I2C lcd(0x27, 16, 2);

// DHT11 configuration
#define DHTPIN 4
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();

  dht.begin();

  lcd.setCursor(0, 0);
  lcd.print("ClimateTrack");
  delay(2000);
  lcd.clear();
}

void loop() {
  float humidity = dht.readHumidity();
  float tempF = dht.readTemperature(true);

  lcd.clear();

  if (isnan(humidity) || isnan(tempF)) {
    lcd.print("Sensor Error!");
  } else {
    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(tempF, 1);
    lcd.print(" F");

    lcd.setCursor(0, 1);
    lcd.print("Humidity: ");
    lcd.print(humidity, 1);
    lcd.print("%");
  }

  delay(2000);
}
