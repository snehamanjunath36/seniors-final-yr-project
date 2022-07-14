#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>



#include "ThingSpeak.h"


#include <EMailSender.h>




String a="Normal",b="Normal",c="Normal"; 

#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"


#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>




EMailSender emailSend("anusha.km.2000.ms@gmail.com", "owdwebpqqmqwlkbl");


const char* ssid = "hehe";  // Enter SSID here
const char* password = "12345678";  //Enter Password here

ESP8266WebServer server(80);



WiFiClient  client;

unsigned long myChannelNumber = 1771270;
const char * myWriteAPIKey = "KGVOGCLEVMYDW00Q";
String myStatus = "";


int ThermistorPin = 0;
int Vo;
float R1 = 10000;
float logR2, R2, T;
float c1 = 1.009249522e-03, c2 = 2.378405444e-04, c3 = 2.019202697e-07;



#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 


#define OLED_RESET     -1 
#define SCREEN_ADDRESS 0x3C 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);




MAX30105 particleSensor;

#define MAX_BRIGHTNESS 255

#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
//Arduino Uno doesn't have enough SRAM to store 100 samples of IR led data and red led data in 32-bit format
//To solve this problem, 16-bit MSB of the sampled data will be truncated. Samples become 16-bit data.
uint16_t irBuffer[100]; //infrared LED sensor data
uint16_t redBuffer[100];  //red LED sensor data
#else
uint32_t irBuffer[100]; //infrared LED sensor data
uint32_t redBuffer[100];  //red LED sensor data
#endif

int32_t bufferLength; //data length
int32_t spo2; //SPO2 value
int8_t validSPO2; //indicator to show if the SPO2 calculation is valid
int32_t heartRate; //heart rate value
int8_t validHeartRate; //indicator to show if the heart rate calculation is valid


void setup()
{
  Serial.begin(115200); // initialize serial communication at 115200 bits per second:
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);

}
display.clearDisplay();

display.setTextSize(1);      
  display.setTextColor(WHITE); 
  display.setCursor(1, 4);    
  display.println("Patient Monitoring System "); 
  display.display(); 
 delay(5000);
  display.clearDisplay();
    
  
  WiFi.begin(ssid, password);

  //check wi-fi is connected to wi-fi network
  while (WiFi.status() != WL_CONNECTED) {
  delay(1000);
  Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected..!");
  Serial.print("Got IP: ");  Serial.println(WiFi.localIP());

  server.on("/", handle_OnConnect);
  server.onNotFound(handle_NotFound);

  server.begin();
  Serial.println("HTTP server started");

  ThingSpeak.begin(client);


 

  // Initialize sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
  {
    Serial.println(F("MAX30105 was not found. Please check wiring/power."));
    while (1);
  }

  Serial.println(F("Attach sensor to finger with rubber band. Press any key to start conversion"));
//  while (Serial.available() == 0) ; //wait until user presses a key
//  Serial.read();

  byte ledBrightness = 60; //Options: 0=Off to 255=50mA
  byte sampleAverage = 4; //Options: 1, 2, 4, 8, 16, 32
  byte ledMode = 2; //Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
  byte sampleRate = 100; //Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
  int pulseWidth = 411; //Options: 69, 118, 215, 411
  int adcRange = 4096; //Options: 2048, 4096, 8192, 16384

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); //Configure sensor with these settings
}

void loop()
{
  
  bufferLength = 100; //buffer length of 100 stores 4 seconds of samples running at 25sps

  //read the first 100 samples, and determine the signal range
  for (byte i = 0 ; i < bufferLength ; i++)
  {
    while (particleSensor.available() == false) //do we have new data?
      particleSensor.check(); //Check the sensor for new data

    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample(); //We're finished with this sample so move to next sample

    Serial.print(F("red="));
    Serial.print(redBuffer[i], DEC);
    Serial.print(F(", ir="));
    Serial.println(irBuffer[i], DEC);
  }

  //calculate heart rate and SpO2 after first 100 samples (first 4 seconds of samples)
  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);

  //Continuously taking samples from MAX30102.  Heart rate and SpO2 are calculated every 1 second
  while (1)
  {
    server.handleClient();
    //dumping the first 25 sets of samples in the memory and shift the last 75 sets of samples to the top
    for (byte i = 25; i < 100; i++)
    {
      redBuffer[i - 25] = redBuffer[i];
      irBuffer[i - 25] = irBuffer[i];
    }

    //take 25 sets of samples before calculating the heart rate.
    for (byte i = 75; i < 100; i++)
    {
      while (particleSensor.available() == false) //do we have new data?
        particleSensor.check(); //Check the sensor for new data


      redBuffer[i] = particleSensor.getRed();
      irBuffer[i] = particleSensor.getIR();
      particleSensor.nextSample(); //We're finished with this sample so move to next sample

      //send samples and calculation result to terminal program through UART
      Serial.print(F("red="));
      Serial.print(redBuffer[i], DEC);
      Serial.print(F(", ir="));
      Serial.print(irBuffer[i], DEC);

      Serial.print(F(", HR="));
      Serial.print(heartRate, DEC);
      

      Serial.print(F(", HRvalid="));
      Serial.print(validHeartRate, DEC);

      Serial.print(F(", SPO2="));
      Serial.print(spo2, DEC);

      Serial.print(F(", SPO2Valid="));
      Serial.println(validSPO2, DEC);


      Vo = analogRead(ThermistorPin);
  R2 = R1 * (1023.0 / (float)Vo - 1.0);
  logR2 = log(R2);
  T = (1.0 / (c1 + c2*logR2 + c3*logR2*logR2*logR2));
  T = T - 273.15;
Serial.print("Temperature");
      Serial.println(T);

     if(T>40)
      {
       c="Emergency";
         //sendmail();
      }
 if(heartRate>120)
      {
         a="Emergency";
         //sendmail1();
      }
if(spo2>110)
      {
         b="Emergency";
         //sendmail2();
      }
     
    }

    //After gathering 25 new samples recalculate HR and SP02
    maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);



    
display.setCursor(1, 4);    
  display.print("heartRate: "); 
  display.println(heartRate);
  display.print("spo2: "); 
  display.println(spo2);
    display.print("Temperature: "); 
  display.println(T);
  
  display.display(); 
display.clearDisplay();
delay(100);

    

     ThingSpeak.setField(1, heartRate);
  ThingSpeak.setField(2, spo2);
  ThingSpeak.setField(3, T);


   ThingSpeak.setStatus(myStatus);


 int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
 if(x == 200){
    Serial.println("Channel update successful.");
 }
 else{
   Serial.println("Problem updating channel. HTTP error code " + String(x));
 }
  }
  
}

void handle_OnConnect() {
  server.send(200, "text/html", SendHTML(heartRate,spo2,T,a,b,c)); 
}

void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
}

String SendHTML(int heartRate, int spo2,int T,String a,String b,String c){
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr +="<head> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr +="<title>Patient Monitoring System</title>\n";
  ptr +="<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr +="body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;}\n";
  ptr +="p {font-size: 24px;color: #444444;margin-bottom: 10px;}\n";
 
  ptr +="</style>\n";
  ptr +="<script>\n";
  ptr +="setInterval(loadDoc,100);\n";
  ptr +="function loadDoc() {\n";
  ptr +="var xhttp = new XMLHttpRequest();\n";
  ptr +="xhttp.onreadystatechange = function() {\n";
  ptr +="if (this.readyState == 4 && this.status == 200) {\n";
  ptr +="document.getElementById(\"webpage\").innerHTML =this.responseText}\n";
  ptr +="};\n";
  ptr +="xhttp.open(\"GET\", \"/\", true);\n";
  ptr +="xhttp.send();\n";
  ptr +="}\n";
  ptr +="</script>\n";
  ptr +="</head>\n";
  ptr +="<body style=color:orange>\n";
  ptr +="<div id=\"webpage\">\n";
  ptr +="<h1>Patient Monitoring System</h1>\n";
  
  ptr +="<p>HeartRate: ";
  ptr +=(int)heartRate;
  ptr +="</p>";

   ptr +="<p>HeartRate: ";
  ptr +=(String)a;
  ptr +="</p>";
  
  ptr +="<p>spo2: ";
  ptr +=(int)spo2;
  ptr +="</p>";

 ptr +="<p>spo2: ";
  ptr +=(String)b;
  ptr +="</p>";
  
  ptr +="<p>Temperature: ";
  ptr +=(int)T;
  ptr +="</p>";


  ptr +="<p>Temperature: ";
  ptr +=(String)c;
  ptr +="</p>";
    
  ptr +="</div>\n";
  ptr +="</body>\n";
  ptr +="</html>\n";
  return ptr;
}




void sendmail()
{
  EMailSender::EMailMessage message;
  message.subject = "Alert!";
  message.message = "Temperature Alert";

  EMailSender::Response resp = emailSend.send("likhitakpatil179@gmail.com", message);

  Serial.println("Sending status: ");

  Serial.println(resp.status);//1
  Serial.println(resp.code);//0
//  int c = resp.status.toInt();
  if(resp.status)
  {

     Serial.print("Mail Sent");
     delay(5000);    
  }
  else
  {

     Serial.println("Error in sending mail, respnse code: ");
     Serial.print(resp.code);
     delay(5000);
  }
  Serial.println(resp.desc);
  
}




void sendmail1()
{
  EMailSender::EMailMessage message;
  message.subject = "Alert!";
  message.message = "HR Alert";

  EMailSender::Response resp = emailSend.send("likhitakpatil179@gmail.com", message);

  Serial.println("Sending status: ");

  Serial.println(resp.status);//1
  Serial.println(resp.code);//0
//  int c = resp.status.toInt();
  if(resp.status)
  {

     Serial.print("Mail Sent");
     delay(5000);    
  }
  else
  {

     Serial.println("Error in sending mail, respnse code: ");
     Serial.print(resp.code);
     delay(5000);
  }
  Serial.println(resp.desc);
  
}



void sendmail2()
{
  EMailSender::EMailMessage message;
  message.subject = "Alert!";
  message.message = "spo2 Alert";

  EMailSender::Response resp = emailSend.send("likhitakpatil179@gmail.com", message);

  Serial.println("Sending status: ");

  Serial.println(resp.status);//1
  Serial.println(resp.code);//0
//  int c = resp.status.toInt();
  if(resp.status)
  {

     Serial.print("Mail Sent");
     delay(5000);    
  }
  else
  {

     Serial.println("Error in sending mail, respnse code: ");
     Serial.print(resp.code);
     delay(5000);
  }
  Serial.println(resp.desc);
  
}
