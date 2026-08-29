#include "OLED.h"
#include "Config.h"

Adafruit_SSD1306 display(
    SCREEN_WIDTH,
    SCREEN_HEIGHT,
    &Wire,
    OLED_RESET
);

void OLED_begin()
{
    Wire.begin(21,22);
    Wire.setClock(400000);

    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
    {
        Serial.println("Error OLED");
        while(true);
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(15,25);
    display.println("MONITOR INICIANDO");
    display.display();

    delay(1500);

    display.clearDisplay();
    display.display();
}