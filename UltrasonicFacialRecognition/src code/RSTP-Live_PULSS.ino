

#include "WiFi.h"
#include "StreamIO.h"
#include "VideoStream.h"
#include "AudioStream.h"
#include "AudioEncoder.h"
#include "RTSP.h"
#include "NNFaceDetectionRecognition.h"
#include "VideoStreamOverlay.h"
#include "LiquidCrystal_PCF8574.h"

// Default audio preset configurations:
// 0 :  8kHz Mono Analog Mic
// 1 : 16kHz Mono Analog Mic
// 2 :  8kHz Mono Digital PDM Mic
// 3 : 16kHz Mono Digital PDM Mic

//set LCD
LiquidCrystal_PCF8574 lcd(0x27);

//腳位設定
//HC-SR04
const int trigPin = 18;
const int echoPin = 19;
//LED
const int R = 8;
const int G = 7;
const int B = 6;
// Choose between using AAC or G711 audio encoder
AAC encoder;
//G711E encoder;

AudioSetting configA(3);
Audio audio;
RTSP rtsp;
StreamIO audioStreamer1(1, 1);   // 1 Input Audio -> 1 Output encoder
StreamIO audioStreamer2(1, 1);   // 1 Input encoder -> 1 Output RTSP


#define CHANNEL     0
#define CHANNELNN   3

// Customised resolution for NN
#define NNWIDTH     576
#define NNHEIGHT    320

VideoSetting config(VIDEO_FHD, 30, VIDEO_H264, 0);
VideoSetting configNN(NNWIDTH, NNHEIGHT, 10, VIDEO_RGB, 0);
NNFaceDetectionRecognition facerecog;

StreamIO videoStreamer(1, 1);
StreamIO videoStreamerFDFR(1, 1);
StreamIO videoStreamerRGBFD(1, 1);

char ssid[] = "vivo Y55s 5G";    // your network SSID (name)
char pass[] = ".00000000";   // your network password
int status = WL_IDLE_STATUS;

IPAddress ip;
int rtsp_portnum; 

void setup() {
   Serial.begin(115200);
   
   // 設定超音波模組的引腳
   pinMode(trigPin, OUTPUT);
   pinMode(echoPin, INPUT);
   // 設定LED燈的引腳
   pinMode(R, OUTPUT);
   pinMode(G, OUTPUT);
   pinMode(B, OUTPUT);
   //LCD顯示
  
   lcd.begin(16, 2);
   lcd.setBacklight(200);
   lcd.clear();
   lcd.setCursor(0,0);
   lcd.print("Hello");
   delay(1000);
   lcd.clear();
   lcd.setCursor(0,0);
   lcd.print("waitting for");
   lcd.setCursor(0,1);
   lcd.print("connect...");

    // Attempt to connect to Wifi network:
    while (status != WL_CONNECTED) {
        Serial.print("Attempting to connect to WPA SSID: ");
        Serial.println(ssid);
        status = WiFi.begin(ssid, pass);

        // wait 2 seconds for connection:
        delay(2000);
    }

    ip = WiFi.localIP();

    // Configure audio peripheral for audio data output
    audio.configAudio(configA);
    audio.begin();

    // Configure audio encoder
    // For G711 audio encoder, choose between u-law and a-law algorithm
    encoder.configAudio(configA);
//    encoder.configCodec(CODEC_G711_PCMU);
//    encoder.configCodec(CODEC_G711_PCMA);
    encoder.begin();

    // Configure RTSP with identical audio format information and codec
    rtsp.configAudio(configA, CODEC_AAC);
//    rtsp.configAudio(configA, CODEC_G711_PCMU);
//    rtsp.configAudio(configA, CODEC_G711_PCMA);
    audioStreamer1.registerInput(audio);
    audioStreamer1.registerOutput1(encoder);
    audioStreamer1.begin();

    audioStreamer2.registerInput(encoder);
    audioStreamer2.registerOutput(rtsp);
    audioStreamer2.begin();

    // Configure camera video channels with video format information
    // Adjust the bitrate based on your WiFi network quality
    config.setBitrate(2 * 1024 * 1024);     // Recommend to use 2Mbps for RTSP streaming to prevent network congestion
    Camera.configVideoChannel(CHANNEL, config);
    Camera.configVideoChannel(CHANNELNN, configNN);
    Camera.videoInit();

    // Configure RTSP with corresponding video format information
    rtsp.configVideo(config);
    rtsp.begin();
    rtsp_portnum = rtsp.getPort();

    // Configure Face Recognition model
    // Select Neural Network(NN) task and models
    facerecog.configVideo(configNN);
    facerecog.modelSelect(FACE_RECOGNITION, NA_MODEL, DEFAULT_SCRFD, DEFAULT_MOBILEFACENET);
    facerecog.begin();
    facerecog.setResultCallback(FRPostProcess);

    // Configure StreamIO object to stream data from video channel to RTSP
    videoStreamer.registerInput(Camera.getStream(CHANNEL));
    videoStreamer.registerOutput(rtsp);
    if (videoStreamer.begin() != 0) {
        Serial.println("StreamIO link start failed");
    }
    // Start data stream from video channel
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

    printInfo_LCD();
    // Start OSD drawing on RTSP video channel
    OSD.configVideo(CHANNEL, config);
    OSD.begin();
}

void loop() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  // 讀取超音波返回的脈衝時間
  long duration = pulseIn(echoPin, HIGH);

  // 將脈衝時間轉換為距離（公分）
  int D = duration * 0.034 /2;
   if(D<5){
   digitalWrite(B,0);
   digitalWrite(R,0);
   digitalWrite(G,1);
   
   
   
    if (Serial.available() > 0) {
        String input = Serial.readString();
        input.trim();

        if (input.startsWith(String("REG="))){
            String name = input.substring(4);
            facerecog.registerFace(name);
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print("Hello");
            lcd.setCursor(0,1);
            lcd.print(name);
        } else if (input.startsWith(String("DEL="))) {
            String name = input.substring(4);
            facerecog.removeFace(name);
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print("BEY BEY");
            lcd.setCursor(0,1);
            lcd.print(name);
        } else if (input.startsWith(String("RESET"))) {
            facerecog.resetRegisteredFace();
        } else if (input.startsWith(String("BACKUP"))) {
            facerecog.backupRegisteredFace();
        } else if (input.startsWith(String("RESTORE"))) {
            facerecog.restoreRegisteredFace();
        }
     OSD.createBitmap(CHANNEL);
     OSD.update(CHANNEL);
    }
    }
     else if(D>5 && D<20){
    digitalWrite(R,0);
    digitalWrite(G,0);
    //LCD訊息:請靠近一點
   lcd.clear();
   lcd.setCursor(2,0);
   lcd.print("Come Closer");
   digitalWrite(B,1);
   delay(200);
   digitalWrite(B,0);
   delay(200);
   digitalWrite(B,1);
   delay(200);
   digitalWrite(B,0);
   delay(1000);
  }
  else if(D>20){
    digitalWrite(R,1);
    digitalWrite(G,0);
    digitalWrite(B,0);
  }



    delay(1000);
    
}

// User callback function for post processing of face recognition results
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
        for (uint32_t i = 0; i < facerecog.getResultCount(); i++) {
            FaceRecognitionResult item = results[i];
            // Result coordinates are floats ranging from 0.00 to 1.00
            // Multiply with RTSP resolution to get coordinates in pixels
            int xmin = (int)(item.xMin() * im_w);
            int xmax = (int)(item.xMax() * im_w);
            int ymin = (int)(item.yMin() * im_h);
            int ymax = (int)(item.yMax() * im_h);

            uint32_t osd_color;
            if (String(item.name()) == String("unknown")) {
                osd_color = OSD_COLOR_RED;
            } else {
                osd_color = OSD_COLOR_GREEN;
            }

            // Draw boundary box
            printf("Face %d name %s:\t%d %d %d %d\n\r", i, item.name(), xmin, xmax, ymin, ymax);
            OSD.drawRect(CHANNEL, xmin, ymin, xmax, ymax, 3, osd_color);

            // Print identification text above boundary box
            char text_str[40];
            snprintf(text_str, sizeof(text_str), "Face:%s", item.name());
            OSD.drawText(CHANNEL, xmin, ymin - OSD.getTextHeight(CHANNEL), text_str, osd_color);
        }
    }
    OSD.update(CHANNEL);
}
void printInfo_LCD(void){
   ip = WiFi.localIP();
   lcd.clear();
   lcd.setCursor(4,0);
   lcd.print("complete");
   delay(1000);
   lcd.clear();
   lcd.setCursor(0,0);
   lcd.print("rstp://");
   lcd.setCursor(0,1);
   lcd.print(ip);
   delay(1000);

}
