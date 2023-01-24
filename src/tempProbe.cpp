#include <DallasTemperature.h>
#include <tempProbe.h>

// Temperature sensor is connected to D4
#define ONE_WIRE_BUS D4

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress insideThermometer;

void setupTempProbe() {
  // Get address of temperature sensor and set resolution
  sensors.begin();
  sensors.getAddress(insideThermometer, 0);
  sensors.setResolution(insideThermometer, 12);
}

float getTemp() {
  // Read temperature
  sensors.requestTemperatures();
  return sensors.getTempC(insideThermometer);
}
