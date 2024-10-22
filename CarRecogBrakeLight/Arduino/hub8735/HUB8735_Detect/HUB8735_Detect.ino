/*

 Example guide:
 https://www.amebaiot.com/en/amebapro2-amb82-mini-arduino-neuralnework-object-detection/

 For recommended setting to achieve better video quality, please refer to our Ameba FAQ: https://forum.amebaiot.com/t/ameba-faq/1220
 */

#include "WiFi.h"
#include "StreamIO.h"
#include "VideoStream.h"
#include "RTSP.h"
#include "NNObjectDetection.h"
#include "VideoStreamOverlay.h"
#include "ObjectClassList.h"

#define CHANNEL 0
#define CHANNELNN 3 

// Lower resolution for NN processing
#define NNWIDTH 576
#define NNHEIGHT 320

// User defined yolo model
// 1: yolov3tiny
// 2: yolov4tiny
// 3: yolov7tiny
#define YOLOMODEL 2

VideoSetting config(VIDEO_FHD, 30, VIDEO_H264, 0);
VideoSetting configNN(NNWIDTH, NNHEIGHT, 10, VIDEO_RGB, 0);
NNObjectDetection ObjDet;
RTSP rtsp;
StreamIO videoStreamer(1, 1);
StreamIO videoStreamerNN(1, 1);

char ssid[] = "hel";              // your network SSID (name)
char pass[] = "1234567890";       // your network password
int status = WL_IDLE_STATUS;

IPAddress ip;
int rtsp_portnum;
    
void setup() {
    Serial.begin(115200);
    Serial1.begin(115200);
    Serial3.begin(115200);
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
    //ObjDet.configVideo(configNN);
    //ObjDet.begin(YOLOMODEL);
    ObjDet.configVideo(configNN);
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
}

void loop() {
    std::vector<ObjectDetectionResult> results = ObjDet.getResult();

    uint16_t im_h = config.height();
    uint16_t im_w = config.width();

    /*Serial.print("Network URL for RTSP Streaming: ");
    Serial.print("rtsp://");
    Serial.print(ip);
    Serial.print(":");
    Serial.println(rtsp_portnum);
    Serial.println(" ");*/
    
    //printf("Total number of objects detected = %d\r\n", results.size());
    //OSD.clearAll(CHANNEL);
    //printf("Total number of objects detected = %d\r\n", ObjDet.getResultCount());
    OSD.createBitmap(CHANNEL);
    
    //if (results.size() > 0) {
    if (ObjDet.getResultCount() > 0) {
        //for (uint32_t i = 0; i < results.size(); i++) {
        for (int i = 0; i < ObjDet.getResultCount(); i++) {
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
                //printf("Item %d %s:\t%d %d %d %d\n\r", i, itemList[obj_type].objectName, xmin, xmax, ymin, ymax);
                //Serial.println(itemList[obj_type].objectName);
                if(itemList[obj_type].objectName == "person"){
                  String Direction;
                  if(xmin < 400)   Direction = "R";
                  else if(xmin > 400 && xmin < 700)    Direction = "M";
                  else if(xmin > 700)   Direction = "L";
                  Serial3.print("1," + Direction);
                  Serial.println("1," + Direction);
                  Serial.println("辨識物件為機車");
                  Serial.print("11");
                }
                else if(itemList[obj_type].objectName == "bus"){
                  String Direction;
                  if(xmin < 400)   Direction = "R";
                  else if(xmin > 400 && xmin < 700)    Direction = "M";
                  else if(xmin > 700)   Direction = "L";
                  Serial3.print("2," + Direction);
                  Serial.println("2," + Direction);
                  Serial.println("辨識物件為公車");
                  Serial.print("22");
                }
                else if(itemList[obj_type].objectName == "truck"){
                  String Direction;
                  if(xmin < 400)   Direction = "R";
                  else if(xmin > 400 && xmin < 700)    Direction = "M";
                  else if(xmin > 700)   Direction = "L";
                  Serial3.print("3," + Direction);
                  Serial.println("3," + Direction);
                  Serial.println("辨識物件為卡車");
                  Serial.print("33");
                }
                else if(itemList[obj_type].objectName == "car"){
                  String Direction;
                  if(xmin < 400)   Direction = "R";
                  else if(xmin > 400 && xmin < 700)    Direction = "M";
                  else if(xmin > 700)   Direction = "L";
                  Serial3.print("4," + Direction);
                  Serial.println("4," + Direction);
                  Serial.println("辨識物件為汽車");
                  Serial1.print("44");
                }
                OSD.drawRect(CHANNEL, xmin, ymin, xmax, ymax, 3, OSD_COLOR_WHITE);

                // Print identification text
                char text_str[20];
                //snprintf(text_str, sizeof(text_str), "%s %d", itemList[obj_type].objectName, item.score());
                OSD.drawText(CHANNEL, xmin, ymin - OSD.getTextHeight(CHANNEL), text_str, OSD_COLOR_CYAN);
            }
        }
    }
    OSD.update(CHANNEL);
    // delay to wait for new results
    delay(100);
}
