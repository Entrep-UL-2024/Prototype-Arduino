#include "utility.h"

void initializePrint(){
    #ifdef PRINTS
    Serial.begin(115200);
    // while(!Serial) delay(10);
    delay(2500); //wait for serial
    delay(125);
    #endif
    Println("");
    Println("MAX30102 Oxymeter Sensor");
}
