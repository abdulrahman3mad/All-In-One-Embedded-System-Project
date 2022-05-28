#include <LiquidCrystal.h>
#include <SoftwareSerial.h>
#define TemperaturePin (0)
#define SmokePin (1)
#define LED (4)
int SmokeValue, TemperatureValue;
char isMotionDetected;
bool IsPageSent;
String IP;
LiquidCrystal LCD(5, 6, 7, 8, 9, 10, 11);
SoftwareSerial ESP(3, 4);

void setup() {  
  ConfigModule();
  delay(1000);
  PinsSettingSet();
  TimersSettingSet();
  LCDSettingSet();
  initADC();
} 

void PinsSettingSet(){
  DDRB |= (1<<LED);
  PORTB &= ~(1<<LED);
  PORTD &= ~(1<<PD2);
}

void TimersSettingSet(){
  //Timers Interrupt
  cli();
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;
  
  //2-Seconds Timer
  TCNT1L = 0xEE;
  TCNT1H = 0x85;
  TCCR1B = 0x05;
  TIMSK1 |= (1<<TOIE1);  
  //-----

  //External Interrupt
  EIMSK |= (1<<INT0);
  EICRA |= (1<<ISC00) | (1<<ISC01);
  //---
  
  sei();
}

void LCDSettingSet(){
  LCD.clear();
  LCD.begin(16, 2);
  LCD.print("Hello With BoB");
  LCD.setCursor(2, 1);
  LCD.print("  System");
  delay(500);
}


void ConfigModule(){
  ESP.begin(115200);
  Serial.begin(115200);
  delay(100);
  
  SendCommand("AT+RST"); // Reset It
  SendCommand("AT+CWLAP");
  SendCommand("AT+CWJAP=r,shady12345"); // Connect To LAN
  SendCommand("AT+CWMODE=1"); // Set it as a client
  SendCommand("AT+CIPMUX=1"); // Turn On multiple Channels 
  SendCommand("AT+CIPSERVER=1,80"); // Set it as a server and Turn on port 80 for HTTP
  IP = SendCommand("AT+CIFSR"); // Get The given IP
  Serial.print(IP);
}

 
ISR(TIMER1_OVF_vect){
  MonitorSensors();
  LCDWrite();
  TCNT1L = 0xEE;
  TCNT1H = 0x85;
} 

ISR(INT0_vect){
  isMotionDetected ^= 1;
  EICRA ^= (1<<ISC00);
}

void MonitorSensors(){
  TemperatureValue = TemperatureRead();
  ADMUX &= ~3;
  SmokeValue = SmokeRead();
  ADMUX &= ~3;
}


float TemperatureRead(){
  TemperatureValue = ADCStartConvertion(0);  
  TemperatureValue = TemperatureValue * 0.00488 * 100;   // ADC * ((Vref(5v) / 1024) * 1000)) / 10 
  return TemperatureValue;
}

float SmokeRead(){
  SmokeValue = ADCStartConvertion(1);
  return SmokeValue;
}

void LCDWrite(){
  LCD.clear();
  LCD.print("Smoke:");
  LCD.print(SmokeValue);

  LCD.print(" ");
  
  if(isMotionDetected)
    LCD.print("Mot: Y");
  else
    LCD.print("Mot: N");
    
  LCD.setCursor(2, 1);
  LCD.print("Temp:");
  LCD.print(TemperatureValue);
}

void initADC(){
  ADCSRA |= (1<<ADEN);
  ADMUX |= (1<<REFS0);
  ADCSRA |= (7<<ADPS0);
}

int ADCStartConvertion(uint8_t pinNum){
  ADMUX |= pinNum;
  ADCSRA |= (1<<ADSC);
  while(ADCSRA & (1<<ADSC)); 
  return ADC;
}  


void loop() {
   if(ESP.available() > 0){
      String InputData = ESP.readString();
      Serial.println(InputData);
      if(InputData.indexOf("GBut=ON") > 0)
          TurnLedOn();  
      else if(InputData.indexOf("GBut=OFF") > 0)
          TurnLedOff();  
      else if(InputData.indexOf("SEND OK") > 0){
        String WebPage = "<style>@import url('https://fonts.googleapis.com/css2?family=Cairo');form{text-align:center;}button{background-color:#006E7F;margin:10px;border:none;font-size:1.2rem;border-radius:4px;padding:1px 28px;color:white;font-weight:bold}*{font-family:'Cairo', sans-serif;}</style>";
        String Command = "AT+CIPSEND=0";
        //Command += InputData[7]-48-18;
        Command += ',';
        Command += WebPage.length();
        SendCommand(Command); // Set The Channel number, and length of string to be sent
        ESP.print(WebPage);  // Send the Web page
        delay(200);
        SendAbnormalConditions();
        delay(200);
        SendCommand("AT+CIPCLOSE=0");  
      }else {
        SendWebPage();
      }
   }
}

String SendCommand(String command){
  ESP.print(command);
  ESP.print("\r\n");
  delay(100);
  return ESP.readString();
}

void SendWebPage(){
  String WebPage = "<html><body><h1 style='color:grey;padding:3px;font-weight:bold;font-size:3rem;text-align:center;'>Embedded System Controller</h1><form method='get'><button class='GBut'name='GBut'value='ON'>Get Up</button><button class='SBut'name='GBut'value='OFF'>Sleep</button></form></body>";
  String Command = "AT+CIPSEND=0";
  Command += ',';
  Command += WebPage.length();
  Serial.println(Command);
  SendCommand(Command); // Set The Channel number, and length of string to be sent
  ESP.print(WebPage);  // Send the Web page
  delay(500);
}

void TurnLedOn(){
  Serial.println("LED is Turned ON");
  PORTB |= (1<<LED);
}

void TurnLedOff(){
  Serial.println("LED is Turned OFF");
  PORTB &= ~(1<<LED);
}

void SendAbnormalConditions(){
 if(TemperatureValue > 30){
    SendCommand("AT+CIPSEND=0,74");
    ESP.print("<script>alert('Temperature is higher than 30!! Oh My God')</script></html>");
  }else if(SmokeValue > 80){
    SendCommand("AT+CIPSEND=0,53");
    ESP.print("<script>alert('There is smoke!! run')</script></html>");
  }
}
