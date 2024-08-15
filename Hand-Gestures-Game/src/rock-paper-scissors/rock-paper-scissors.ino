/*

 Example guide:
 https://www.amebaiot.com/en/amebapro2-amb82-mini-arduino-neuralnework-object-detection/

 For recommended setting to achieve better video quality, please refer to our Ameba FAQ: https://forum.amebaiot.com/t/ameba-faq/1220

 NN Model Selection
 Select Neural Network(NN) task and models using .modelSelect(nntask, objdetmodel, facedetmodel, facerecogmodel).
 Replace with NA_MODEL if they are not necessary for your selected NN Task.

 NN task
 =======
 OBJECT_DETECTION/ FACE_DETECTION/ FACE_RECOGNITION

 Models
 =======
 YOLOv3 model         DEFAULT_YOLOV3TINY   / CUSTOMIZED_YOLOV3TINY
 YOLOv4 model         DEFAULT_YOLOV4TINY   / CUSTOMIZED_YOLOV4TINY
 YOLOv7 model         DEFAULT_YOLOV7TINY   / CUSTOMIZED_YOLOV7TINY
 SCRFD model          DEFAULT_SCRFD        / CUSTOMIZED_SCRFD
 MobileFaceNet model  DEFAULT_MOBILEFACENET/ CUSTOMIZED_MOBILEFACENET
 No model             NA_MODEL
 */

#include "WiFi.h"
#include "StreamIO.h"
#include "VideoStream.h"
#include "RTSP.h"
#include "NNObjectDetection.h"
#include "VideoStreamOverlay.h"
#include "ObjectClassList.h"
#include "SPI.h"
#include "AmebaILI9341.h"
#include "scissors.h"
#include "paper.h"
#include "rock.h"

// For all supportted boards (AMB21/AMB22, AMB23, BW16/BW16-TypeC, AW-CU488_ThingPlus), 
// Select 2 GPIO pins connect to TFT_RESET and TFT_DC. And default SPI_SS/SPI1_SS connect to TFT_CS.
/*
#define TFT_RESET       5
#define TFT_DC          4
*/
#define TFT_RESET       9
#define TFT_DC          10
#define TFT_CS          SPI1_SS

AmebaILI9341 tft = AmebaILI9341(TFT_CS, TFT_DC, TFT_RESET);

#define ILI9341_SPI_FREQUENCY 20000000


#define CHANNEL 0
#define CHANNELNN 3

// Lower resolution for NN processing
#define NNWIDTH 576
#define NNHEIGHT 320

// OSD layers
#define RECTLAYER OSDLAYER0
#define TEXTLAYER OSDLAYER1

VideoSetting config(VIDEO_FHD, 30, VIDEO_H264, 0);
VideoSetting configNN(NNWIDTH, NNHEIGHT, 10, VIDEO_RGB, 0);
NNObjectDetection ObjDet;
RTSP rtsp;
StreamIO videoStreamer(1, 1);
StreamIO videoStreamerNN(1, 1);

char ssid[] = "YOUR_WIFI_SSID";    // your network SSID (name)
char pass[] = "YOUR_WIFI_PASSWORD";       // your network password
int status = WL_IDLE_STATUS;

IPAddress ip;
int rtsp_portnum; 

int checkNum = -1;
uint8_t oldNum,CktCount,Confirm;

uint8_t isClean,isPrint;
int myNum;

void setup() {
    Serial.begin(115200);
    Serial1.begin(115200);
    Serial1.println("Serial1 Test!");

    /* TFT */
    SPI.setDefaultFrequency(ILI9341_SPI_FREQUENCY);
    tft.begin();


    tft.clr();
    tft.setRotation(3);
    
    tft.setForeground(ILI9341_ORANGE);
    tft.setFontSize(3);
  
    tft.setCursor(10, 5);
    tft.print("GAME : rock-paper-scissors");
   tft.drawRectangle(0,0,319,239,ILI9341_WHITE);
   
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
    Camera.videoInit();

    // Configure RTSP with corresponding video format information
    rtsp.configVideo(config);
    rtsp.begin();
    rtsp_portnum = rtsp.getPort();

    // Configure object detection with corresponding video format information
    // Select Neural Network(NN) task and models
    ObjDet.configVideo(configNN);
    ObjDet.setResultCallback(ODPostProcess);
    ObjDet.modelSelect(OBJECT_DETECTION, DEFAULT_YOLOV4TINY, NA_MODEL, NA_MODEL);
    ObjDet.begin();
    
    // Configure StreamIO object to stream data from video channel to RTSP
    videoStreamer.registerInput(Camera.getStream(CHANNEL));
    videoStreamer.registerOutput(rtsp);
    if (videoStreamer.begin() != 0) {
        Serial.println("StreamIO link start failed");
    }

    // Start data stream from video channel
    Camera.channelBegin(CHANNEL);

    // Configure StreamIO object to stream data from RGB video channel to object detection
    videoStreamerNN.registerInput(Camera.getStream(CHANNELNN));
    videoStreamerNN.setStackSize();
    videoStreamerNN.setTaskPriority();
    videoStreamerNN.registerOutput(ObjDet);
    if (videoStreamerNN.begin() != 0) {
        Serial.println("StreamIO link start failed");
    }

    // Start video channel for NN
    Camera.channelBegin(CHANNELNN);

    // Start OSD drawing on RTSP video channel
    OSD.configVideo(CHANNEL, config);
    OSD.begin();
    delay(1500);

    tft.setCursor(10, 5);
   
    tft.print("                           ");
    delay(1500);
    tft.setCursor(160, 100);
    tft.setForeground(ILI9341_RED);
    tft.print("YOU AREA");
    delay(1500);
    tft.setCursor(160, 100);
    tft.print("        ");

    tft.setCursor(20, 50);
    tft.setForeground(ILI9341_RED);
    tft.print("GO!");  
    delay(1500);  
    tft.setCursor(20, 50);
    tft.print("   ");  
}

void loop() {
    // Do nothing

    if(Confirm && checkNum > -1)
    {
      if(!isPrint)
      {
        isPrint = 1;

        tft.setCursor(100, 10);
        tft.print("        ");

        tft.fillRectangle(15,85,150,150,ILI9341_BLACK);
        tft.fillRectangle(160,85,150,150,ILI9341_BLACK);
        if(checkNum == 0)
        {
          
          tft.drawBitmap(160,85,150,150,scissors);
         
        }   
        else if (checkNum == 1)
        {
          
          tft.drawBitmap(160,85,150,150,rock);
          
        }
        else if (checkNum == 2)
        {
          
          tft.drawBitmap(160,85,150,150,paper);
          
        }

        if(myNum == 0)
        {
          tft.drawBitmap(15,85,150,150,scissors);
          tft.setCursor(100, 10);
          if(checkNum == 1)
          {
            tft.setForeground(ILI9341_GREEN);
            tft.print("YOU WIN");
          }
          else if(checkNum != 0)
          {
            tft.setForeground(ILI9341_RED);
            tft.print("YOU LOSS");
          }
        }
        else if(myNum == 1)
        {
          tft.drawBitmap(15,85,150,150,rock);
          tft.setCursor(100, 10);
          if(checkNum == 2)
          {
            tft.setForeground(ILI9341_GREEN);
            tft.print("YOU WIN");
          }
          else if(checkNum != 1)
          {
            tft.setForeground(ILI9341_RED);
            tft.print("YOU LOSS");
          }
        }
        else if(myNum == 2)
        {
          tft.drawBitmap(15,85,150,150,paper);
          tft.setCursor(100, 10);
          if(checkNum == 0)
          {
            tft.setForeground(ILI9341_GREEN);
            tft.print("YOU WIN");
          }
          else if(checkNum != 2)
          {
            tft.setForeground(ILI9341_RED);
            tft.print("YOU LOSS");
          }          
        }
            
      }
    }
    else 
    {
      isPrint = 0;
    }
}

// User callback function for post processing of object detection results
void ODPostProcess(std::vector<ObjectDetectionResult> results) {
    uint16_t im_h = config.height();
    uint16_t im_w = config.width();

    Serial.print("Network URL for RTSP Streaming: ");
    Serial.print("rtsp://");
    Serial.print(ip);
    Serial.print(":");
    Serial.println(rtsp_portnum);
    Serial.println(" ");

    printf("Total number of objects detected = %d\r\n", ObjDet.getResultCount());

    OSD.createBitmap(CHANNEL, RECTLAYER);
    OSD.createBitmap(CHANNEL, TEXTLAYER);
    if (ObjDet.getResultCount() > 0) {
        for (uint16_t i = 0; i < ObjDet.getResultCount(); i++) {
            int obj_type = results[i].type();
            if (itemList[obj_type].filter) {    // check if item should be ignored
                ObjectDetectionResult item = results[i];
                // Result coordinates are floats ranging from 0.00 to 1.00
                // Multiply with RTSP resolution to get coordinates in pixels
                int xmin = (int)(item.xMin() * im_w);
                int xmax = (int)(item.xMax() * im_w);
                int ymin = (int)(item.yMin() * im_h);
                int ymax = (int)(item.yMax() * im_h);

                // Draw boundary box
                printf("Item %d %s:\t%d %d %d %d\n\r", i, itemList[obj_type].objectName, xmin, xmax, ymin, ymax);
                OSD.drawRect(CHANNEL, xmin, ymin, xmax, ymax, 3, OSD_COLOR_WHITE, RECTLAYER);

                // Print identification text
                char text_str[20];
                snprintf(text_str, sizeof(text_str), "%s %d", itemList[obj_type].objectName, item.score());
                OSD.drawText(CHANNEL, xmin, ymin - OSD.getTextHeight(CHANNEL), text_str, OSD_COLOR_CYAN, TEXTLAYER);
            }

            if(ObjDet.getResultCount() == 1 && isPrint == 0)
            {
              oldNum = itemList[obj_type].index;
            }
        }

        if(ObjDet.getResultCount() == 1 && isPrint == 0)
        {
          myNum = random(3);

          if(checkNum != oldNum)
          {
            checkNum = oldNum;
          }
          else
          {
            if(checkNum == oldNum)
            { 
              CktCount++;
            }
            else
            {
              CktCount = 0;
            }

            if (CktCount > 1)
            {
              Confirm = 1;
            }
            
          }
        }
    }
    else
    {
      checkNum = -1;
      CktCount = 0;
      Confirm = 0;
    }
    OSD.update(CHANNEL, RECTLAYER);
    OSD.update(CHANNEL, TEXTLAYER);
}
