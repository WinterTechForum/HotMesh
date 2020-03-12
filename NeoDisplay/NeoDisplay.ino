#include <ESP8266WiFi.h>
#include <ESP8266WiFiMesh.h>
#include <TypeConversionFunctions.h>
#include <assert.h>

// NeoPixel Ring simple sketch (c) 2013 Shae Erisson
// Released under the GPLv3 license to match the rest of the
// Adafruit NeoPixel library

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
 #include <avr/power.h> // Required for 16 MHz Adafruit Trinket
#endif

// Which pin on the Arduino is connected to the NeoPixels?
#define NP_PIN        14 // On Trinket or Gemma, suggest changing this to 1

// How many NeoPixels are attached to the Arduino?
#define PIXEL_COUNT 8*4

// When setting up the NeoPixel library, we tell it how many pixels,
// and which pin to use to send signals. Note that for older NeoPixel
// strips you might need to change the third parameter -- see the
// strandtest example for more information on possible values.
Adafruit_NeoPixel pixels(PIXEL_COUNT, NP_PIN, NEO_GRB + NEO_KHZ800);

uint32_t pixelPaintRGB(int red, int green, int blue) {
  // pixels.Color() takes RGB values, from 0,0,0 up to 255,255,255
  uint32_t color = pixels.Color(red, green, blue);

  // The first NeoPixel in a strand is #0, second is 1, all the way up
  // to the count of pixels minus one.
  for (int i=0; i<PIXEL_COUNT; i++) {
    pixels.setPixelColor(i, color);
    //pixels.show();   // Send the updated pixel colors to the hardware.
    //delay(10);
  }
  pixels.show();   // Send the updated pixel colors to the hardware.
  return color;
}

uint32_t pixelPaintHue(uint32_t value) {
  uint32_t color = pixels.ColorHSV(value, 255, 63);

  // The first NeoPixel in a strand is #0, second is 1, all the way up
  // to the count of pixels minus one.
  for (int i=0; i<PIXEL_COUNT; i++) {
    pixels.setPixelColor(i, color);
    //pixels.show();   // Send the updated pixel colors to the hardware.
    //delay(10);
  }
  pixels.show();   // Send the updated pixel colors to the hardware.
  return color;
}

uint32_t pixelJoystickRGB(const String& msg) {
  int js_val = atoi(msg.c_str());
  int red = js_val / 16;
  int green = (js_val / 4) % 32;
  int blue = js_val % 32;
  pixelPaintRGB(red, green, blue);
  char buff[33];
  Serial.print(F("RGB: ("));
  Serial.print(itoa(red, buff, 16));
  Serial.print(F(","));
  Serial.print(itoa(green, buff, 16));
  Serial.print(F(","));
  Serial.print(itoa(blue, buff, 16));
  Serial.println(F(")"));
}

uint32_t pixelJoystick(const String& msg) {
  int js_val = atoi(msg.c_str());
  pixelPaintHue(js_val * 64);
}

uint32_t pixelBoom() {
  for (int i=0; i < 10 ;i++) {
    for (int v=0; v <= 255; v+=10) {
      pixelPaintRGB(v, v, v);      
      delay(5);
    }
    for (int v=255; v >= 0; v-=10) {
      pixelPaintRGB(v, v, v);            
      delay(5);
    }    
  }
  pixels.clear();
  return 0;
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
ESP8266WiFiMesh meshNode = ESP8266WiFiMesh(manageRequest, manageResponse, networkFilter, FPSTR(exampleWiFiPassword), FPSTR(exampleMeshName), "display", true);

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

  Serial.println(F("manageRequest():"));

  /* Print out received message */
  Serial.print("Request received: ");
  Serial.println(request);

  uint32_t color = 0;
  if (request == "BOOM") {
    color = pixelBoom();
  } else {
    color = pixelJoystick(request);    
  }
  Serial.println("Request processed");

  /* return a string to send back */
  return ("Display response Color:" + String(color) + " from " + meshInstance.getMeshName() + meshInstance.getNodeID() + ".");
  //responseNumber++
}

/**
   Callback for when you get a response from other nodes

   @param response The response string received from another node in the mesh
   @param meshInstance The ESP8266WiFiMesh instance that called the function.
   @returns The status code resulting from the response, as an int
*/
transmission_status_t manageResponse(const String &response, ESP8266WiFiMesh &meshInstance) {
  transmission_status_t statusCode = TS_TRANSMISSION_COMPLETE;

  Serial.println(F("manageResponse():"));
  /* Print out received message */
  Serial.print(F("Request sent: "));
  Serial.println(meshInstance.getMessage());
  Serial.print(F("Response received: "));
  Serial.println(response);

  // Our last request got a response, so time to create a new request.
  meshInstance.setMessage(String(F("Hello world request #")) + String(++requestNumber) + String(F(" from "))
                          + meshInstance.getMeshName() + meshInstance.getNodeID() + String(F(".")));

  // (void)meshInstance; // This is useful to remove a "unused parameter" compiler warning. Does nothing else.
  return statusCode;
}

/**
   Callback used to decide which networks to connect to once a WiFi scan has been completed.

   @param numberOfNetworks The number of networks found in the WiFi scan.
   @param meshInstance The ESP8266WiFiMesh instance that called the function.
*/
void networkFilter(int numberOfNetworks, ESP8266WiFiMesh &meshInstance) {
  Serial.println(F("networkFilter():"));
  for (int networkIndex = 0; networkIndex < numberOfNetworks; ++networkIndex) {
    String currentSSID = WiFi.SSID(networkIndex);
    int meshNameIndex = currentSSID.indexOf(meshInstance.getMeshName());

    /* Connect to any _suitable_ APs which contain meshInstance.getMeshName() */
    if (meshNameIndex >= 0) {
      uint64_t targetNodeID = stringToUint64(currentSSID.substring(meshNameIndex + meshInstance.getMeshName().length()));

      if (targetNodeID < stringToUint64(meshInstance.getNodeID())) {
        ESP8266WiFiMesh::connectionQueue.push_back(NetworkInfo(networkIndex));
      }
    }
  }
}

void setup() {
  // Prevents the flash memory from being worn out, see: https://github.com/esp8266/Arduino/issues/1054 .
  // This will however delay node WiFi start-up by about 700 ms. The delay is 900 ms if we otherwise would have stored the WiFi network we want to connect to.
  WiFi.persistent(false);

  Serial.begin(115200);
  delay(50); // Wait for Serial.

  //yield(); // Use this if you don't want to wait for Serial.

  // The WiFi.disconnect() ensures that the WiFi is working correctly. If this is not done before receiving WiFi connections,
  // those WiFi connections will take a long time to make or sometimes will not work at all.
  WiFi.disconnect();

  Serial.println();
  Serial.println();

  Serial.println(F("Note that this library can use static IP:s for the nodes to speed up connection times.\n"
                   "Use the setStaticIP method as shown in this example to enable this.\n"
                   "Ensure that nodes connecting to the same AP have distinct static IP:s.\n"
                   "Also, remember to change the default mesh network password!\n\n"));

  Serial.println(F("Setting up mesh node..."));

  /* Initialise the mesh node */
  meshNode.begin();
  meshNode.activateAP(); // Each AP requires a separate server port.
  meshNode.setStaticIP(IPAddress(192, 168, 4, 24)); // Activate static IP mode to speed up connection times.

  pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
  pixels.setBrightness(64);

  //for (int h = 0; h < 65536; h += 64) {
  //  pixelPaintHue(h);
  //  delay(10);
  //}
  //delay(1000);
  pixelBoom();
  pixelPaintRGB(40, 64, 0);
}

int32_t timeOfLastScan = -10000;
void loopX() {
  if (millis() - timeOfLastScan > 3000 // Give other nodes some time to connect between data transfers.
      || (WiFi.status() != WL_CONNECTED && millis() - timeOfLastScan > 2000)) { // Scan for networks with two second intervals when not already connected.

    String request = String(F("Hello world request #")) + String(requestNumber) + String(F(" from ")) + meshNode.getMeshName() + meshNode.getNodeID() + String(F("."));

    //pixelPaint(0, 30, 10); // Greenish aqua
    pixelPaintRGB(0, 10, 20); // Dim bluish aqua
    meshNode.attemptTransmission(request, false);
    timeOfLastScan = millis();

    // One way to check how attemptTransmission worked out
    if (ESP8266WiFiMesh::latestTransmissionSuccessful()) {
      Serial.println(F("Transmission successful."));
      pixelPaintRGB(0, 64, 20);  // Bright Greenish aqua
    }

    // Another way to check how attemptTransmission worked out
    if (ESP8266WiFiMesh::latestTransmissionOutcomes.empty()) {
      Serial.println(F("No mesh AP found."));
      pixelPaintRGB(60, 0, 0);  // Bright red
      pixelPaintRGB(50, 0, 10); // Bright fuschia
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

void loop() {
    /* Accept any incoming connections */
    meshNode.acceptRequest();
}
