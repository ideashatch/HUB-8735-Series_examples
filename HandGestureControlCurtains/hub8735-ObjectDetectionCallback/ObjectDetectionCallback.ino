#include "WiFi.h"
#include "StreamIO.h"
#include "VideoStream.h"
#include "RTSP.h"
#include "NNObjectDetection.h"
#include "VideoStreamOverlay.h"
#include "ObjectClassList.h"
#include "scissors.h"
#include "paper.h"
#include "rock.h"
#include <AccelStepper.h>

#define CHANNEL   0
#define CHANNELNN 3

// Lower resolution for NN processing
#define NNWIDTH  576
#define NNHEIGHT 320

VideoSetting config(VIDEO_FHD, 30, VIDEO_H264, 0);
VideoSetting configNN(NNWIDTH, NNHEIGHT, 10, VIDEO_RGB, 0);
NNObjectDetection ObjDet;
RTSP rtsp;
StreamIO videoStreamer(1, 1);
StreamIO videoStreamerNN(1, 1);

char ssid[] = "Max";    // your network SSID (name)
char pass[] = "maxlin910920";        // your network password
int status = WL_IDLE_STATUS;

IPAddress ip;
int rtsp_portnum;

#define DIR_PIN 8   // DIR 接口
#define PUL_PIN 9   // PUL 接口
#define ENA_PIN 10  // EMA (ENA) 接口
// 模拟的状态变量
bool detected_paper = false;
bool detected_scissors = false;
bool detected_rock = false;




void setup()
{
    Serial.begin(115200);

    // Setup motor control pins
    // 初始化步进电机
    pinMode(DIR_PIN, OUTPUT);
    pinMode(PUL_PIN, OUTPUT);
    pinMode(ENA_PIN, OUTPUT);

    digitalWrite(ENA_PIN, HIGH); // 使能步进电机
    digitalWrite(DIR_PIN, HIGH); // 初始方向设置为正向


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
    config.setBitrate(2 * 1024 * 1024);    // Recommend to use 2Mbps for RTSP streaming to prevent network congestion
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
}

void loop(){
// 假设有函数或传感器更新这些变量


  // 根据检测到的状态控制步进电机
  if (detected_paper) {
    digitalWrite(DIR_PIN, HIGH);  // 设置正向
    moveMotor();
  } else if (detected_scissors) {
    // 停止
    // 不需要执行任何动作，保持当前状态
  } else if (detected_rock) {
    digitalWrite(DIR_PIN, LOW);  // 设置反向
    moveMotor();
  }
}

void moveMotor() {
  // 发送脉冲信号来控制步进电机的运动
  for (int i = 0; i < 200; i++) { // 例如，发出 200 个脉冲完成一个完整的步进
    digitalWrite(PUL_PIN, HIGH);
    delayMicroseconds(500); // 控制脉冲间隔时间，影响步进电机的速度
    digitalWrite(PUL_PIN, LOW);
    delayMicroseconds(500);
  }
}


// User callback function for post processing of object detection results
void ODPostProcess(std::vector<ObjectDetectionResult> results)
{
    uint16_t im_h = config.height();
    uint16_t im_w = config.width();

    Serial.print("Network URL for RTSP Streaming: ");
    Serial.print("rtsp://");
    Serial.print(ip);
    Serial.print(":");
    Serial.println(rtsp_portnum);
    Serial.println(" ");

    printf("Total number of objects detected = %d\r\n", ObjDet.getResultCount());
    OSD.createBitmap(CHANNEL);

    bool detected_paper = false;
    bool detected_scissors = false;
    bool detected_rock = false;

    if (ObjDet.getResultCount() > 0) {
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
                printf("Item %d %s:\t%d %d %d %d\n\r", i, itemList[obj_type].objectName, xmin, xmax, ymin, ymax);
                OSD.drawRect(CHANNEL, xmin, ymin, xmax, ymax, 3, OSD_COLOR_WHITE);

                // Print identification text
                char text_str[20];
                snprintf(text_str, sizeof(text_str), "%s %d", itemList[obj_type].objectName, item.score());
                OSD.drawText(CHANNEL, xmin, ymin - OSD.getTextHeight(CHANNEL), text_str, OSD_COLOR_CYAN);

                // Set detection flags based on object type
                if (strcmp(itemList[obj_type].objectName, "paper") == 0) {
                    detected_paper = true;
                } else if (strcmp(itemList[obj_type].objectName, "scissors") == 0) {
                    detected_scissors = true;
                } else if (strcmp(itemList[obj_type].objectName, "rock") == 0) {
                    detected_rock = true;
                }
            }
        }
    }
    OSD.update(CHANNEL);

    // Control motor based on detection results

}
