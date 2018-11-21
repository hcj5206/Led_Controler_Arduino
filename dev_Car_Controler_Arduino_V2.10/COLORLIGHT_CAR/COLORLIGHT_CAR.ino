/*
 * V1.02 TCP协议
 *此段代码为6路彩灯控制
 *接受10位字符串，开头为“(”，结尾为“)”，截至符号为%，前6位控制6路灯，7位为闪烁指令，8位为闪烁频率指令，
*9为软复位，10为WIFI重置，默认为0，启动命令均为“9“；
*Start_Send_Imessage();单片机配置完网络后，开机连接成功后立即发送自身IP、SN码、电量给PHP，
   直至PHP接收成功，如果数据库存在即更新，不存在即插入。避免了手动更新数据库。
*连接失败 红灯闪烁，连接成功绿灯常亮2秒
*2018年8月27日  多主机进行应答
*2018年8月28日 V1.03 IP显示错误，已更改
*2018年9月27日 V1.04新版子配置网络时,其AP_name无法正常显示,分析:第一次配置时ROM已经写入默认的指针,应该直接修改 AP_name为Car-SN码
*2018年9月29日 v1.10 配置WIFI成功后 连接上wifi后,关闭AP模式.
*2018年9月29日 增添OTA空中升级 取消闪烁频率设置位,改为OTA空中升级
*2018年10月26日 V2.00更改为彩灯带
*2018年11月15日  V2.10 ,解决V2.00存在 第二路灯亮灯异常, 
 *
*/
#include <dummy.h>
#include <ESP8266WiFi.h>
#include <DYWiFiConfig.h>
#include <DYStoreConfig.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Ticker.h>
#define Version "V2.10"  //版本编号
//rgb彩灯
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif
#define PIN            13
#define NUMPIXELS      20
#define NUMSHELF   5  //总格数
int SHELF_LED_NUM=NUMPIXELS/NUMSHELF;//每格灯的个数
int SHELF[]={0,4,8,12,16};
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

//rgb彩灯


#define mcu_port 400
#define DEF_WIFI_SSID        "D1"
#define DEF_WIWI_PASSWORD     "01234567890"
#define AP_NAME "Car_hanhai"  //dev
#define host "http://192.168.1.200/Led_control.*php"//PHP地址，需更改##
#define LED_NUM 6//LED数量 ##
#define PCF_NUM 8 //PCF板子数量##
#define POWER_DEV 0.6 //电池电压偏差值
Ticker flipper;
DYWiFiConfig wificonfig;
DYStoreConfig storeconfig;
ESP8266WebServer webserver(80);
#define MAX_SRV_CLIENTS 3   //最大同时联接数，即你想要接入的设备数量，8266tcpserver只能接入五个，哎
WiFiServer server(mcu_port);//你要的端口号，随意修改，范围0-65535
WiFiClient serverClients[MAX_SRV_CLIENTS];
String test_connect="";
String test_connect_bin="";
String payload="";
String mcu_sn="";
String mcu_power="";  
char *mcu_ip=NULL;

String order_all;//指令行  一共20位
String order_led;//颜色灯路行 一共16位
const char  *ap_name=NULL;
char LED_LIGHT[LED_NUM];
char COLOR_PIN[LED_NUM][3];
char date_buffer;
int LED_PIN;
float twinkle_frequency=0.2 ;//闪烁频率 1
boolean twinkle=0;//闪烁标识码  1闪 0不闪 最后一位为1闪 0不闪
int State_Send_Imessage=1;
uint8_t i;
void OTAsetup(){
    // Hostname defaults to esp8266-[ChipID]
   ArduinoOTA.setHostname(ap_name);
   ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("ArduinoOTA really!");
}
void setup() {
  Serial.begin(115200);
  mcu_sn=(String)ESP.getChipId();
  String a = "Car-" + mcu_sn;
  ap_name = a.c_str();
  wifisetup();
  RGB_setup();   
  wificonfig.enableAP();
 while (WiFi.status() != WL_CONNECTED)
    {
      Serial.print(".");
        delay(100);
        wificonfig.handle();
        led_state_connecting();
    }
    led_state_connected();
    Serial.println("my WIFI connected");
    server.begin();
    server.setNoDelay(true);  //加上后才正常些
    measure_mcu_power();//测电压
	  //wificonfig.disableAP();
    OTAsetup();
    
  //  Start_Send_Imessage();//开机后发送自身Ip信息，直到数据库接收到，数据库若存在则更新，不存在则插入
 
}
void(* resetFunc) (void) = 0;//软复位子函数，地址位指向0；
void RGB_setup(){
  
  pixels.begin(); // This initializes the NeoPixel library.
}
void loop() {
    wificonfig.handle();
    TCP_connect();
    while(payload!=""){
         Serial.print("code_back=: ");
         Serial.println(payload);
          int n=payload.indexOf('(');
          int m=payload.indexOf(')');
          order_all=payload.substring(n+1,m);
			  order_led = payload.substring(n + 1, n + 1 + LED_NUM);
			  if (order_all[(LED_NUM)] == '1') { twinkle = 1; twinkle_frequency = 0.3;Serial.println("shanshuo"); } //17号位控制是否闪烁
			  if (order_all[(LED_NUM)] == '0')  twinkle = 0;
			  if (order_all[(1 + LED_NUM)] == '9') {  ArduinoOTA.handle();     }//第8位  OTA升级
			  if (order_all[(2 + LED_NUM)] == '9') { resetFunc(); Serial.print("restart===:"); } //第19位，软复位
			  if (order_all[(3 + LED_NUM)] == '9') wificonfig_clear();//第20位如果为9，WIFI重置。
			  light_control(order_led);
			  TCP_connect();
			  //payload="";
			  delay(500);
      }
  }

void Start_Send_Imessage(){
  while(State_Send_Imessage==1){ 
        wificonfig.handle();
      HTTPClient http;
      INT_IP(int(WiFi.localIP()));
      String http_data=(String)host+"?mcu_sn="+mcu_sn+"&mcu_ip="+mcu_ip+"&mcu_power="+mcu_power+"&mcu_port="+mcu_port;
       http.begin(http_data); 
       Serial.print("[HTTP] GET...\n");
        int httpCode = http.GET();
           if(httpCode > 0) {
            Serial.printf("[HTTP] GET... code: %d\n", httpCode);
            if(httpCode==404){
               led_state_connecting(); //连接失败闪烁红灯
               Serial.printf(host);
                Serial.println(" No File");
     delay(1);
            }
            if(httpCode == HTTP_CODE_OK) {
              led_state_connected();//连接成功常亮蓝灯2秒，
              State_Send_Imessage=0;
            }
        } else {
            Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
          led_state_connecting(); //连接失败闪烁红灯
        }
        http.end();
    }
}
void measure_mcu_power(){
 int sensorValue = analogRead(A0);
 //float voltage = sensorValue * (5.0 / 1023.0);
 float voltage = sensorValue  / 1023.0*5+ POWER_DEV;
  mcu_power  =String(voltage)+"V";
}
void TCP_connect(){
  if (server.hasClient()){
       INT_IP(int(WiFi.localIP()));
    for(i = 0; i < MAX_SRV_CLIENTS; i++){
      //find free/disconnected spot
      if (!serverClients[i] || !serverClients[i].connected()){
        if(serverClients[i]) serverClients[i].stop();
        serverClients[i] = server.available();
        Serial.print("New client: "); Serial.print(i);
        break;
      }
    }
    //no free/disconnected spot so reject
    if ( i == MAX_SRV_CLIENTS) {
       WiFiClient serverClient = server.available();
       serverClient.stop();
        Serial1.println("Connection rejected ");
    }
  }
for(int serverClient_num=0;serverClient_num<MAX_SRV_CLIENTS;serverClient_num++){
  if  (serverClients[serverClient_num].available()>0) {
    int rows1=0;
    int rows2=0;
    int num_totals=0;
    String buff=serverClients[serverClient_num].readStringUntil('%');//接受上位机传来的信息，格式为{/（xxx……xx）}，x共20位
    Serial.print("buff=");
      Serial.println(buff);
     int L=buff.indexOf('(');
     int R=buff.indexOf(')');
     int V=buff.length();
     String code_rev=buff.substring(L+1,R);
     String Try_back="Try_back";
     if(code_rev==Try_back){
     String reback=(String)"Get!"+"&"+mcu_sn+"&"+mcu_ip+"&"+mcu_port+"&"+mcu_power+"&"+Version+"&"+buff+"\n";
     serverClients[serverClient_num].print(reback);
     }
     for(int i=1;i<=LED_NUM+4;i++){
      rows1=rows1+isdigit(buff[L+i]);
      rows2=rows2+buff[L+i]-48;
     }
  //   Serial.printf("rows=%d,buff.length()=%d,r=%d,buff_last=%d   ",rows2,buff.length(),R,int((buff[R+1]-48)%10));
   
     if((rows2%10)==int((buff[R+1]-48)%10)) {num_totals=1;}
     else num_totals=0;
     // Serial.printf("L=%d,R=%d,V=%d,rows1=%d   ",L,R,V,rows1);

     if(L!=-1&&R!=-1&&V==13&&rows1==10){
      if(num_totals==1){
       payload=buff;
      measure_mcu_power();
    String reback=(String)"OK!"+"&"+mcu_sn+"&"+mcu_ip+"&"+mcu_port+"&"+mcu_power+"&"+Version"&"+buff+"\n";
     serverClients[serverClient_num].print(reback);
      }
      else {serverClients[serverClient_num].print("error!CODE:400\n");}
     }
      else if(code_rev!=Try_back)  serverClients[serverClient_num].print("error!CODE:100\n");
    
  }
}
  
}

void INT_IP(int ip_add)//整形转为字符串IP（x.x.x.x）
{
    char str_ip_index[4]={'\0'};
     unsigned int ip_int,ip_int_index[4],ip_temp_numbr=24;  
         int j =0,a=3;   
    for(j=0;j<4;j++)  
    {  
        ip_int_index[j]=(ip_add>>ip_temp_numbr)&0xFF;  
        ip_temp_numbr-=8;  
    }  
  
    if ((mcu_ip=(char *)malloc(17*sizeof(char)))==NULL)  
    {  
      return;
    }  
    sprintf(mcu_ip,"%d.%d.%d.%d",ip_int_index[3],ip_int_index[2],ip_int_index[1],ip_int_index[0]);  
}
//0 0 0 红绿蓝
void light_control(String payload){
     for(int led = 0;led<LED_NUM-1;led++){
       date_buffer= payload.charAt(led);
        if(date_buffer=='0') { LED_LIGHT[led]='0';
          COLOR_PIN[led][0]=0;COLOR_PIN[led][1]=0; COLOR_PIN[led][2]=0; }
        if(date_buffer=='1')  { LED_LIGHT[led]='1';
          COLOR_PIN[led][0]=0;COLOR_PIN[led][1]=0; COLOR_PIN[led][2]=255; }
        if(date_buffer=='2')  { LED_LIGHT[led]='2';
          COLOR_PIN[led][0]=0;COLOR_PIN[led][1]=255; COLOR_PIN[led][2]=0; }
        if(date_buffer=='4')  { LED_LIGHT[led]='4';
          COLOR_PIN[led][0]=0;COLOR_PIN[led][1]=255; COLOR_PIN[led][2]=255; }
        if(date_buffer=='3')  { LED_LIGHT[led]='3';
          COLOR_PIN[led][0]=255;COLOR_PIN[led][1]=0; COLOR_PIN[led][2]=0; }
        if(date_buffer=='5') { LED_LIGHT[led]='5';
          COLOR_PIN[led][0]=255;COLOR_PIN[led][1]=0; COLOR_PIN[led][2]=255; }
        if(date_buffer=='6')  { LED_LIGHT[led]='6';
          COLOR_PIN[led][0]=255;COLOR_PIN[led][1]=255; COLOR_PIN[led][2]=0; }
        if(date_buffer=='7')  { LED_LIGHT[led]='7';
          COLOR_PIN[led][0]=255;COLOR_PIN[led][1]=255; COLOR_PIN[led][2]=255; }
          turnon_led(led);//亮灯
          //

          // #define NUMPIXELS      20
          //#define NUMSHELF   5  //总格数
          //int SHELF_LED_NUM=NUMPIXELS/NUMSHELF;//每格灯的个数
          //int SHELF[]={0,4,8,12,16};
         if(twinkle==1)    {flipper.attach(twinkle_frequency, turnoff_led);}
         if(twinkle==0)    flipper.detach();
          test_connect=test_connect+LED_LIGHT[led];  //灯的值
          test_connect_bin=test_connect_bin+int(COLOR_PIN[led][0])+int(COLOR_PIN[led][1])+int(COLOR_PIN[led][2]);  //灯的状态码  
       }
//          Serial.println(test_connect);
         Serial.print("test_connect_bin=");
        Serial.println(test_connect_bin);
       pixels.show(); // This sends the updated pixel color to the hardware.
          memset(LED_LIGHT,0,sizeof(LED_LIGHT));//归零
          memset(COLOR_PIN,0,sizeof(COLOR_PIN));
          test_connect="";
          test_connect_bin="";
          order_led="";
          order_all="";
         
}
void turnon_led(int led)//彩灯启动，控制16路彩灯，一个PCF端口从0-7
{
  //0 0-4 1 5-9 2 10-14
   for(int i=SHELF[led];i<SHELF[led]+SHELF_LED_NUM;i++){
    // pixels.Color takes RGB values, from 0,0,0 up to 255,255,255
    pixels.setPixelColor(i, pixels.Color( COLOR_PIN[led][0], COLOR_PIN[led][1], COLOR_PIN[led][2])); // Moderately bright green color.
   }
 
}
void turnoff_led()//关灯
{
 for(int i=0;i<NUMPIXELS;i++){
    // pixels.Color takes RGB values, from 0,0,0 up to 255,255,255
    pixels.setPixelColor(i, pixels.Color(0,0,0)); // Moderately bright green color.
    pixels.show(); // This sends the updated pixel color to the hardware.
   }
}
void led_state_connecting(){
    for(int i=0;i<NUMPIXELS;i++){
    // pixels.Color takes RGB values, from 0,0,0 up to 255,255,255
    pixels.setPixelColor(i, pixels.Color(255,0,0)); // Moderately bright green color.
    pixels.show(); // This sends the updated pixel color to the hardware.
    }
    delay(300);
    for(int i=0;i<NUMPIXELS;i++){
    // pixels.Color takes RGB values, from 0,0,0 up to 255,255,255
    pixels.setPixelColor(i, pixels.Color(0,0,0)); // Moderately bright green color.
    pixels.show(); // This sends the updated pixel color to the hardware.
    }
}
void led_state_connected(){
    for(int i=0;i<NUMPIXELS;i++){
        // pixels.Color takes RGB values, from 0,0,0 up to 255,255,255
        pixels.setPixelColor(i, pixels.Color(0,255,0)); // Moderately bright green color.
        pixels.show(); // This sends the updated pixel color to the hardware.
       }
    
    delay(2000);
    for(int i=0;i<NUMPIXELS;i++){
        // pixels.Color takes RGB values, from 0,0,0 up to 255,255,255
        pixels.setPixelColor(i, pixels.Color(0,0,0)); // Moderately bright green color.
        pixels.show(); // This sends the updated pixel color to the hardware.
       }
}
void wificb(int c) {
  Serial.print("=-=-=-=-");
  Serial.println(c);
}
void wifisetup() {
    Serial.println("Startup");
    
    DYWIFICONFIG_STRUCT defaultConfig =  wificonfig.createConfig();
    strcpy(defaultConfig.SSID,DEF_WIFI_SSID);
    strcpy(defaultConfig.SSID_PASSWORD,DEF_WIWI_PASSWORD);
    strcpy(defaultConfig.HOSTNAME, ap_name);
    strcpy(defaultConfig.APNAME,ap_name);
    wificonfig.setDefaultConfig(defaultConfig);
    wificonfig.begin(&webserver, "/");
    wificonfig.enableAP();
}
void wificonfig_clear(){
   storeconfig.clear();
   Serial.print("clear up");
}

