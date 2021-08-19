/***
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-cam-take-photo-display-web-server/
  
  IMPORTANT!!! 
   - Select Board "AI Thinker ESP32-CAM"
   - GPIO 0 must be connected to GND to upload a sketch
   - After connecting GPIO 0 to GND, press the ESP32-CAM on-board RESET button to put your board in flashing mode
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
***/

#include "WiFi.h"
#include <ESP32Servo.h>
#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"
#include <ESPAsyncWebServer.h>
#include <StringArray.h>
#include <SPIFFS.h>
#include <FS.h>
#include <Base64.h>

// Replace with your network credentials
const char* ssid = "your-ssid";
const char* password = "your-password";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

boolean takeNewPhoto = false;
boolean isAuthorised = false;
boolean isOpen = false;

// Photo File Name to save in SPIFFS
#define FILE_PHOTO "/photo.jpg"

// OV2640 camera module pins (CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
#define SERVO_GPIO_NUM    2
#define led 13
#define PWM_CHANNEL 10
#define PWM_FREQUENCY 50
#define PWM_RESOLUTION 10

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>

<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body {
            text-align: center;
        }

        .vert {
            margin-bottom: 10%;
        }

        .hori {
            margin-bottom: 0%;
        }
    </style>
    <script src="https://unpkg.com/vue"></script>
    <script src="https://unpkg.com/axios@0.2.1/dist/axios.min.js"></script>
</head>

<body>
    <div id="app">
        <h2>ESP32-CAM Last Photo</h2>
        <p>It might take more than 5 seconds to capture a photo.</p>
        <p>
            <button onclick="rotatePhoto();">ROTATE</button>
            <button onclick="capturePhoto()">CAPTURE PHOTO</button>
            <button onclick="location.reload();">REFRESH PAGE</button>
        </p>
        <img src="image" id="image" width="70%">


        <div v-if="!image">
            <h2>Select an image</h2>

            <input type="file" @change="onFileChange">
            <button v-if="!uploadURL" @click="uploadImage">Upload image</button>
        </div>
        <div v-else>
            <button v-if="!uploadURL" @click="removeImage">Remove image</button>
            <button v-if="!uploadURL" @click="uploadImage">Upload image</button>
        </div>
        <h2 v-if="uploadURL">Success! Image uploaded to bucket.</h2>
    </div>

</body>

<script>
    const MAX_IMAGE_SIZE = 1000000

    /* ENTER YOUR ENDPOINT HERE */

    const API_ENDPOINT = 'https://twxehmv651.execute-api.ap-southeast-1.amazonaws.com/default/CompareFaces-IoTS_project_ET0731' // e.g. https://ab1234ab123.execute-api.us-east-1.amazonaws.com/uploads
    new Vue({
        el: "#app",
        data: {
            image: '',
            uploadURL: ''
        },
        methods: {
            onFileChange(e) {
                let files = e.target.files || e.dataTransfer.files
                if (!files.length) return
                console.log("On File change");
                this.createImage(files[0])
            },
            createImage(file) {
                // var image = new Image()
                let reader = new FileReader()
                reader.onload = (e) => {
                    console.log('length: ', e.target.result.includes('data:image/jpeg'))
                    if (!e.target.result.includes('data:image/jpeg')) {
                        return alert('Wrong file type - JPG only.')
                    }
                    if (e.target.result.length > MAX_IMAGE_SIZE) {
                        return alert('Image is loo large.')
                    }
                    this.image = e.target.result
                    console.log("Image: ", this.image);
                }
                reader.readAsDataURL(file)
            },
            createImageFromurl: async (url, thisVue) => {
                // Implement me
                var xhr = new XMLHttpRequest();
                xhr.open('get', url);
                xhr.responseType = 'blob';
                xhr.onload = async function () {
                    var fr = new FileReader();

                    fr.onload = function () {
                        thisVue.image = this.result;
                    };

                    await fr.readAsDataURL(xhr.response); // async call
                };
                xhr.send();
            },
            removeImage: function (e) {
                console.log('Remove clicked')
                this.image = ''
            },

            uploadImage: async function (e) {
                console.log('Upload clicked')

                if (!this.image) {
                    await this.createImageFromurl('/image', this); // TODO:

                    await new Promise(r => setTimeout(r, 5000));
                }
                // Get the presigned URL

                //console.log('Response: ', response)
                console.log('Uploading: ', this.image)
                let imageData = this.image.split(',')[1]
                console.log('Uploading to: ', API_ENDPOINT)
                const result = await fetch(API_ENDPOINT, {
                    method: 'POST',
                    body: JSON.stringify({
                        'image': {
                            "data": imageData
                        }
                    })
                })
                console.log('Result: ', result)
                body = await result.json();
                if (body["personIsRecognised"]) {
                    sendAuthorisation();
                }
            }
        }
    })
</script>
<script>
    var deg = 0;
    function capturePhoto() {
        var xhr = new XMLHttpRequest();
        xhr.open('GET', "/capture", true);
        xhr.send();
    }

    function sendAuthorisation() {
        var xhr = new XMLHttpRequest();
        xhr.open('GET', "/authorised", true);
        xhr.send();
    }

    function rotatePhoto() {
        var img = document.getElementById("photo");
        deg += 90;
        if (isOdd(deg / 90)) { document.getElementById("container").className = "vert"; }
        else { document.getElementById("container").className = "hori"; }
        img.style.transform = "rotate(" + deg + "deg)";
    }
    function isOdd(n) { return Math.abs(n % 2) == 1; }
</script>

</html>)rawliteral";

void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);
  pinMode(led, OUTPUT); 

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    ESP.restart();
  }
  else {
    delay(500);
    Serial.println("SPIFFS mounted successfully");
  }

  // Print ESP32 Local IP Address
  Serial.print("IP Address: http://");
  Serial.println(WiFi.localIP());

  ledcSetup(PWM_CHANNEL, PWM_FREQUENCY, PWM_RESOLUTION);
  ledcAttachPin( SERVO_GPIO_NUM, PWM_CHANNEL);

  // Turn-off the 'brownout detector'
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  // OV2640 camera module
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  }

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", index_html);
  });

  server.on("/capture", HTTP_GET, [](AsyncWebServerRequest * request) {
    takeNewPhoto = true;
    request->send_P(200, "text/plain", "Taking Photo");
  });

  server.on("/image", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, FILE_PHOTO, "image/jpg", false);
  });

  server.on("/authorised", HTTP_GET, [](AsyncWebServerRequest * request) {
    isAuthorised = true;
    request->send(200, "text/plain", "authorised");
  });

  // Start server
  server.begin();

}


bool checkPhoto( fs::FS &fs ) {
  File f_pic = fs.open( FILE_PHOTO );
  unsigned int pic_sz = f_pic.size();
  return ( pic_sz > 100 );
}

// Capture Photo and Save it to SPIFFS
void capturePhotoSaveSpiffs( void ) {
  camera_fb_t * fb = NULL; // pointer
  bool ok = 0; // Boolean indicating if the picture has been taken correctly

  do {
    // Take a photo with the camera
    Serial.println("Taking a photo...");

    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      return;
    }

    // Photo file name
    Serial.printf("Picture file name: %s\n", FILE_PHOTO);
    File file = SPIFFS.open(FILE_PHOTO, FILE_WRITE);

    // Insert the data in the photo file
    if (!file) {
      Serial.println("Failed to open file in writing mode");
    }
    else {
      file.write(fb->buf, fb->len); // payload (image), payload length
      Serial.print("The picture has been saved in ");
      Serial.print(FILE_PHOTO);
      Serial.print(" - Size: ");
      Serial.print(file.size());
      Serial.println(" bytes");
    }
    // Close the file
    file.close();
    esp_camera_fb_return(fb);

    // check if file has been correctly saved in SPIFFS
    ok = checkPhoto(SPIFFS);
  } while ( !ok );
}

void userAuthorised( void ) {
  ledcWrite( PWM_CHANNEL, 100);
  digitalWrite(led, HIGH);
  delay(2000);
  ledcWrite( PWM_CHANNEL, 50);
  delay(15);
  digitalWrite(led, LOW);
} 

void loop() {
  if (takeNewPhoto) {
    capturePhotoSaveSpiffs();
    takeNewPhoto = false;
  }
  if(isAuthorised) {
    userAuthorised();
    isAuthorised = false;
  }

  delay(1);
}
