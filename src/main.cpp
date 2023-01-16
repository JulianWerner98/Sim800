#include <Arduino.h>
#include <SoftwareSerial.h>

SoftwareSerial mySerial(8, 9); // RX, TX
long intervall = 0;
int tries = 0;
boolean activ = false;
boolean incommingMessage = false;

#define INTERVALL 300 * 100 // in sek * 100
#define RESET 2
#define LED 13
#define INTERRUPT_PIN 3
#define INTERRUPT_LED 4
#define FAIL_LED 5

// XX = country code, e.g. "49" for Germany and xxxxxxxxxxx = phone number
const char TELEFONE_NUMBER[] = "+491729999128";

String updateSerial(String);
void sendSMS(String);
void errorHandling();
void resetSim();
String getSMS();
void ring();

void setup()
{
  pinMode(RESET, OUTPUT);
  digitalWrite(RESET, LOW);
  pinMode(LED, OUTPUT);
  pinMode(FAIL_LED, OUTPUT);
  pinMode(INTERRUPT_LED, OUTPUT);
  pinMode(INTERRUPT_PIN, INPUT);
  digitalWrite(FAIL_LED, HIGH);
  Serial.begin(9600);
  mySerial.begin(9600);

  // Setup Timer Interrupt
  cli();       // stop all interrupts
  TCNT2 = 0;   // Timer Counter 2
  TCCR2A = 0;  // Loesche Timer Counter Controll Register A
  TCCR2B = 0;  // Loesche Timer Counter Controll Register B
  OCR2A = 155; // Setze Output Compare Register A
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
  digitalWrite(5, LOW);
}

void loop()
{
  if (mySerial.available())
  {
    Serial.println("Data available");
    String message = mySerial.readString();
    Serial.println(message);
    message.toLowerCase();
    int index = message.indexOf("on");
    boolean on = index >= 0;
    String sendMessage = "";
    if (message.indexOf("relais1") > -1)
    {
      sendMessage = "Relais 1 is ";
      sendMessage += on ? "on" : "off";
      Serial.println(sendMessage);
      sendSMS(sendMessage);
      digitalWrite(INTERRUPT_LED, on);
    }
    else if (message.indexOf("relais2") > -1)
    {
      sendMessage = "Relais 2 is ";
      sendMessage += on ? "on" : "off";
      Serial.println(sendMessage);
      sendSMS(sendMessage);
      digitalWrite(FAIL_LED, on);
    }
    message = "";
  }
  delay(500);
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
  digitalWrite(RESET, HIGH);
  delay(1000);
  digitalWrite(RESET, LOW);
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
  digitalWrite(5, LOW);
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

ISR(TIMER2_COMPA_vect)
{
  if (!(intervall++ % 100))
  {
    digitalWrite(LED, !digitalRead(LED));
  }
}