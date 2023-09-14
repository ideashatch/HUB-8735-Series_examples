#include <ArduinoJson.h>
#include <ArduinoJson.hpp>
#include <WiFi.h>
#include <WiFiClient.h>
#include "WiFiSSLClient.h"
#include "WiFiUdp.h"
#include "StreamIO.h"
#include "NNFaceDetectionRecognition.h"
#include "VideoStreamOverlay.h"
#include "AmebaFatFS.h"
#include "SSLLineNotify.h"
#include <ArduinoHttpClient.h>
#include <time.h>
#include <SD.h>

#define PU02_Serial Serial1 
#include "PU02API.h"

#define CHANNELVID  0 // Channel for RTSP streaming
#define CHANNELJPEG 1 // Channel for taking snapshots
#define CHANNELNN   3 // RGB format video for NN only avaliable on channel 3

// Customised resolution for NN
#define NNWIDTH 576
#define NNHEIGHT 320

// OSD layers
#define RECTTEXTLAYER0 OSDLAYER0
#define RECTTEXTLAYER1 OSDLAYER1
#define DOORSTATUSLAYER2 OSDLAYER2

// Select the maximum number of snapshots to capture
#define MAX_UNKNOWN_COUNT 5
#define CHANNEL 0
#define SERVO_PIN  8

VideoSetting configVID(VIDEO_FHD, CAM_FPS, VIDEO_H264, 0);
VideoSetting configJPEG(VIDEO_FHD, CAM_FPS, VIDEO_JPEG, 1);
VideoSetting configNN(NNWIDTH, NNHEIGHT, 10, VIDEO_RGB, 0);

StreamIO videoStreamer(1, 1);
StreamIO videoStreamerFDFR(1, 1);
StreamIO videoStreamerRGBFD(1, 1);

char ssid[] = "WIFI-SSID"; // your network SSID (name)
char pass[] = "WIFI-PASSWORD";    // your network password
int status = WL_IDLE_STATUS;

// ===========================================================================================
bool buttonState = false;
bool regFace = true;
bool unknownDetected = false;
bool roundBegan = false;
int unknownCount = 0;

//status flag
bool systemOn = false;
bool controlState = false;
bool doorOpen = false;

// ===========================================================================================
// LINE Notify
LineNotify lineClient;

// File Initialization
AmebaFatFS fs;
char info1_str[15];
char linemsg_str[15];
// LINE TOKEN
char* LINE_TOKEN = "{YOUR_LINE_TOKEN}";
const char* LINE_SERVER = "notify-api.line.me";
const int LINE_PORT = 443;
// ===========================================================================================
// Memory space for image buffer
uint32_t img_addr = 0;
uint32_t img_len = 0;

// ===========================================================================================
WiFiClient wifiClient;
HttpClient httpClient = HttpClient(wifiClient, LINE_SERVER, LINE_PORT);
WiFiSSLClient sslClient;


// Buffer to hold incoming and outgoing packets
byte packetBuffer[48];

// Create a WiFiUDP instance
WiFiUDP udp;
unsigned int localPort = 8888;  // Local port to listen for NTP responses
const int NTP_PACKET_SIZE = 48;  // NTP packet size
const char* ntpServer = "pool.ntp.org";
const int timeZone = 8; 

// ===========================================================================================
// Variables for storing mmWave's previous values, and message sending period
int previousValue = 0;
bool initialReading = true;
unsigned long lastMessageTime = 0;
const unsigned long messageCooldown = 180000; // 180 seconds cooldown

const int threshold = 17;
const int fluctuationRange = 5; // Allow some fluctuation around the threshold value
bool initialMessageSent = false;
// ===========================================================================================
// If PU02 returns value, check if the distance value matches condition,
// If so, send LINE Notify Message to alert or notify user
bool checkAndNotify(int sensorValue) 
{
    // Check if the distance value is below the upper threshold (indicating something is close)
    if (sensorValue < threshold) 
    {
        // If this is the initial reading, update the previousValue and set initialReading to false
        if (initialReading) 
        {
            previousValue = sensorValue;
            initialReading = false;           
            return false;
        }
        // Check if the absolute difference between the current value and the previous value
        // is greater than the fluctuation range
        int difference = abs(sensorValue - previousValue);
        if (difference > fluctuationRange) 
        {
            // Enough fluctuation detected, trigger the notification
            unsigned long currentTime = millis();
            if (!initialMessageSent || (currentTime - lastMessageTime >= messageCooldown)) 
            {
                // Send the message here (you can modify this part to use your Line Notify function)
                Serial.println("Something is getting close!");
                capstoreImage(fetchNetworkTime());
                sendLineMsg("Something/Someone approached to your doorstep.&stickerPackageId=446&stickerId=2016", true);

                // Update the initial message status and the last message time
                initialMessageSent = true;
                lastMessageTime = currentTime;
                return true;
            } 
            else if(currentTime - lastMessageTime <= messageCooldown)
            {
                Serial.println("Message Cool Down.");
            }
        }
    } 

    // Update the previous value for the next check
    previousValue = sensorValue;
    return false;
}

// ===========================================================================================
// Get real-time network time(UNIX time), then parse to formatted time
time_t fetchNetworkTime() 
{
    byte packetBuffer[NTP_PACKET_SIZE];
    // Clear the packet buffer
    memset(packetBuffer, 0, NTP_PACKET_SIZE);

    // Set the first byte of the packet to 0x1B
    packetBuffer[0] = 0x1B;

    // Send an NTP request to the time server
    udp.beginPacket(ntpServer, 123);  // NTP port is 123
    udp.write(packetBuffer, NTP_PACKET_SIZE);
    udp.endPacket();

    // Wait for the response packet
    delay(1000);
    if (udp.parsePacket()) 
    {
        // Read the packet into the buffer
        udp.read(packetBuffer, NTP_PACKET_SIZE);

        // Extract the seconds since Jan 1, 1900
        unsigned long secondsSince1900 = (unsigned long)(packetBuffer[40] << 24) |
                                        (unsigned long)(packetBuffer[41] << 16) |
                                        (unsigned long)(packetBuffer[42] << 8) |
                                        (unsigned long)(packetBuffer[43]);

        // Convert NTP time to Unix timestamp
        const unsigned long seventyYears = 2208988800UL;
        unsigned long epochTime = secondsSince1900 - seventyYears;

        // Apply time zone offset
        time_t localTime = epochTime + (timeZone * 3600);

        return localTime;
    }
    return 0;

}
// ===========================================================================================
// Send LINE Message (Only text)
void sendLineMsg(char *msg, bool isNotify)
{
    if (lineClient.connect(LINE_SERVER, 443))
    {
        lineClient.send(msg, !isNotify);
    }
}

// ===========================================================================================
// Capture and store images in SD Card
void capstoreImage(time_t unixTime)
{
    fs.begin();

    // Parse Unix Time to YYYY-MM-DD HH-mm
    struct tm *timeinfo;
    timeinfo = gmtime(&unixTime);

    char fileNameTime[75]; 

    snprintf(fileNameTime, sizeof(fileNameTime), "%04d-%02d-%02d_%02d-%02d",
            timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
            timeinfo->tm_hour, timeinfo->tm_min);

    String filePath = String(fs.getRootPath()) + "Stranger_" + String(fileNameTime) + ".jpg";
    File file = fs.open(filePath);

    char path[128];
    strcpy(path, filePath.c_str());

    delay(300);
    Camera.getImage(CHANNEL, &img_addr, &img_len);     
    file.write((uint8_t*)img_addr, img_len);
    file.close();
    fs.setLastModTime(path, 
                      timeinfo->tm_year + 1900, 
                      timeinfo->tm_mon + 1, 
                      timeinfo->tm_mday, 
                      timeinfo->tm_hour, 
                      timeinfo->tm_min, 
                      timeinfo->tm_sec);
    fs.end();
}

// ===========================================================================================
void setup() 
{
    Serial.begin(115200);
    Serial1.begin(115200);
    // Attempt to connect to Wifi network:
    while (status != WL_CONNECTED) 
    {
        Serial.print("Attempting to connect to WPA SSID: ");
        Serial.println(ssid);
        status = WiFi.begin(ssid, pass);

        // wait 2 seconds for connection:
        delay(2000);
    }

    // Configure camera video channels with video format information
    // Camera.configVideoChannel(CHANNELVID, configVID);/
    Camera.configVideoChannel(CHANNEL, configJPEG);
    // Camera.configVideoChannel(CHANNELNN, configNN);
    Camera.videoInit();
        
    // Start data stream from video channel
    Camera.channelBegin(CHANNEL);

    // Set Line Notify Token
    lineClient.setLineToken(LINE_TOKEN);    //Enter your license
    lineClient.stop();
     
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    // Initialize UDP 
    udp.begin(localPort);        
}

void loop()
{
    // Get mmWave value, and determine whether is something closing in 
    int sensorValue = PU02_Len(50,2); 
    checkAndNotify(sensorValue);
    delay(150);
}

void printFormattedTime(time_t unixTime, char* buffer, size_t bufferSize) 
{
    struct tm *timeinfo;
    timeinfo = gmtime(&unixTime);

    char timeStr[75];
    snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d",
            timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
            timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

    Serial.println(timeStr);
}

// ===========================================================================================
void printWifiStatus() 
{
    // print the SSID of the network you're attached to:
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());

    // print your WiFi IP address:
    IPAddress ip = WiFi.localIP();
    Serial.print("IP Address: ");
    Serial.println(ip);

    // print the received signal strength:
    long rssi = WiFi.RSSI();
    Serial.print("signal strength (RSSI):");
    Serial.print(rssi);
    Serial.println(" dBm");
}

