#include <FlexiTimer2.h>
#include <Bridge.h>
#include <YunServer.h>
#include <YunClient.h>

#define AOUT1 18
#define AOUT2 19
#define BOUT1 20
#define BOUT2 21

// Listen to the default port 5555, the Yún webserver
// will forward there all the HTTP requests you send
YunServer server;

volatile int mode = 0;
volatile int count = 0, h_width = 2,l_width = 12; 

void setup() {
  // Bridge startup
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);
  Bridge.begin();
  digitalWrite(13, HIGH);
  Serial.begin(9600);
  pinMode(AOUT1,OUTPUT);
  pinMode(AOUT2,OUTPUT);
  pinMode(BOUT1,OUTPUT);
  pinMode(BOUT2,OUTPUT);
  digitalWrite(AOUT1,LOW);
  digitalWrite(AOUT2,LOW);
  digitalWrite(BOUT1,LOW);
  digitalWrite(BOUT2,LOW);
  // Listen for incoming connection only from localhost
  // (no one from the external network could connect)
  server.listenOnLocalhost();
  server.begin();
  
  FlexiTimer2::set(1,myPwm);
  FlexiTimer2::start();
}

void loop() {
  // Get clients coming from server
  YunClient client = server.accept();
  // There is a new client?
  if (client) {
    // Process request
    process(client);
    ////Serial.println("accses");
    // Close connection and free resources.
    client.stop();
  }
  delay(50); // Poll every 50ms
}

void process(YunClient client) {
  // read the command
  String command = client.readStringUntil('/');
  Serial.println(command);
  if (command == "FORWARD") {
    h_width = 2;
    l_width = 12;
    mode = 1;
  }
  if (command == "BACK") {
    h_width = 2;
    l_width = 12;
    mode = 2;
  }
  if (command == "STOP") {
    mode = 0;
  }
  if (command == "LEFT"){
    h_width = 3;
    l_width = 11;
    mode = 4;
  }
  if (command == "RIGHT"){
    h_width = 3;
    l_width = 11;
    mode = 3;
  }
}

void myPwm(){
  if(count == 0){
    switch(mode){
      case 0:
        digitalWrite(AOUT1,LOW);
        digitalWrite(AOUT2,LOW);
        digitalWrite(BOUT1,LOW);
        digitalWrite(BOUT2,LOW);
        break;
      case 1:
        digitalWrite(AOUT1,LOW);
        digitalWrite(AOUT2,HIGH);
        digitalWrite(BOUT1,HIGH);
        digitalWrite(BOUT2,LOW);
        break;
      case 2:
        digitalWrite(AOUT1,HIGH);
        digitalWrite(AOUT2,LOW);
        digitalWrite(BOUT1,LOW);
        digitalWrite(BOUT2,HIGH);
        break;
      case 3:
        digitalWrite(AOUT1,LOW);
        digitalWrite(AOUT2,HIGH);
        digitalWrite(BOUT1,LOW);
        digitalWrite(BOUT2,HIGH);
        break;
      case 4:
        digitalWrite(AOUT1,HIGH);
        digitalWrite(AOUT2,LOW);
        digitalWrite(BOUT1,HIGH);
        digitalWrite(BOUT2,LOW);
        break;  
    }
    count++;
  }
  else if(count == h_width){
    digitalWrite(AOUT1,LOW);
    digitalWrite(AOUT2,LOW);
    digitalWrite(BOUT1,LOW);
    digitalWrite(BOUT2,LOW);
    count++;
  }
  else if(count >= (h_width + l_width)){
    count = 0;  
  }
  else{
    count++;  
  }
}



