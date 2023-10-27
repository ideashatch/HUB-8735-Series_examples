// Point the camera at a target face and enter the following commands into the serial monitor,
// Register face:           "REG={Name}"            Ensure that there is only one face detected in frame
// Exit registration mode:  "EXIT"                  Stop trying to register a face before it is successfully registered
// Reset registered faces:  "RESET"                 Forget all previously registered faces
// Backup registered faces to flash:    "BACKUP"    Save registered faces to flash
// Restore registered faces from flash: "RESTORE"   Load registered faces from flash

#include "WiFi.h"
#include "StreamIO.h"
#include "VideoStream.h"
#include "RTSP.h"
#include "NNFaceDetectionRecognition.h"
#include "VideoStreamOverlay.h"
#include <AmebaServo.h>
#include "AmebaFatFS.h"
#include "SSLLineNotify.h"

#define CHANNELVID  0 // Channel for RTSP streaming
#define CHANNELJPEG 1 // Channel for taking snapshots
#define CHANNELNN   3 // RGB format video for NN only avaliable on channel 3

// Customised resolution for NN
#define NNWIDTH  576
#define NNHEIGHT 320

// OSD layers
#define RECTTEXTLAYER0 OSDLAYER0
#define RECTTEXTLAYER1 OSDLAYER1
#define DOORSTATUSLAYER2 OSDLAYER2

// Pin Definition
#define RED_LED    3
#define GREEN_LED  4
#define BUTTON_PIN 17
#define SERVO_PIN  8

VideoSetting configVID(VIDEO_FHD, 20, VIDEO_H264, 0);
VideoSetting configNN(NNWIDTH, NNHEIGHT, 10, VIDEO_RGB, 0);
NNFaceDetectionRecognition facerecog;
RTSP rtsp;
StreamIO videoStreamer(1, 1);
StreamIO videoStreamerFDFR(1, 1);
StreamIO videoStreamerRGBFD(1, 1);
AmebaServo myservo;

char ssid[] = "IDS-Cloud-2F";    // your network SSID (name)
char pass[] = "026607200088";       // your network password
int status = WL_IDLE_STATUS;

/* AP mode
char ap_ssid[] = "apmode";
char ap_pass[] = "00000000";
char ap_channel[] = "1"; 
int ap_ssid_status = 0;  
*/

bool doorOpen = false;
int buttonCount = 0;        //press count

//status flag
bool systemOn = false;
bool controlState = false;

long button_timeout = 0;
int buttonPressState = 0;  //0: unknow 1: short 2: double

// File Initialization
//AmebaFatFS fs;

void button_handler(uint32_t id, uint32_t event) {
  buttonCount++;
  printf("button_handler\r\n");
}

char server[] = "notify-api.line.me";    // name address for LINE notify (using DNS)
LineNotify client;

void setup() {
    // GPIO Initialisation
    pinMode(RED_LED, OUTPUT);
    pinMode(GREEN_LED, OUTPUT);

    myservo.attach(SERVO_PIN);
  
    Serial.begin(115200);

    // attempt to connect to Wifi network:
    while (status != WL_CONNECTED) {
        Serial.print("Attempting to connect to WPA SSID: ");
        Serial.println(ssid);
        status = WiFi.begin(ssid, pass);

        // wait 2 seconds for connection:
        delay(2000);
    }

    // Configure camera video channels with video format information
    Camera.configVideoChannel(CHANNELVID, configVID);
    Camera.configVideoChannel(CHANNELNN, configNN);
    Camera.videoInit();

    // Configure RTSP with corresponding video format information
    rtsp.configVideo(configVID);
    rtsp.begin();
  
    // Configure Face Recognition model
    facerecog.configVideo(configNN);
    facerecog.modelSelect(FACE_RECOGNITION, NA_MODEL, DEFAULT_SCRFD, DEFAULT_MOBILEFACENET);
    facerecog.begin();
    facerecog.setResultCallback(FRPostProcess);

    // Configure StreamIO object to stream data from video channel to RTSP
    videoStreamer.registerInput(Camera.getStream(CHANNELVID));
    videoStreamer.registerOutput(rtsp);
    if (videoStreamer.begin() != 0) {
        Serial.println("StreamIO link start failed");
    }
    // Start data stream from video channel
    Camera.channelBegin(CHANNELVID);
  
    // Configure StreamIO object to stream data from RGB video channel to face detection
    videoStreamerRGBFD.registerInput(Camera.getStream(CHANNELNN));
    videoStreamerRGBFD.setStackSize();
    videoStreamerRGBFD.setTaskPriority();
    videoStreamerRGBFD.registerOutput(facerecog);
    if (videoStreamerRGBFD.begin() != 0) {
        Serial.println("StreamIO link start failed");
    }
    // Start video channel for NN
    //Camera.channelBegin(CHANNELNN);
  
    // Start OSD drawing on RTSP video channel
    OSD.configVideo(CHANNELVID, configVID);
    OSD.begin();
    
    // Servo moves to position the angle 180 degree (LOCK CLOSE)
    myservo.write(180);

    client.setLineToken("*******************************************");    //Enter your license

    int button = PUSH_BTN;
    pinMode(button, INPUT_IRQ_RISE);
    digitalSetIrqHandler(button, button_handler);
       
}

char info1_str[15];
char linemsg_str[15];

void Line_Send(char *msg, bool isNotify)
{
    if (client.connect(server, 443))
    {
      client.send(msg,!isNotify);
    }
}

void loop() {
    if (buttonCount == 1)
    {
        buttonPressState = 1;
    }
    else if (buttonCount == 0)
    {
        buttonPressState = 0;
    }
    else
    {
        buttonPressState = 2;
    }
    buttonCount = 0;
      
      if ((buttonPressState == 1) && (button_timeout == 0) && (systemOn == false)) {  // When button is being pressed, systemOn becomes true     
          systemOn = true;

          if (controlState == false)
          {
              Line_Send("RingTrigger",true);
              Camera.channelBegin(CHANNELNN);
              
          }

          button_timeout++;
          for (int count = 0; count < 3; count++) {
              digitalWrite(RED_LED, HIGH);
              digitalWrite(GREEN_LED, HIGH);
              delay(500);
              digitalWrite(RED_LED, LOW);
              digitalWrite(GREEN_LED, LOW);
              delay(500);
          }
      } else if ((button_timeout >= 10) && (systemOn == true)) {
          //Both LED remains on when button not pressed
          systemOn = false;
          button_timeout = 0;
          //OSD.drawText(CHANNELVID, 0, 0,"CLOSE", OSD_COLOR_RED, DOORSTATUSLAYER2);
          snprintf(info1_str, sizeof(info1_str), "CLOSE");
          //printf("system 10 second timeout! button_timeout = %d\r\n", button_timeout);
          if (controlState == false)
          {
              Camera.channelEnd(CHANNELNN);
              OSD.createBitmap(CHANNELVID,RECTTEXTLAYER0);
              OSD.createBitmap(CHANNELVID,RECTTEXTLAYER1);
              OSD.update(CHANNELVID, RECTTEXTLAYER0);
              OSD.update(CHANNELVID, RECTTEXTLAYER1);
          }
      }
      else if ((button_timeout < 10) && (systemOn == true))
      {
          button_timeout++;
          printf("systemOn ing! button_timeout = %d\r\n", button_timeout);
      }
      else
      {          
                   
          if ((buttonPressState == 2) && (controlState == false))
          {
              controlState = true;
              Camera.channelBegin(CHANNELNN);

          }
          else if ((buttonPressState == 2) && (controlState == true))
          {
              controlState = false;
              Camera.channelEnd(CHANNELNN);
              delay(1000);
              OSD.createBitmap(CHANNELVID,RECTTEXTLAYER0);
              OSD.createBitmap(CHANNELVID,RECTTEXTLAYER1);
              OSD.update(CHANNELVID, RECTTEXTLAYER0);
              OSD.update(CHANNELVID, RECTTEXTLAYER1);
          }
      }
    

    if (controlState == true)
    {
        if (Serial.available() > 0) {
          String input = Serial.readString();
          input.trim();
    
          if (input.startsWith(String("REG="))) {
            String name = input.substring(4);
            facerecog.registerFace(name);
          } else if (input.startsWith(String("EXIT"))) {
            facerecog.exitRegisterMode();
          } else if (input.startsWith(String("RESET"))) {
            facerecog.resetRegisteredFace();
          } else if (input.startsWith(String("BACKUP"))) {
            facerecog.backupRegisteredFace();
          } else if (input.startsWith(String("RESTORE"))) {
            facerecog.restoreRegisteredFace();
          }
        }
    }
    
    // Take snapshot and open door for 10 seconds
    if (controlState == false)
    {
        if ((doorOpen == true) && (systemOn == true)) {
            myservo.write(0);
            Serial.println("Opening Door!");
            snprintf(info1_str, sizeof(info1_str), "DOOR OPEN");
            
            delay(10000);
            myservo.write(180);
            digitalWrite(RED_LED, HIGH);
            digitalWrite(GREEN_LED, LOW);
        }
        else
        {
            Serial.println("close Door!");
            snprintf(info1_str, sizeof(info1_str), "DOOR CLOSE");
            doorOpen = false;
        }
    }

    OSD.createBitmap(CHANNELVID,RECTTEXTLAYER0);
    OSD.createBitmap(CHANNELVID,RECTTEXTLAYER1);
    OSD.createBitmap(CHANNELVID,DOORSTATUSLAYER2);
    if (controlState == false)
    {
      if (systemOn)
      {
          OSD.drawText(CHANNELVID, 30, 30, "System ON", OSD_COLOR_WHITE, DOORSTATUSLAYER2);
      }
      else
      {
          OSD.drawText(CHANNELVID, 30, 30,"System OFF", OSD_COLOR_WHITE, DOORSTATUSLAYER2);
      }
      
      if (doorOpen)
      {
          OSD.drawText(CHANNELVID, 30, 70, info1_str, OSD_COLOR_GREEN, DOORSTATUSLAYER2);
      }
      else
      {
          OSD.drawText(CHANNELVID, 30, 70,info1_str, OSD_COLOR_RED, DOORSTATUSLAYER2);
      }
    }
    else
    {
        OSD.drawText(CHANNELVID, 30, 30, "ControlMode", OSD_COLOR_WHITE, DOORSTATUSLAYER2);      
    }
    
    OSD.update(CHANNELVID, RECTTEXTLAYER0);
    OSD.update(CHANNELVID, RECTTEXTLAYER1);
    OSD.update(CHANNELVID, DOORSTATUSLAYER2);
    delay(1000);

}

// User callback function for post processing of face recognition results
void FRPostProcess(std::vector<FaceRecognitionResult> results) {
    uint16_t im_h = configVID.height();
    uint16_t im_w = configVID.width();

    printf("Total number of faces detected = %d\r\n", facerecog.getResultCount());

    OSD.createBitmap(CHANNELVID, RECTTEXTLAYER0);
    OSD.createBitmap(CHANNELVID, RECTTEXTLAYER1);
    if (facerecog.getResultCount() > 0) {
        if (systemOn == true) {
            if (facerecog.getResultCount() > 1) { // Door remain close when more than one face detected
                doorOpen = false;
                digitalWrite(RED_LED, HIGH);
                digitalWrite(GREEN_LED, LOW);
            } else {
                FaceRecognitionResult singleItem = results[0];
                if (String(singleItem.name()) == String("unknown")) {  // Door remain close when unknown face detected
                    doorOpen = false;
                    digitalWrite(RED_LED, HIGH);
                    digitalWrite(GREEN_LED, LOW);
                } else { // Door open when a single registered face detected
                    digitalWrite(RED_LED, LOW);
                    digitalWrite(GREEN_LED, HIGH);
                    if (doorOpen == false)
                    {
                        doorOpen = true;
                        Line_Send(linemsg_str,true);
                    }
                }
            }
        }
    }
    else
    {
      
    }

    for (uint32_t i = 0; i < facerecog.getResultCount(); i++) {
        FaceRecognitionResult item = results[i];
        // Result coordinates are floats ranging from 0.00 to 1.00
        // Multiply with RTSP resolution to get coordinates in pixels
        int xmin = (int)(item.xMin() * im_w);
        int xmax = (int)(item.xMax() * im_w);
        int ymin = (int)(item.yMin() * im_h);
        int ymax = (int)(item.yMax() * im_h);

        uint32_t osd_color;
        int osd_layer;
        // Draw boundary box
        if (String(item.name()) == String("unknown")) {
            osd_color = OSD_COLOR_RED;
            osd_layer = RECTTEXTLAYER0;
        } else {
            osd_color = OSD_COLOR_GREEN;
            osd_layer = RECTTEXTLAYER1;
        }
        printf("Face %d name %s:\t%d %d %d %d\n\r", i, item.name(), xmin, xmax, ymin, ymax);
        OSD.drawRect(CHANNELVID, xmin, ymin, xmax, ymax, 3, osd_color, osd_layer);

        // Print identification text above boundary box
        char text_str[40];
        snprintf(text_str, sizeof(text_str), "Face:%s", item.name());
        OSD.drawText(CHANNELVID, xmin, ymin - OSD.getTextHeight(CHANNELVID), text_str, osd_color, osd_layer);
    }
    OSD.update(CHANNELVID, RECTTEXTLAYER0);
    OSD.update(CHANNELVID, RECTTEXTLAYER1);
}
