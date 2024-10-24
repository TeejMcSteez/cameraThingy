/*
TODO:
Figure out how I would like to sense motion upon detection or just act as a full time camera taking pictures at a set rate and displaying that feed to the server

Also need to figure out if I want to store any of the data and if I do how to do that properly. Do I want to do a read/write/delete overflow scheme or have a function 
that saves only the pictures I want to save? 

For now I think I will keep it as a kind of CCTV live cam (shitty cctv) and perhaps send the jpg's to another computer to store at mass later after 
I make a cover for it so it can go outside and figure out if I want it to run of battery/solar/or wired. 
*/

//GPT-o1 Skeleton
#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>  // Include the mDNS library

// Replace these with your WiFi credentials
const char* ssid = "Your_WIFI";
const char* password = "Your_PASSWORD";

// Camera configuration for ESP32-CAM
#define PWDN_GPIO_NUM    32
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM     0
#define SIOD_GPIO_NUM    26
#define SIOC_GPIO_NUM    27

#define Y9_GPIO_NUM      35
#define Y8_GPIO_NUM      34
#define Y7_GPIO_NUM      39
#define Y6_GPIO_NUM      36
#define Y5_GPIO_NUM      21
#define Y4_GPIO_NUM      19
#define Y3_GPIO_NUM      18
#define Y2_GPIO_NUM       5
#define VSYNC_GPIO_NUM   25
#define HREF_GPIO_NUM    23
#define PCLK_GPIO_NUM    22

WebServer server(80);

void startCameraServer();

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  // Initialize mDNS service
  if (!MDNS.begin("esp32cam")) {  // Set the hostname to "esp32cam"
    Serial.println("Error setting up MDNS responder!");
  } else {
    Serial.println("MDNS responder started");
    Serial.println("You can now access the camera stream at: http://esp32cam.local/");
  }

  // Camera initialization
  camera_config_t config;
  config.ledc_channel   = LEDC_CHANNEL_0;
  config.ledc_timer     = LEDC_TIMER_0;
  config.pin_d0         = Y2_GPIO_NUM;
  config.pin_d1         = Y3_GPIO_NUM;
  config.pin_d2         = Y4_GPIO_NUM;
  config.pin_d3         = Y5_GPIO_NUM;
  config.pin_d4         = Y6_GPIO_NUM;
  config.pin_d5         = Y7_GPIO_NUM;
  config.pin_d6         = Y8_GPIO_NUM;
  config.pin_d7         = Y9_GPIO_NUM;
  config.pin_xclk       = XCLK_GPIO_NUM;
  config.pin_pclk       = PCLK_GPIO_NUM;
  config.pin_vsync      = VSYNC_GPIO_NUM;
  config.pin_href       = HREF_GPIO_NUM;
  config.pin_sscb_sda   = SIOD_GPIO_NUM;
  config.pin_sscb_scl   = SIOC_GPIO_NUM;
  config.pin_pwdn       = PWDN_GPIO_NUM;
  config.pin_reset      = RESET_GPIO_NUM;
  config.xclk_freq_hz   = 20000000;
  config.pixel_format   = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size    = FRAMESIZE_VGA;
    config.jpeg_quality  = 10;
    config.fb_count      = 2;
  } else {
    config.frame_size    = FRAMESIZE_CIF;
    config.jpeg_quality  = 12;
    config.fb_count      = 1;
  }

  // Initialize the camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  // Start the camera server
  startCameraServer();
}

void loop() {
  server.handleClient();
}

// Function to start the camera web server
void startCameraServer() {
  // Handler for the root URL "/"
server.on("/", HTTP_GET, []() {
    String html = "<html><head><title>ESP32-CAM</title> <meta http-equiv='refresh' content='5' /> <style>body{background-color:black; text-align:center;} h1{color: white;}</style> </head><body> <h1>ESP32-CAM Image</h1> <img src='/capture' /> </body></html>";
    server.send(200, "text/html", html);
});

  // Handler for the image capture URL "/capture"
  server.on("/capture", HTTP_GET, []() {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      server.send(500, "text/plain", "Camera capture failed");
      return;
    }

    // Send response directly to client
    WiFiClient client = server.client();
    if (!client.connected()) {
      Serial.println("Client disconnected before sending data");
      esp_camera_fb_return(fb);
      return;
    }

    // Construct and send the HTTP response manually
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: image/jpeg");
    client.println("Content-Length: " + String(fb->len));
    client.println("Connection: close");
    client.println();

    // Send the image data directly to the client
    size_t bytesSent = 0;
    while (bytesSent < fb->len) {
      size_t chunkSize = client.write(fb->buf + bytesSent, fb->len - bytesSent);//if chunk size is bigger than packet send it will send the max and subtract that from the total fb to send the rest of the fb after the first
      if (chunkSize == 0) {
        Serial.println("Failed to send image data");
        break;
      } 
      bytesSent += chunkSize;
      Serial.print("Bytes Sent: ");
      Serial.println(bytesSent);
    }

    // Return the frame buffer back to the camera to be reused
    esp_camera_fb_return(fb);

    // Close the connection to ensure the client receives the entire image
    client.stop();
  });

  // Start the web server
  server.begin();
}
