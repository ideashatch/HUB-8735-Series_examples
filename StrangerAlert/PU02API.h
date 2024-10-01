/* 雷捷電子 PU02 雷達波感測模組 FW:1.3A
 1.使用 UART 方式溝通 
 2.設定變數(PU02_Serial)，例如: #define PU02_Serial Serial3 
 3.設定溝通速率 115200  
*/

byte PU02_Respone_Format[14]={0x55,0xAA,0x21,0x15,0x08,0x01,0x01,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
byte PU02_Read_Data[8];
byte PU02_Count=0;
int PU02_Distance;
 
int PU02_Len(int set_len,int set_time)
{
    byte i=0;
    bool flag=false;  
    while(!PU02_Serial.available()) 
        return -1;
    for (i=0;i<6;i++)
    {
        PU02_Serial.readBytes(PU02_Read_Data,1);
        if (PU02_Read_Data[0] !=PU02_Respone_Format[i]) {i=0;flag=false;continue;}
        flag=true;
    }
    
    if (flag)
    {
        PU02_Serial.readBytes(PU02_Read_Data,8);
        /*
        for (i=0;i<8;i++)
        {
        int x=PU02_Read_Data[i];
        if (x<0x10) Serial.print("0");  
        Serial.print(PU02_Read_Data[i],HEX);
        }
        Serial.println();
        */
        PU02_Distance=PU02_Read_Data[4]*0x100 + PU02_Read_Data[3];
        //Serial.println(PU02_Distance,DEC);
        if ( PU02_Distance>1 && PU02_Distance<set_len) PU02_Count++;
        if (PU02_Count>set_time)
        {
            PU02_Count=0;
            Serial.print(PU02_Distance,DEC);
            Serial.println(" get");
            return PU02_Distance; 
        }
    }
    return -1; 
}
