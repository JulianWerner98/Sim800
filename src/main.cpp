#include <SoftwareSerial.h>
#include <TM1637Display.h>
#include <Wire.h>
#include <DS3231.h>
#include <Adafruit_I2CDevice.h>
#include <SPI.h>

SoftwareSerial mySerial(15, 17); // RX, TX
TM1637Display display0(14, 8);
TM1637Display display1(14, 9);
TM1637Display display2(14, 10);
TM1637Display display3(14, 11);
TM1637Display display4(14, 12);
DS3231 myRTC;

long intervall = 0;
int counter = 0;
int tries = 0;
boolean pointActive = false;
boolean activ = false;
boolean incommingMessage = false;
int seconds = 0;

unsigned long start[5] = {97, 98, 1000, 600, 1371};
int duration[5] = {0, 0, 0, 0, 0};

#define INTERVALL 300 * 100 // in sek * 100
#define RESET 16
#define LED 13
#define BRIGTHNESS 15

// XX = country code, e.g. "49" for Germany and xxxxxxxxxxx = phone number
const char TELEFONE_NUMBER[] = "+491729999128";

String updateSerial(String);
void sendSMS(String);
void errorHandling();
void resetSim();
String getSMS();
void ring();
void handleRelais(String);
void showTime(int days, int hours, int minutes);
void displayOnSeq(TM1637Display &display, int differenz, int count);

int delayVal = 50;
int pause = 12;

bool century = false;
bool h12Flag;
bool pmFlag;

void setup()
{
  // Setzt die Helligkeit des Displays mögliche Werte sind 0 bis 15
  display0.setBrightness(BRIGTHNESS);
  display1.setBrightness(BRIGTHNESS);
  display2.setBrightness(BRIGTHNESS);
  display3.setBrightness(BRIGTHNESS);
  display4.setBrightness(BRIGTHNESS);
  uint8_t data[] = {0xff, 0xff, 0xff, 0xff};
  // Setzt die Anzahl der möglichen Segmente.
  display0.setSegments(data);
  display1.setSegments(data);
  display2.setSegments(data);
  display3.setSegments(data);
  display4.setSegments(data);

  pinMode(RESET, OUTPUT);
  digitalWrite(RESET, HIGH);
  pinMode(LED, OUTPUT);
  for (int i = 0; i <= 5; i++)
  {
    pinMode(i + 2, OUTPUT);
    digitalWrite(i + 2, LOW);
  }

  Serial.begin(9600);
  mySerial.begin(9600);
  Wire.begin();

  // Setup Timer Interrupt
  cli();       // stop all interrupts
  TCNT2 = 0;   // Timer Counter 2
  TCCR2A = 0;  // Loesche Timer Counter Controll Register A
  TCCR2B = 0;  // Loesche Timer Counter Controll Register B
  OCR2A = 100; // Setze Output Compare Register A
  // Setze CS20, CS21 und CS22 - Clock Select Bit 10,11,12 (Prescaler 1024)
  TCCR2B |= (1 << CS22) | (1 << CS21) | (1 << CS20);
  // CTC-Mode ein
  TCCR2A |= (1 << WGM21); // CTC-Mode (Clear Timer and Compare)
  // Timer/Counter Interrupt Mask Register
  TIMSK2 |= (1 << OCIE2A); // Output Compare A Match Interrupt Enable

  sei(); // allow interrupts

  Serial.println("Initializing...");

  // delay(60000);
  errorHandling();
  delay(10000);
  String signal = updateSerial("AT+CSQ");
  sendSMS("Module ready" + signal.substring(4, signal.length() - 2));
}

void loop()
{
  if (mySerial.available())
  {
    Serial.println("Data available");
    String message = mySerial.readString();
    Serial.println(message);
    handleRelais(message);
    message = "";
  }
  delay(100);
  int secondsNew = myRTC.getSecond();

  if (secondsNew != seconds)
  {
    int minutes = myRTC.getMinute();
    int hours = myRTC.getHour(h12Flag, pmFlag);
    int day = myRTC.getDate();
    pointActive = !pointActive;
    seconds = secondsNew;
    showTime(day, hours, minutes);
  }
}

void handleRelais(String message)
{
  message.toLowerCase();
  String sendMessage = "";
  if (message.indexOf("relais") > -1)
  {
    int index = message.indexOf("relais");
    int relais = message.substring(index + 6, index + 8).toInt();
    if (relais > 0 && relais < 6)
    {
      int indexOn = message.indexOf("on");
      boolean on = indexOn > -1;
      sendMessage = "Relais " + String(relais) + " is ";
      sendMessage += on ? "on" : "off";
      if (on)
      {
        Serial.print("Die Zahl ist: ");
        Serial.println(message.substring(indexOn + 3, indexOn + 5).toInt());
        // ToDo setze Zeit
        start[relais - 1] = myRTC.getDate() * 24 * 60 + myRTC.getHour(h12Flag, pmFlag) * 60 + myRTC.getMinute();
        duration[relais - 1] = message.substring(indexOn + 3, indexOn + 5).toInt();
        sendMessage += " for " + String(duration[relais - 1]) + " hours";
      }
      Serial.println(sendMessage);
      digitalWrite(1 + relais, on);
      sendSMS(sendMessage);
    }
    else
    {
      Serial.println("Relais " + String(relais) + " not found");
      sendSMS("Relais " + String(relais) + " not found");
    }
  }
}

String updateSerial(String message = "")
{
  if (message.length())
  {
    delay(500);
    mySerial.println(message);
  }
  String back = "";
  delay(500);
  while (Serial.available())
  {
    // Forward what Serial received to Software Serial Port
    mySerial.write(Serial.read());
  }
  while (mySerial.available())
  {
    // Forward what Software Serial received to Serial Port
    int c = mySerial.read();
    if (c != 13 && c != 10)
    {
      back += (char)c;
      // Serial.write(c);
    }
  }
  back = back.substring(message.length());
  Serial.println(back);
  return back;
}

void errorHandling()
{
  activ = false;
  Serial.println("Start Checking");
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
    if (fail++ > 50)
    {
      resetSim();
      fail = -1;
    }
  } while (!returnValue.equals("+CPIN: READYOK"));
  Serial.println("Sim Ready");

  fail = -1;
  do
  {
    delay(1000);
    returnValue = updateSerial("AT+CREG?");
    if (fail++ > 165)
    {
      resetSim();
      fail = -1;
    }
  } while ((!returnValue.equals("+CREG: 0,1OK")) && !returnValue.equals("+CREG: 0,5OK"));
  updateSerial("AT+CMGF=1");
  updateSerial("AT+CNMI=1,2,0,0,0");
  updateSerial("AT+CMGDA=\"DEL ALL\"");
  updateSerial("AT+CSQ");
  updateSerial("AT+CCID");

  Serial.println("Sim Registered");
  activ = true;
}

void resetSim()
{
  Serial.println("Start Reset");
  digitalWrite(RESET, LOW);
  delay(1000);
  digitalWrite(RESET, HIGH);
  delay(20000);
  Serial.println("End Reset");
}

void sendSMS(String message)
{
  Serial.println("Send SMS");
  errorHandling();

  updateSerial("AT+CMGS=\"" + String(TELEFONE_NUMBER) + "\"");

  // SMS text content
  String tmp = updateSerial(message);
  handleRelais(tmp);
  Serial.println(tmp);
  mySerial.write(26);
  String back = updateSerial();
  Serial.println("->" + back + "<-");
  if (back.length())
  {
    errorHandling();
    if (tries++ <= 15)
    {
      sendSMS(message);
    }
    else
    {
      errorHandling();
      sendSMS(message);
      tries = 0;
    }
  }
  tries = 0;
  Serial.println("Successfully sended");
}

String getSMS()
{
  digitalWrite(5, HIGH);
  String message = "";
  Serial.println("Get SMS");
  delay(5000);
  String sms = mySerial.readString();
  Serial.println("SMS is :" + sms);

  int index = sms.indexOf(10);
  message = sms.substring(index + 1, sms.length() - 2);
  Serial.println("Message is : " + message);
  updateSerial("AT+CMGDA=\"DEL ALL\"");
  return message;
}

void showTime(int days, int hours, int minutes)
{
  int currentTimeStamp = days * 24 * 60 + hours * 60 + minutes;
  int differenz = start[0] + duration[0] * 60 - currentTimeStamp;
  displayOnSeq(display0, differenz, 0);
  differenz = start[1] + duration[1] * 60 - currentTimeStamp;
  displayOnSeq(display1, differenz, 1);
  differenz = start[2] + duration[2] * 60 - currentTimeStamp;
  displayOnSeq(display2, differenz, 2);
  differenz = start[3] + duration[3] * 60 - currentTimeStamp;
  displayOnSeq(display3, differenz, 3);
  differenz = start[4] + duration[4] * 60 - currentTimeStamp;
  displayOnSeq(display4, differenz, 4);
}

void displayOnSeq(TM1637Display &display, int differenz, int count)
{
  if (differenz <= 0)
  {
    digitalWrite(count + 2, LOW);
    display.clear();
  }
  else
  {
    display.showNumberDecEx(differenz / 60 * 100 + differenz % 60, pointActive ? 0b01000000 : 0, true, 4, 0);
  }
}

ISR(TIMER2_COMPA_vect)
{
  if (counter++ > 100)
  {
    counter = 0;
    digitalWrite(LED, !digitalRead(LED));
  }
}