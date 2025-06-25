/*

 Example guide:
 https://www.amebaiot.com/en/amebapro2-arduino-neuralnework-object-detection/

 NN Model Selection
 Select Neural Network(NN) task and models using modelSelect(nntask, objdetmodel, facedetmodel, facerecogmodel).
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

#include <Wire.h>
#include <I2C_LCD_libraries/LiquidCrystal_I2C.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "video_drv.h"
#include "isp_ctrl_api.h"

#include <httpd/httpd.h>
#ifdef __cplusplus
}
#endif
#define CHANNEL   0
#define CHANNELNN 3

// Lower resolution for NN processing
#define NNWIDTH  576
#define NNHEIGHT 320

#define DICE_NUM  3
#define check_time  10

#define EN_RTSP   1

VideoSetting config(VIDEO_FHD, 30, VIDEO_H264, 0);
VideoSetting configNN(NNWIDTH, NNHEIGHT, 10, VIDEO_RGB, 0);
NNObjectDetection ObjDet;
RTSP rtsp;
StreamIO videoStreamer(1, 1);
StreamIO videoStreamerNN(1, 1);

char ssid[] = "network_SSID";         // your network SSID (name)
char pass[] = "password";        // your network password
int status = WL_IDLE_STATUS;

IPAddress ip;
int rtsp_portnum;
int button = PUSH_BTN;
bool bisPress = false;
bool breleaseprint = false;
uint32_t count = 0;
bool bDetectDice = false;
bool bShowDiceOSDOnce = false;
char osd_dice_str[20];
char osd_dice_str2[20];

// LCM1602 I2C LCD
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);    // Set the LCD I2C address

struct DiceItem {
    uint8_t index;
    uint8_t count;
};

void print_dice_rolling()
{
    lcd.clear();
    lcd.setCursor(0, 0);    // go to home
    lcd.print("GAME Start");
    
    lcd.setCursor(0, 1);    // go to the next line
    lcd.print("Rolling... ");
}

void print_dice_result(uint8_t n1, uint8_t n2, uint8_t n3)
{
    lcd.clear();
    lcd.setCursor(0, 0);    // go to home
    lcd.print("The dice: ");
    lcd.print(itemList[n1].objectName);
    lcd.print(",");
    lcd.print(itemList[n2].objectName);
    lcd.print(",");
    lcd.print(itemList[n3].objectName);
    
    lcd.setCursor(0, 1);    // go to the next line
    lcd.print("Total: ");
    lcd.print(itemList[n1].index + itemList[n2].index + itemList[n3].index + 3);
    lcd.print(".");

    snprintf(osd_dice_str, sizeof(osd_dice_str), "The dice: %s %s %s ", itemList[n1].objectName,itemList[n2].objectName,itemList[n3].objectName);
    snprintf(osd_dice_str2, sizeof(osd_dice_str2), "Total: %d.", itemList[n1].index + itemList[n2].index + itemList[n3].index + 3);
    bShowDiceOSDOnce = true;
//    OSD.drawText(CHANNEL, xmin, ymin - OSD.getTextHeight(CHANNEL), text_str, OSD_COLOR_CYAN);
}

void setup()
{
    Serial.begin(115200);

    pinMode(button, INPUT_PULLUP);

    lcd.begin(16, 2, LCD_5x8DOTS, Wire);    // initialize the lcd
    // lcd.begin(16, 2, LCD_5x8DOTS, Wire1);               // Uncomment this line to use Wire1

    lcd.backlight();

    lcd.setCursor(0, 0);    // go to home
    lcd.print("** DICE  GAME **");
    lcd.setCursor(0, 1);    // go to the next line
    lcd.print("Press BTN Start");

    // attempt to connect to Wifi network:
    while (status != WL_CONNECTED) {
        Serial.print("Attempting to connect to WPA SSID: ");
        Serial.println(ssid);
        status = WiFi.begin(ssid, pass);

        // wait 2 seconds for connection:
        delay(2000);
    }
    ip = WiFi.localIP();

#if EN_RTSP
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
    ObjDet.modelSelect(OBJECT_DETECTION, DEFAULT_YOLOV4TINY, NA_MODEL, NA_MODEL);
    //ObjDet.modelSelect(OBJECT_DETECTION, DEFAULT_YOLOV7TINY, NA_MODEL, NA_MODEL);
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
#endif

}

DiceItem Dice[DICE_NUM] = {
    {0xff, 0},
    {0xff, 0},
    {0xff, 0}
};

void cleardice()
{
    printf("cleardice\r\n");
    for (int i = 0; i < DICE_NUM; i++)
    {
      Dice[i].index = 0xff;
      Dice[i].count = 0;
    }
}

uint32_t timeout = 300;
uint8_t loop_timeout = 30;

bool checkdicenumber(uint8_t index, uint8_t cur_index)
{
    if (Dice[index].index = 0xff)
    {
        Dice[index].index = cur_index;
        Dice[index].count++;
    }
    else if (Dice[index].index == cur_index)
    {
        Dice[index].count++;
    }
    else  //Dice[i].index !=
    {
          cleardice();
    }       

    printf("************\r\n");
    printf("Dice[0] = %d, %d\r\n", Dice[0].index, Dice[0].count);
    printf("Dice[1] = %d, %d\r\n", Dice[1].index, Dice[1].count);
    printf("Dice[2] = %d, %d\r\n", Dice[2].index, Dice[2].count);
    printf("************\r\n");

    if ((Dice[0].count >= check_time) && (Dice[1].count >= check_time) && (Dice[2].count >= check_time))
    {
      print_dice_result(Dice[0].index, Dice[1].index, Dice[2].index);
      cleardice();
      return 1;
    }

    timeout--;

    if (timeout == 0)
    {
       printf("dice check timeout\r\n");
       timeout = 300;
       return 1;
    }

    return 0;
}

void loop()
{
    std::vector<ObjectDetectionResult> results = ObjDet.getResult();

    uint16_t im_h = config.height();
    uint16_t im_w = config.width();

    if ((!bDetectDice) || (DICE_NUM == 0xFF))
    {
        OSD.createBitmap(CHANNEL);
        if (bShowDiceOSDOnce)
        {
            OSD.drawText(CHANNEL, 10, 10, osd_dice_str, OSD_COLOR_BLACK);
            OSD.drawText(CHANNEL, 10, 40, osd_dice_str2, OSD_COLOR_BLACK);            
        }
        OSD.update(CHANNEL);

        bDetectDice = true;
    }
    else
    {
      OSD.createBitmap(CHANNEL);
      OSD.update(CHANNEL);
      bShowDiceOSDOnce = false;
    }

    if(bDetectDice)
    {
        printf("Total number of objects detected = %d\r\n", ObjDet.getResultCount());
        
        if ((ObjDet.getResultCount() == DICE_NUM) || (DICE_NUM == 0xFF))
        {
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

                        if (DICE_NUM != 0xFF)  //
                        {
                            if(checkdicenumber(i, obj_type))
                            {
                              bDetectDice = false;
                            }
                        }
                    }
                }
            }
            OSD.update(CHANNEL);
        }
        else
        {
            print_dice_rolling();
            loop_timeout--;
            if (loop_timeout == 0)
            {
              loop_timeout = 30;
              bDetectDice = 0;
            }
        }
        // delay to wait for new results
        //delay(100);
    }
    // delay to wait for new results
    delay(100);
}
