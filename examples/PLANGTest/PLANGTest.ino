#include <SPI.h>
#include <SD.h>
#include <PLang.h>

PLang myEngine;

void setup() {
    Serial.begin(115200);
    
    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("Unable to open SD card");
        while(1);
    }
    myEngine.begin();
    if (!myEngine.load("test.plg")) {
        Serial.print("Error loading test.plg: ");
        Serial.print(strerror(errno));
        Serial.print(" at line ");
        Serial.println(errline);
        while(1);
    }

    Serial.println("Running program...");
}

void loop() {
    myEngine.run();
}
