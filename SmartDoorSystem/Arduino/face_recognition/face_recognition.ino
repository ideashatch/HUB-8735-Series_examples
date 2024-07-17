/*

 Example guide:
 https://www.amebaiot.com/en/amebapro2-arduino-neuralnework-face-recognition/

 Face registration commands
 --------------------------
 Point the camera at a target face and enter the following commands into the serial monitor,
 Register face:                       "REG={Name}"  Ensure that there is only one face detected in frame
 Remove face:                         "DEL={Name}"  Remove a registered face
 Reset registered faces:              "RESET"       Forget all previously registered faces
 Backup registered faces to flash:    "BACKUP"      Save registered faces to flash
 Restore registered faces from flash: "RESTORE"     Load registered faces from flash

 NN Model Selection
 -------------------
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

#include "WiFi.h"                        // 引入WiFi庫以實現網路連接
#include "StreamIO.h"                    // 引入StreamIO庫以處理輸入/輸出流
#include "VideoStream.h"                 // 引入VideoStream庫以處理視頻流
#include "RTSP.h"                        // 引入RTSP庫以實現即時流協議功能
#include "NNFaceDetectionRecognition.h"  // 引入NNFaceDetectionRecognition庫以實現基於神經網路的臉部檢測和識別
#include "VideoStreamOverlay.h"          // 引入VideoStreamOverlay庫以在視頻流上疊加數據
#include <PubSubClient.h>                // 引入PubSubClient庫以實現MQTT通信

#define CHANNEL   0
#define CHANNELNN 3

// 自訂神經網路的解析度
#define NNWIDTH  576
#define NNHEIGHT 320

VideoSetting config(VIDEO_FHD, 30, VIDEO_H264, 0);  // 設定視頻配置為Full HD（1080p）、30幀每秒、H.264編碼，無其他選項
VideoSetting configNN(NNWIDTH, NNHEIGHT, 10, VIDEO_RGB, 0);  // 設定神經網路的視頻配置，使用自訂寬度和高度、10幀每秒、RGB格式，無其他選項
NNFaceDetectionRecognition facerecog;  // 創建NNFaceDetectionRecognition對象，用於臉部檢測和識別
RTSP rtsp;  // 創建RTSP對象，用於即時流協議
StreamIO videoStreamer(1, 1);  // 創建StreamIO對象，用於視頻流輸入輸出，通道數為1進1出
StreamIO videoStreamerFDFR(1, 1);  // 創建StreamIO對象，用於臉部檢測和識別的視頻流輸入輸出，通道數為1進1出
StreamIO videoStreamerRGBFD(1, 1);  // 創建StreamIO對象，用於RGB格式臉部檢測的視頻流輸入輸出，通道數為1進1出


char ssid[] = "IOT01";             // 設定網路名稱
char pass[] = "1234567890";        // 設定網路密碼
int status = WL_IDLE_STATUS;       // 連線狀態變數

IPAddress ip;
int rtsp_portnum;
char buf[128];
char path[128];

//MQTT相關設定
char mqttServer[] = "120.102.36.38";        //MQTT伺服器網址        
//char subscribeTopic[] = "QAQ";            //MQTT訂閱主題
void callback(char* topic, byte* payload, unsigned int length);  // 聲明一個回調函數，用於處理接收到的MQTT訊息
WiFiClient wifiClient;  // 創建一個WiFiClient對象，用於WiFi連接
PubSubClient client(mqttServer, 5007, callback, wifiClient);  // 創建一個PubSubClient對象，用於MQTT通信，連接到指定的MQTT伺服器和端口，並設置回調函數和WiFi客戶端


int button = 8;       //設定出門按鈕腳位
int buttonState;      //設定出門按鈕狀態
int LED_pin = 3;      //設定開關門狀態燈腳位
int Reedswitch = 9;   //設定磁簧開關腳位
int ReedswitchState;  //設定磁簧開關狀態腳位

// 訂閱主題資料接收函式
void callback(char* topic, byte* payload, unsigned int length)
{
    //顯示是哪個主題傳資料
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (unsigned int i = 0; i < length; i++) {
      Serial.print((char)(payload[i]));
    }
    Serial.println();

    // 為了重新發布此有效負載，必須製作一個副本
    // 因為原始有效負載緩衝區將會被覆蓋
    // 建置 PUBLISH 資料包.

    /*// 為有效負載副本分配正確的記憶體量
    byte* p = (byte*)(malloc(length));
    // 將有效負載複製到新緩衝區
    memcpy(p, payload, length);
    client.publish(publishTopic, p, length);
    // 釋放記憶體
    free(p);*/
}

void setup()
{
    Serial.begin(115200);           //開啟串列傳輸
    pinMode(LED_pin, OUTPUT);       //設定開關門狀態燈為輸出
    digitalWrite(LED_pin, LOW);     //初始狀態燈
    pinMode(button, INPUT);         //設定出門按鈕為輸入
    pinMode(Reedswitch, INPUT);     //設定磁簧開關為輸入
    // 嘗試連線 Wifi 網路：
    while (status != WL_CONNECTED) {
        Serial.print("Attempting to connect to WPA SSID: ");
        Serial.println(ssid);
        status = WiFi.begin(ssid, pass);

        // wait 2 seconds for connection:
        delay(2000);
    }

    ip = WiFi.localIP();

    // 設定攝影機視訊頻道的視訊格式訊息
    // 根據您的 WiFi 網路品質調整位元率
    config.setBitrate(2 * 1024 * 1024);    // 建議使用 2Mbps 進行 RTSP 串流傳輸，以防止網路擁塞
    Camera.configVideoChannel(CHANNEL, config);
    Camera.configVideoChannel(CHANNELNN, configNN);
    Camera.videoInit();

    // 配置RTSP對應的視訊格式訊息
    rtsp.configVideo(config);
    rtsp.begin();
    rtsp_portnum = rtsp.getPort();

    // 設定人臉辨識模型
    // 選擇神經網路（NN）任務和模型
    facerecog.configVideo(configNN);
    facerecog.modelSelect(FACE_RECOGNITION, NA_MODEL, DEFAULT_SCRFD, DEFAULT_MOBILEFACENET);
    facerecog.begin();
    facerecog.setResultCallback(FRPostProcess);
    facerecog.restoreRegisteredFace();

    // 設定 StreamIO 物件將資料從視訊通道串流傳輸到 RTSP
    videoStreamer.registerInput(Camera.getStream(CHANNEL));
    videoStreamer.registerOutput(rtsp);
    if (videoStreamer.begin() != 0) {
        Serial.println("StreamIO link start failed");
    }
    // 從視訊頻道開始資料流
    Camera.channelBegin(CHANNEL);
    // 設定StreamIO物件將RGB視訊通道的資料串流到人臉偵測
    videoStreamerRGBFD.registerInput(Camera.getStream(CHANNELNN));
    videoStreamerRGBFD.setStackSize();
    videoStreamerRGBFD.setTaskPriority();
    videoStreamerRGBFD.registerOutput(facerecog);
    if (videoStreamerRGBFD.begin() != 0) {
        Serial.println("StreamIO link start failed");
    }

    // 啟動 NN 的視訊頻道
    Camera.channelBegin(CHANNELNN);

    // 在 RTSP 視訊頻道上啟動 OSD 繪製
    OSD.configVideo(CHANNEL, config);
    OSD.begin();
}

void loop()
{
    //讀取磁簧開關與出門按鈕狀態
    buttonState = digitalRead(button);
    ReedswitchState = digitalRead(Reedswitch);
    //設定串列命令
    if (Serial.available() > 0) {
        String input = Serial.readString();
        input.trim();

        if (input.startsWith(String("REG="))) {            //設定命令"REG=人名",註冊人臉
            String name = input.substring(4);
            facerecog.registerFace(name);
        } else if (input.startsWith(String("DEL="))) {    //設定命令"DEL=人名",刪除特定的已註冊面孔
            String name = input.substring(4);
            facerecog.removeFace(name);
        } else if (input.startsWith(String("RESET"))) {   //設定命令"RESET",忘記所有以前註冊的面孔
            facerecog.resetRegisteredFace();
        } else if (input.startsWith(String("BACKUP"))) {  //設定命令"BACKUP",已註冊面孔的副本保存到閃存中
            facerecog.backupRegisteredFace();
        } else if (input.startsWith(String("RESTORE"))) { //設定命令"RESTORE",從閃存中加載已註冊的人臉
            facerecog.restoreRegisteredFace();
        }
    }
    if(ReedswitchState == LOW){          //當磁簧開關被闔上一起，代表門已被關上    
      client.publish("IOT01door", "0");           //mqtt傳送指令至app，代表開門     
      digitalWrite(LED_pin, LOW);
    }
    delay(2000);
    OSD.createBitmap(CHANNEL);
    OSD.update(CHANNEL);
    if (!client.connected()) {
      // 如果客戶端未連線
      reconnect();
      // 重新連線
    }
}

// 用於人臉辨識結果後處理的使用者回呼函數
void FRPostProcess(std::vector<FaceRecognitionResult> results)
{
    uint16_t im_h = config.height();
    uint16_t im_w = config.width();
   
    //列印RTSP網址以及埠號
    Serial.print("Network URL for RTSP Streaming: ");
    Serial.print("rtsp://");
    Serial.print(ip);
    Serial.print(":");
    Serial.println(rtsp_portnum);
    Serial.println(" ");
    
    //列印偵測到第N個人臉
    printf("Total number of faces detected = %d\r\n", facerecog.getResultCount());
    OSD.createBitmap(CHANNEL);

    if (facerecog.getResultCount() > 0) {
        for (int i = 0; i < facerecog.getResultCount(); i++) {
            FaceRecognitionResult item = results[i];
            // 結果座標是從 0.00 到 1.00 的浮點數
            // 乘以 RTSP 解析度以獲得以像素為單位的座標
            int xmin = (int)(item.xMin() * im_w);
            int xmax = (int)(item.xMax() * im_w);
            int ymin = (int)(item.yMin() * im_h);
            int ymax = (int)(item.yMax() * im_h);

            uint32_t osd_color;
            //如果偵測到的人臉未註冊過,辨識框會是紅色並列印unknown,辨識框會是綠色框並列印註冊人名
            if (String(item.name()) == String("unknown")) {
                osd_color = OSD_COLOR_RED;
            } else {
                osd_color = OSD_COLOR_GREEN;
            }

            // 繪製邊界框
            printf("Face %d name %s:\t%d %d %d %d\n\r", i, item.name(), xmin, xmax, ymin, ymax);
            String hum = item.name();
            Serial.println(hum);
            
            //如果在沒有按下出門按鈕的情況下辨識到人臉，代表是從外面辨識
            if(hum == "UserA" && buttonState == 1){   //辨識結果是否符合已註冊身分"Tony"
               
              client.publish("IOT01name", "UserA");        //mqtt傳送指令至app，代表"Tony"回來了
              delay(10);
              client.publish("IOT01door", "1");           //mqtt傳送指令至app，代表開門
              digitalWrite(LED_pin, HIGH);           //開啟開關門狀態燈
              hum = "";
            }
            if(hum == "UserB" && buttonState == 1){//辨識結果是否符合已註冊身分"Ashley"
              client.publish("IOT01name", "UserB");     //mqtt傳送指令至app，代表"Ashley"回來了
              delay(10);
              client.publish("IOT01door", "1");           //mqtt傳送指令至app，代表開門
              digitalWrite(LED_pin, HIGH);          //開啟開關門狀態燈
              hum = "";
            }
            //如果在按下出門按鈕的情況下辨識到人臉，代表是從裡面辨識
            if(hum == "UserA" && buttonState == 0){ //辨識結果是否符合已註冊身分"Tony"
              client.publish("IOT01out", "UserA");       //mqtt傳送指令至app，代表"Tony"出門了
              delay(10);
              client.publish("IOT01door", "1");           //mqtt傳送指令至app，代表開門
              hum = "";
            }
            if(hum == "UserB" && buttonState == 0){//辨識結果是否符合已註冊身分"Ashley"
              client.publish("IOT01out", "UserB");      //mqtt傳送指令至app，代表"Ashley"出門了
              delay(10);
              client.publish("IOT01door", "1");           //mqtt傳送指令至app，代表開門
              hum = "";
            }
            OSD.drawRect(CHANNEL, xmin, ymin, xmax, ymax, 3, osd_color);

            // 在邊界框上方列印標識文本
            char text_str[40];
            snprintf(text_str, sizeof(text_str), "Face:%s", item.name());
            OSD.drawText(CHANNEL, xmin, ymin - OSD.getTextHeight(CHANNEL), text_str, osd_color);
        }
    }
    OSD.update(CHANNEL);
}

void reconnect() {
  // 連線函數
  while (!client.connected()) {
    // 當未連線時執行迴圈
    Serial.print("Attempting MQTT connection...");
    // 嘗試連線至 MQTT 伺服器
    String clientId = "ESP8266Client-";
    // 建立客戶端識別碼
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      // 如果成功連線
      Serial.println("connected");
      // 顯示連線成功
      client.subscribe("QAQ");
      // 訂閱特定主題
    } else {
      // 如果連線失敗
      Serial.print("failed, rc=");
      // 顯示失敗及錯誤碼
      Serial.print(client.state());
      // 顯示 MQTT 客戶端狀態
      Serial.println(" try again in 5 seconds");
      // 顯示嘗試重新連線訊息
      delay(6000);
      // 等待 6 秒再重新嘗試
    }
  }
}
