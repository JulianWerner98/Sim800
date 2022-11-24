#include <Arduino.h>
#include <SoftwareSerial.h>

SoftwareSerial mySerial(8, 9); // RX, TX
int count = 1;
long intervall = 0;
boolean sendEnable = false;
boolean startSend = false;
#define RESET 2
#define LED 13
#define INTERVALL 30

// XX = country code, e.g. "49" for Germany and xxxxxxxxxxx = phone number
const char TELEFONE_NUMBER[] = "+491729999128";

String updateSerial(String);
void sendSMS(String);
void errorHandling();
void resetSim();

void setup()
{
	pinMode(RESET, OUTPUT);
	pinMode(LED, OUTPUT);
	Serial.begin(9600);
	mySerial.begin(9600);
	// Setup Timer Interrupt
	cli();		 // stop all interrupts
	TCNT2 = 0;	 // Timer Counter 2
	TCCR2A = 0;	 // Loesche Timer Counter Controll Register A
	TCCR2B = 0;	 // Loesche Timer Counter Controll Register B
	OCR2A = 155; // Setze Output Compare Register A
	// Setze CS20, CS21 und CS22 - Clock Select Bit 10,11,12 (Prescaler 1024)
	TCCR2B |= (1 << CS22) | (1 << CS21) | (1 << CS20);
	// CTC-Mode ein
	TCCR2A |= (1 << WGM21); // CTC-Mode (Clear Timer and Compare)
	// Timer/Counter Interrupt Mask Register
	TIMSK2 |= (1 << OCIE2A); // Output Compare A Match Interrupt Enable
	sei();					 // allow interrupts

	Serial.println("Initializing...");
	resetSim();

	delay(1000);
	errorHandling();
}

void loop()
{
	
	if(startSend) {
		sendSMS(String(count++) + ". Message");
		startSend = false;
	}
}

String updateSerial(String message)
{
	if (message.length())
		mySerial.println(message);
	String back = "";
	delay(200);
	while (Serial.available())
	{
		// Forward what Serial received to Software Serial Port
		mySerial.write(Serial.read());
	}
	while (mySerial.available())
	{
		// Forward what Software Serial received to Serial Port
		delay(20);
		int c = mySerial.read();
		if (c != 13 && c != 10)
			back += (char)c;
		Serial.write(c);
	}
	back = back.substring(message.length());
	// Serial.println("\n#" + back);
	return back;
}

void sendSMS(String message)
{
	Serial.println("Send SMS");
	errorHandling();
	// Configuring TEXT mode
	updateSerial("AT+CMGF=1");

	updateSerial("AT+CMGS=\"" + String(TELEFONE_NUMBER) + "\"");

	// SMS text content
	updateSerial(message);
	mySerial.write(26);
	String back = updateSerial("");
	Serial.println("->" + back + "<-");
	if (back.length())
	{
		errorHandling();
		sendSMS(message);
	}
}
void errorHandling()
{
	sendEnable = false;
	int fail = -1;
	String returnValue = "";
	do
	{
		delay(500);
		returnValue = updateSerial("AT");
		if (fail++ > 20)
		{
			resetSim();
			fail = -1;
		}
	} while (!returnValue.equals("OK"));
	Serial.println("Module Ready");
	fail = -1;
	do
	{
		delay(500);
		returnValue = updateSerial("AT+CPIN?");
		if (fail++ > 30)
		{
			resetSim();
			fail = -1;
		}
	} while (!returnValue.equals("+CPIN: READYOK"));
	Serial.println("Sim Ready");
	sendEnable = true;
}
void resetSim()
{
	Serial.println("Start Reset");
	digitalWrite(RESET, LOW);
	delay(1000);
	digitalWrite(RESET, HIGH);
	delay(10000);
	Serial.println("End Reset");
}
ISR(TIMER2_COMPA_vect)
{
	if (!(intervall % 100))
	{
		digitalWrite(LED, !digitalRead(LED));
	}
	if (intervall++ >= INTERVALL * 100)
	{
		intervall = 0;
		if (sendEnable)
		{
			startSend = true;
		}
	}
}