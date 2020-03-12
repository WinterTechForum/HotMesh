#include <ESP8266WiFi.h>
#include <ESP8266WiFiMesh.h>
#include <TypeConversionFunctions.h>
#include <assert.h>

// DISPLAY-SPECIFIC
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     LED_BUILTIN // You don't think it be like it is, but it do.
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const int buttonPin = D5;

/**
 * Special display code (so we don't clutter up the craziness below even more
 */
int tick_count = 0;
int device_count = 0;
int send_count = 0;
int receive_count = 0;
 
int network_initialized = 0;
int time_to_boom = 0;

void displaySetup() {
  pinMode(buttonPin, INPUT);
  Serial.begin(115200);
  Serial.println();
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(1000); // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();
}

void drawStatus() {
  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE); // Draw white text
  
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  
  // Top bar information
  display.setTextSize(2);      // Normal 1:1 pixel scale
  display.setCursor(0, 0);     // Start at top-left corner
  display.print(F("HotMesh "));

  int anim = (tick_count / 20) % 4;
  switch (anim) {
    case 0: display.println("|"); break;
    case 1: display.println("/"); break;
    case 2: display.println("-"); break;
    case 3: display.println("\\"); break;
  }
  
  // display.drawLine(0, 15, display.width()-1, 15, SSD1306_WHITE);

  // Status string settings
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setCursor(0, 16);     // Start at top-left corner

  // Connection status
  display.print(F("Status: "));
  if (network_initialized) {
    display.println(F("CONNECTED"));
  } else {
    display.println(F("NO NETWORK"));
  }

  display.println();
 
  display.print(F("Snd: "));
  if (send_count == 0) {
    display.println(F("--"));
  } else {
    display.println(send_count);
  }
  display.print(F("Rcv: "));
  if (receive_count == 0) {
    display.println(F("--"));
  } else {
    display.println(receive_count);
  }

  int buttonState = digitalRead(buttonPin);
 
  display.println();
  display.print(F("Button: "));
  if (buttonState == HIGH) {
    display.println("HIGH");
    time_to_boom = 1;
  } else {
    display.println("LOW");
  }
 
  tick_count += 1;
  
  display.display();
}

/**
   NOTE: Although we could define the strings below as normal String variables,
   here we are using PROGMEM combined with the FPSTR() macro (and also just the F() macro further down in the file).
   The reason is that this approach will place the strings in flash memory which will help save RAM during program execution.
   Reading strings from flash will be slower than reading them from RAM,
   but this will be a negligible difference when printing them to Serial.

   More on F(), FPSTR() and PROGMEM:
   https://github.com/esp8266/Arduino/issues/1143
   https://arduino-esp8266.readthedocs.io/en/latest/PROGMEM.html
*/
const char exampleMeshName[] PROGMEM = "HotMesh_";
const char exampleWiFiPassword[] PROGMEM = "";

unsigned int requestNumber = 0;
unsigned int responseNumber = 0;

String manageRequest(const String &request, ESP8266WiFiMesh &meshInstance);
transmission_status_t manageResponse(const String &response, ESP8266WiFiMesh &meshInstance);
void networkFilter(int numberOfNetworks, ESP8266WiFiMesh &meshInstance);

/* Create the mesh node object */
ESP8266WiFiMesh meshNode = ESP8266WiFiMesh(manageRequest, manageResponse, networkFilter, FPSTR(exampleWiFiPassword), FPSTR(exampleMeshName), "lcd", false);

const int trigPin = 5;
const int echoPin = 6;

/**
   Callback for when other nodes send you a request

   @param request The request string received from another node in the mesh
   @param meshInstance The ESP8266WiFiMesh instance that called the function.
   @returns The string to send back to the other node
*/
String manageRequest(const String &request, ESP8266WiFiMesh &meshInstance) {
  // We do not store strings in flash (via F()) in this function.
  // The reason is that the other node will be waiting for our response,
  // so keeping the strings in RAM will give a (small) improvement in response time.
  // Of course, it is advised to adjust this approach based on RAM requirements.

  /* Print out received message */
  Serial.print("Request received: ");
  Serial.println(request);
  
  
  String response("<empty response>");
  
  Serial.print("Sending response: ");
  Serial.println(response);

  receive_count += 1;
  
  return response;
}

/**
   Callback for when you get a response from other nodes

   @param response The response string received from another node in the mesh
   @param meshInstance The ESP8266WiFiMesh instance that called the function.
   @returns The status code resulting from the response, as an int
*/
transmission_status_t manageResponse(const String &response, ESP8266WiFiMesh &meshInstance) {
  transmission_status_t statusCode = TS_TRANSMISSION_COMPLETE;

  /* Print out received message */
  Serial.print(F("Response received: "));
  Serial.println(response);

  return statusCode;
}

/**
   Callback used to decide which networks to connect to once a WiFi scan has been completed.

   @param numberOfNetworks The number of networks found in the WiFi scan.
   @param meshInstance The ESP8266WiFiMesh instance that called the function.
*/
void networkFilter(int numberOfNetworks, ESP8266WiFiMesh &meshInstance) {
  for (int networkIndex = 0; networkIndex < numberOfNetworks; ++networkIndex) {
    String currentSSID = WiFi.SSID(networkIndex);
    int meshNameIndex = currentSSID.indexOf(meshInstance.getMeshName());

    /* Connect to any _suitable_ APs which contain meshInstance.getMeshName() */
    if (meshNameIndex >= 0) {
      uint64_t targetNodeID = stringToUint64(currentSSID.substring(meshNameIndex + meshInstance.getMeshName().length()));

      ESP8266WiFiMesh::connectionQueue.push_back(NetworkInfo(networkIndex));
      /*if (targetNodeID < stringToUint64(meshInstance.getNodeID())) {
        ESP8266WiFiMesh::connectionQueue.push_back(NetworkInfo(networkIndex));
      }*/
    }
  }
}

void setup() {
  displaySetup();

  // Prevents the flash memory from being worn out, see: https://github.com/esp8266/Arduino/issues/1054 .
  // This will however delay node WiFi start-up by about 700 ms. The delay is 900 ms if we otherwise would have stored the WiFi network we want to connect to.
  WiFi.persistent(false);

  Serial.begin(115200);
  delay(50); // Wait for Serial.

  //yield(); // Use this if you don't want to wait for Serial.

  // The WiFi.disconnect() ensures that the WiFi is working correctly. If this is not done before receiving WiFi connections,
  // those WiFi connections will take a long time to make or sometimes will not work at all.
  WiFi.disconnect();

  Serial.println(F("Setting up mesh node..."));

  /* Initialise the mesh node */
  meshNode.begin();
  meshNode.activateAP(); // Each AP requires a separate server port.
  meshNode.setStaticIP(IPAddress(192, 168, 4, 23)); // Activate static IP mode to speed up connection times.
}

void makeRequest() {
  Serial.println(F("Sending request"));
  
  meshNode.attemptTransmission("BOOM", /*concludingDisconnect=*/false);
}

int32_t timeOfLastScan = -10000;
void loop() {
  drawStatus();
  if (millis() - timeOfLastScan > 100 // Give other nodes some time to connect between data transfers.
      || (WiFi.status() != WL_CONNECTED && millis() - timeOfLastScan > 2000)) { // Scan for networks with two second intervals when not already connected.

    if (time_to_boom) {
      makeRequest();
      time_to_boom = 0;
    }

    timeOfLastScan = millis();

    // One way to check how attemptTransmission worked out
    if (ESP8266WiFiMesh::latestTransmissionSuccessful()) {
      Serial.println(F("Transmission successful."));
    }

    // Another way to check how attemptTransmission worked out
    if (ESP8266WiFiMesh::latestTransmissionOutcomes.empty()) {
      Serial.println(F("No mesh AP found."));
    } else {
      for (TransmissionResult &transmissionResult : ESP8266WiFiMesh::latestTransmissionOutcomes) {
        if (transmissionResult.transmissionStatus == TS_TRANSMISSION_FAILED) {
          Serial.println(String(F("Transmission failed to mesh AP ")) + transmissionResult.SSID);
        } else if (transmissionResult.transmissionStatus == TS_CONNECTION_FAILED) {
          Serial.println(String(F("Connection failed to mesh AP ")) + transmissionResult.SSID);
        } else if (transmissionResult.transmissionStatus == TS_TRANSMISSION_COMPLETE) {
          // No need to do anything, transmission was successful.
        } else {
          Serial.println(String(F("Invalid transmission status for ")) + transmissionResult.SSID + String(F("!")));
          assert(F("Invalid transmission status returned from responseHandler!") && false);
        }
      }
    }
    Serial.println();
  } else {
    /* Accept any incoming connections */
    meshNode.acceptRequest();
  }
}
