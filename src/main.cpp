#include <Arduino.h>
#include <SoftwareSerial.h>

SoftwareSerial mySerial(8,9); // RX, TX
int count = 1;

// replace with your own PIN of the Sim card
const char SIM_PIN_NUMBER[] = "1234";

// XX = country code, e.g. "49" for Germany and xxxxxxxxxxx = phone number
const char TELEFONE_NUMBER[] = "+491729999128";

void updateSerial();

void setup()
{
	Serial.begin(9600);
	mySerial.begin(9600);

	Serial.println("Initializing...");
	delay(1000);

	// Once the handshake test is successful, it will back to OK
	mySerial.println("AT");
	updateSerial();

  	delay(60000);

	mySerial.println("AT+CPIN?"); //=\""+String(SIM_PIN_NUMBER)+"\"");
	updateSerial();
  	delay(60000);
}

void loop() {
  // Configuring TEXT mode
	mySerial.println("AT+CMGF=1");
	updateSerial();

	mySerial.println("AT+CMGS=\""+String(TELEFONE_NUMBER)+"\"");
	updateSerial();

	// SMS text content
	mySerial.print(String(count++) + ". Nachricht");
	updateSerial();
	mySerial.write(26);
  delay(60000);
}

void updateSerial()
{
	delay(200);
	while (Serial.available()) {
		// Forward what Serial received to Software Serial Port
		mySerial.write(Serial.read());
	}
	while(mySerial.available()) {
		// Forward what Software Serial received to Serial Port
		Serial.write(mySerial.read());
	}
}