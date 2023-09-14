/*


 Face registration commands
 --------------------------
 Point the camera at a target face and enter the following commands into the serial monitor,
 Register face:           "REG={Name}"            Ensure that there is only one face detected in frame
 Exit registration mode:  "EXIT"                  Stop trying to register a face before it is successfully registered
 Reset registered faces:  "RESET"                 Forget all previously registered faces
 Backup registered faces to flash:    "BACKUP"    Save registered faces to flash
 Restore registered faces from flash: "RESTORE"   Load registered faces from flash



*/

#include "WiFi.h"
#include "StreamIO.h"
#include "VideoStream.h"
#include "RTSP.h"
#include "NNFaceDetectionRecognition.h"
#include "VideoStreamOverlay.h"
#include "SPI.h"
#include "AmebaILI9341.h"
#include "TJpg_Decoder.h"
#include "AmebaFatFS.h"

#define CHANNEL   0
#define CHANNELTFT   1
#define CHANNELNN 3

// Customised resolution for NN
#define NNWIDTH 576
#define NNHEIGHT 320

/*SPI TFT LCD*/
#define TFT_RESET       27//PF_1//5
#define TFT_DC          28//PF_2//4
#define TFT_CS          SPI1_SS
#define ILI9341_SPI_FREQUENCY 25000000
#define FILENAME "StImg_"

AmebaILI9341 tft = AmebaILI9341(TFT_CS, TFT_DC, TFT_RESET);

VideoSetting config(VIDEO_FHD, 30, VIDEO_H264, 0);
VideoSetting configNN(NNWIDTH, NNHEIGHT, 10, VIDEO_RGB, 0);
VideoSetting configTFT(VIDEO_VGA,CAM_FPS,VIDEO_JPEG,1);
uint32_t img_addr = 0;
uint32_t img_len = 0;
uint32_t count = 0;

NNFaceDetectionRecognition facerecog;
RTSP rtsp;
StreamIO videoStreamer(1, 1);
StreamIO videoStreamerFDFR(1, 1);
StreamIO videoStreamerRGBFD(1, 1);

char ssid[] = "your_ssid";    // your network SSID (name)
char pass[] = "your_password";       // your network password
int status = WL_IDLE_STATUS;

//Line notify
char server[] = "notify-api.line.me"; 
String LineToken = "your_token";
String message = "";

WiFiSSLClient client;

IPAddress ip;
int rtsp_portnum; 

/*  delay function
*/
unsigned long CurrentTime,preTime;
const int intervalSwitch = 5000;

unsigned long LineCurrentTime,LinepreTime;
const int LineintervalSwitch = (1000 * 60)* 1;
bool lineFlag = false;
bool SendLineMessageFlag = false;

unsigned long OpenCameraCurrentTime,OpenCamerapreTime;
const int OpenCameraintervalSwitch = 9000;
int LCD_AutoClosePin = 5;

bool FaceDetected = false;
uint16_t faceDetectedCount;
bool faceDetected = false;
uint32_t picturecount = 0;
// File Initialization
AmebaFatFS fs;


bool tft_output(int16_t x,int16_t y, uint16_t w, uint16_t h, uint16_t * bitmap)
{
    tft.drawBitmap(x,y,w,h,bitmap);

    return 1;
}


void setup() {
    Serial.begin(115200);

    SPI.setDefaultFrequency(ILI9341_SPI_FREQUENCY);
    pinMode(LCD_AutoClosePin,OUTPUT);
    
    digitalWrite(LCD_AutoClosePin,LOW);
    tft.begin();
    tft.setRotation(3);

    TJpgDec.setJpgScale(2);
    TJpgDec.setCallback(tft_output);

    // attempt to connect to Wifi network:
    while (status != WL_CONNECTED) {
        Serial.print("Attempting to connect to WPA SSID: ");
        Serial.println(ssid);
        status = WiFi.begin(ssid, pass);

        // wait 2 seconds for connection:
        delay(2000);
    }

    ip = WiFi.localIP();
    
    // Configure camera video channels with video format information
    // Adjust the bitrate based on your WiFi network quality
    config.setBitrate(2 * 1024 * 1024);     // Recommend to use 2Mbps for RTSP streaming to prevent network congestion
    Camera.configVideoChannel(CHANNEL, config);
    Camera.configVideoChannel(CHANNELNN, configNN);
    Camera.configVideoChannel(CHANNELTFT, configTFT);
    Camera.videoInit();

    Camera.channelBegin(CHANNELTFT);
    // Configure RTSP with corresponding video format information
    rtsp.configVideo(config);
    rtsp.begin();
    rtsp_portnum = rtsp.getPort();


    facerecog.configVideo(configNN);
    facerecog.modelSelect(FACE_RECOGNITION, NA_MODEL, DEFAULT_SCRFD, DEFAULT_MOBILEFACENET);
    facerecog.begin();
    facerecog.setResultCallback(FRPostProcess);

    videoStreamer.registerInput(Camera.getStream(CHANNEL));
    videoStreamer.registerOutput(rtsp);
    if (videoStreamer.begin() != 0) {
        Serial.println("StreamIO link start failed");
    }
    Camera.channelBegin(CHANNEL);

    // Configure StreamIO object to stream data from RGB video channel to face detection
    videoStreamerRGBFD.registerInput(Camera.getStream(CHANNELNN));
    videoStreamerRGBFD.setStackSize();
    videoStreamerRGBFD.setTaskPriority();
    videoStreamerRGBFD.registerOutput(facerecog);
    if (videoStreamerRGBFD.begin() != 0) {
        Serial.println("StreamIO link start failed");
    }
    // Start video channel for NN
    Camera.channelBegin(CHANNELNN);

    // Start OSD drawing on RTSP video channel
    OSD.configVideo(CHANNEL, config);
    OSD.begin();
}

void loop() {
    if (Serial.available() > 0) {
        String input = Serial.readString();   //透過 Serial 等待輸入
        input.trim();

        if (input.startsWith(String("REG="))){        //註冊名字。記憶辨識結果,註冊人臉時，請確保畫面中只有一張人臉。
            String name = input.substring(4);
            facerecog.registerFace(name);
        } else if (input.startsWith(String("EXIT"))) {  //輸入命令“EXIT”退出註冊模式。
            facerecog.exitRegisterMode();
        } else if (input.startsWith(String("RESET"))) { //輸入命令“RESET”以忘記所有先前註冊的臉部。所有先前分配的面孔和名稱都將被刪除。
            facerecog.resetRegisteredFace();
        } else if (input.startsWith(String("BACKUP"))) {  //輸入命令“BACKUP”將已註冊的面孔的副本保存到閃存。如果存在備份，請輸入命令“RESTORE”以從閃存加載已註冊的面孔。
            facerecog.backupRegisteredFace();
        } else if (input.startsWith(String("RESTORE"))) {
            facerecog.restoreRegisteredFace();
        }
    }


   CurrentTime = millis();
   if(CurrentTime - preTime > intervalSwitch){
		preTime = CurrentTime;
    OSD.createBitmap(CHANNEL);
    OSD.update(CHANNEL);
    
   }


    Camera.getImage(CHANNELTFT,&img_addr,&img_len);

    if(lineFlag) //line
    {
        if(SendLineMessageFlag == false )
        {
            SendLineMessageFlag = true;
            
            if(client.connect(server,443))
            {
                Serial.println("connected to server");
                // 使用 HTTP request:
                message =   "有陌生人在門口停留!!!";
                String query = "message=" + message;
                client.print("POST /api/notify HTTP/1.1\r\n");
                client.print("Host: " + String(server) +"\r\n"); 
                client.print("Authorization: Bearer " + LineToken + "\r\n"); 
                client.print("Content-Type: application/x-www-form-urlencoded\r\n");
                client.print("Content-Length: " + String(query.length()) + "\r\n");
                client.print("\r\n");
              
                client.print(query + "\r\n");      


            }

        }

        LineCurrentTime = millis();
        if(LineCurrentTime - LinepreTime > LineintervalSwitch)
        {
          LinepreTime = LineCurrentTime;
          
          lineFlag = false;
          SendLineMessageFlag = false;
      
         }

    } 

    if(faceDetected)       //偵測到未註冊的人臉為陌生人, 自動拍照存檔
    {
      Serial.println("save picture !!!!!!!");
      faceDetected = false;
      fs.begin();
      File file = fs.open(String(fs.getRootPath()) + String(FILENAME) + String(picturecount) + String(".jpg"));   
      
      file.write((uint8_t*)img_addr, img_len);
      delay(1);
      file.close();
      fs.end();
      picturecount++; 
    
     
    }
    
    TJpgDec.getJpgSize(0,0,(uint8_t *)img_addr,img_len);
    TJpgDec.drawJpg(0,0,(uint8_t *)img_addr,img_len);    
    
      if(FaceDetected)
      {

        /*open TFT LCD*/ 
        digitalWrite(LCD_AutoClosePin,HIGH); //偵測到陌生人臉時, 開啟 LCD 
        /*end */

        OpenCameraCurrentTime = millis();
        if(OpenCameraCurrentTime - OpenCamerapreTime > OpenCameraintervalSwitch){
          OpenCamerapreTime = OpenCameraCurrentTime;        
          FaceDetected = false;
          Serial.println("close TFT_LCD!!!!!!!");
          /*
            close TFT_LCD
          */          
          digitalWrite(LCD_AutoClosePin,LOW);  // 時間到,自動關閉 LCD TFT
        }

      }
}

// User callback function f
void FRPostProcess(std::vector<FaceRecognitionResult> results) {
    uint16_t im_h = config.height();
    uint16_t im_w = config.width();

    Serial.print("Network URL for RTSP Streaming: ");
    Serial.print("rtsp://");
    Serial.print(ip);
    Serial.print(":");
    Serial.println(rtsp_portnum);
    Serial.println(" ");

    printf("Total number of faces detected = %d\r\n", facerecog.getResultCount());
    OSD.createBitmap(CHANNEL);

    if (facerecog.getResultCount() > 0) {
        FaceDetected = true;

        for (uint32_t i = 0; i < facerecog.getResultCount(); i++) {
            FaceRecognitionResult item = results[i];
            int xmin = (int)(item.xMin() * im_w);
            int xmax = (int)(item.xMax() * im_w);
            int ymin = (int)(item.yMin() * im_h);
            int ymax = (int)(item.yMax() * im_h);

            uint32_t osdcolor;
            if (String(item.name()) == String("unknown")) {
                osdcolor = OSD_COLOR_RED;
                faceDetectedCount++;
                if(faceDetectedCount < 6)
                {
                  faceDetected = true;
                  faceDetectedCount = 0;
                }

                if(lineFlag == false)
                  lineFlag = true;                  
                  
            } else {
                osdcolor = OSD_COLOR_GREEN;
            }

          
            printf("Face %d name %s:\t%d %d %d %d\n\r", i, item.name(), xmin, xmax, ymin, ymax);
            OSD.drawRect(CHANNEL, xmin, ymin, xmax, ymax, 3, osdcolor);

            char text_str[40];
            snprintf(text_str, sizeof(text_str), "Face:%s", item.name());
            OSD.drawText(CHANNEL, xmin, ymin - OSD.getTextHeight(CHANNEL), text_str, osdcolor);
        }
        //FaceDetected = false;
    }

    OSD.update(CHANNEL);
}
