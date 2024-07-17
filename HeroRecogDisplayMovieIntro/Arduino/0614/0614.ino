#include "WiFi.h"
#include "StreamIO.h"
#include "VideoStream.h"
#include "RTSP.h"
#include "NNObjectDetection.h"
#include "VideoStreamOverlay.h"
#include "ObjectClassList.h"
#include "WiFiClient.h"

char ssid[] = "network SSID";    // your network SSID (name)
char password[] = " network password";   // your network password
WiFiClient client;

#define CHANNEL   0
#define CHANNELNN 3
#define NNWIDTH  576
#define NNHEIGHT 320

VideoSetting config(VIDEO_FHD, 30, VIDEO_H264, 0);
VideoSetting configNN(NNWIDTH, NNHEIGHT, 10, VIDEO_RGB, 0);
NNObjectDetection ObjDet;
RTSP rtsp;
StreamIO videoStreamer(1, 1);
StreamIO videoStreamerNN(1, 1);

IPAddress ip;
int rtsp_portnum;

bool ironManRequestSent = false;
bool caRequestSent = false;
unsigned long lastRequestTime = 0;
const unsigned long requestInterval = 10000; // 10 seconds

void setup()
{
    Serial.begin(115200);

    // Attempt to connect to WiFi network:
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print("Attempting to connect to WPA SSID: ");
        Serial.println(ssid);
        WiFi.begin(ssid, password);
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

void loop()
{
    if ((ironManRequestSent || caRequestSent) && (millis() - lastRequestTime >= requestInterval)) {
        ironManRequestSent = false; // Reset flag after 10 seconds
        caRequestSent = false; // Reset flag after 10 seconds
    }
    // Do nothing else in loop
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

                // Open browser and navigate to specific webpage when "Iron man" or "CA" object is detected
                if (strcmp(itemList[obj_type].objectName, "Iron man") == 0 && !ironManRequestSent) {
                    if (!client.connect("172.20.10.2", 8888)) {
                        Serial.println("Connection to Python server failed");
                        return;
                    }
                    // Make a HTTP request to Python server to open browser:
                    client.println("GET /open_browser1 HTTP/1.1");
                    client.print("Host: 172.20.10.2\r\n");
                    client.println("Connection: close\r\n");
                    client.println();

                    ironManRequestSent = true; // Set flag after sending HTTP request
                    lastRequestTime = millis(); // Record the time of the request
                }

                if (strcmp(itemList[obj_type].objectName, "CA") == 0 && !caRequestSent) {
                    if (!client.connect("172.20.10.2", 8888)) {
                        Serial.println("Connection to Python server failed");
                        return;
                    }
                    // Make a HTTP request to Python server to open browser:
                    client.println("GET /open_browser2 HTTP/1.1");
                    client.print("Host: 172.20.10.2\r\n");
                    client.println("Connection: close\r\n");
                    client.println();

                    caRequestSent = true; // Set flag after sending HTTP request
                    lastRequestTime = millis(); // Record the time of the request
                }
            }
        }
    }
    OSD.update(CHANNEL);
}
