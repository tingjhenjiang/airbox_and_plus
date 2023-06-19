/*
    Demonstrates the use a 16x2 LCD display and 4x4 LCD display.
    PM5003T and DS3231(CR2031)
    The Arduino circuit connection for LCD:
    * LCD RS  pin to analog pin A0
    * LCD  Enable pin to analog pin A1
    * LCD D4  pin to analog pin A2
    * LCD D5  pin to analog pin A3
    * LCD D6  pin to analog pin A4
    * LCD D7  pin to analog pin A5
    The Arduino circuit connection for MAtrix Key Pad:
    * ROW1 pin  to digital pin 12
    * ROW2 pin  to digital pin 11
    * ROW3 pin  to digital pin 10
    * ROW4 pin  to digital pin 9
    * COLUMN1  pin to digital pin 8
    * COLUMN2  pin to digital pin 7
    * COLUMN3  pin to digital pin 6
    * COLUMN4  pin to digital pin 5
    Name:-  yhtseng
    Date:-   2018/4/2
    Version:-  V0.9
    e-mail:-  yhtseng@gmail.com
*/

// 加入標頭檔
#include "DS3231.h"
#include "LiquidCrystal_I2C.h" //https://forum.arduino.cc/t/i-get-compile-error-on-the-line-lcd-begin/626195
#include "Keypad.h"
#include "MQUnifiedsensor.h"
#include <SoftwareSerial.h> // PM2.5 for PMS5003T

// 鍵盤行列數
const byte ROWS = 4; // 4行
const byte COLS = 4; // 4列

// 鍵盤鍵值映射
char hexaKeys[ROWS][COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

// 鍵盤針腳定義
byte rowPins[ROWS] = {12,11,10,9}; // 行偵測
byte colPins[COLS] = {8, 7, 6, 5}; // 列偵測

// 定義鍵盤
Keypad yhtKeypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

// 定義PM5003T
SoftwareSerial PM5003T(3, 4); // RX, TX

// 定義LCD
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

// 定義計時模組
DS3231 Clock;

// 定義偵測瓦斯氣體模組 MQ-7
MQUnifiedsensor MQ7("Arduino Nano",5,10,A3,"MQ-7");

#define pmsDataLen 32
// 宣告要使用的全域變數
uint8_t buf[pmsDataLen]; // 長度32的無號8位整數陣列

long pm10;
long pm25;
long pm100;
long temp;
long humi;
bool Century = false;
bool h12;
bool PM;
int idx;
float o3;
float ratio;
uint8_t c;
// uint8_t temperature;
float COppm;
char presentkey;
char previouskey = '2';

void setup() {
    // set up the LCD's number of columns and rows:
    lcd.begin(16, 2);  // 初始化 LCD，一行 16 個字元，共 2 行

    // Print time and a message to the LCD.
    ReadDS3231();                  // 從DS3231感測器取得資訊
    lcd.setCursor(0, 1);           // 設定游標位置在第二行行首
    lcd.print("Type 0 for help."); // 輸入0以獲得使用說明

    // MQ-7 prepare
    MQ7.setRegressionMethod(1); //_PPM = a*ratio^b
    MQ7.setA(99.042);
    MQ7.setB(-1.518); //Benzene
    MQ7.init();
    Serial.print("MQ-7 Calibrating please wait.");
    float calcR0 = 0;
    for(int i = 1; i<=10; i ++)
    {
        MQ7.update(); // Update data, the arduino will be read the voltage on the analog pin
        calcR0 += MQ7.calibrate(27.5);
        Serial.print(".");
    }
    MQ7.setR0(calcR0/10);
    Serial.println(" done!.");
    // if(isinf(calcR0)) {Serial.println("Warning: Conection issue founded, R0 is infite (Open circuit detected) please check your wiring and supply"); while(1);}
    // if(calcR0 == 0){Serial.println("Warning: Conection issue founded, R0 is zero (Analog pin with short circuit to ground) please check your wiring and supply"); while(1);}
    // MQ7.serialDebug(true);

    // Serial相關
    PM5003T.begin(9600); // PMS5003T偵測器使用UART(鮑率9600)
    Serial.begin(9600);  // 開始Serial port(鮑率9600)
} // 初始化結束

void loop() {
    delay(1500);    // 延遲1.5秒

    // 取得按鍵的值
    presentkey = yhtKeypad.getKey();
    if (presentkey == '\0') {
        presentkey = previouskey;
    }
    else {
        if (presentkey != '0') {
            previouskey = presentkey;
        }
    }
    Serial.println(presentkey);

    // 顯示時間
    ReadDS3231();          // 從DS3231感測器取得資訊
    lcd.setCursor(0, 1);   // 設定游標位置在第二行行首

    // 顯示PM1.0
    if (presentkey == '1') {     // 如果按鍵1被按下
        getPM25();               // 取得感測器數值
        clear(1);                // 清除LCD顯示
        lcd.print("PM1.0=");    // LCD印出"PM1.0= "
        lcd.print(pm10);         // LCD印出變數數值
        lcd.setCursor(11, 1);    // 設定游標位置在第二行第11位置
        lcd.print("ug/m3");      // LCD印出"ug/m3"
    }
    
    // 顯示PM2.5
    else if (presentkey == '2') {
        getPM25();              // 從感測器讀取數值
        clear(1);               // 清除LCD顯示
        lcd.print("PM2.5=");   // LCD印出"PM2.5= "
        lcd.print(pm25);        // LCD印出PM2.5數值
        lcd.setCursor(11, 1);   // 設定游標位置在第二行行首第11字
        lcd.print("ug/m3/std@25");     // LCD印出"ug/m3"
    }
    // 顯示PM10
    else if (presentkey == '3') {  // 如果按鍵3被按下
        getPM25();                 // 取得感測器數值
        clear(1);                  // 清除LCD顯示
        lcd.print("PM10.=");      // LCD印出"PM10.= "
        lcd.print(pm100);          // LCD印出PM10.數值
        lcd.setCursor(11, 1);      // 設定游標位置在第二行第11位置
        lcd.print("ug/m3/std@50");        // LCD印出"ug/m3"
    }
    // 顯示CO含量
    else if (presentkey == '4') {               // 如果按鍵4被按下
        getCO();
        clear(1);
        lcd.print("CO=");
        lcd.print(COppm);
        // lcd.setCursor(5, 1);
        lcd.print(" ppm/std@25");
    }
    // 顯示空氣粒子濃度感測器的溫度
    else if (presentkey == '5') {    // 如果按鍵5被按下
        getPM25();                   // 從感測器讀取數值
        lcd.print("Temperature=");   // LCD印出"Temperature="
        if (temp < 10) {             // 格式化輸出
            lcd.print(' ');
        }
        lcd.print(temp);             // 印出溫度數值
        lcd.print((char)0xDF);       // 印出度的符號
        lcd.print('C');              // 印出C
    }
    // 顯示濕度
    else if (presentkey == '6') {  // 如果按鍵6被按下
        getPM25();                 // 從感測器讀取數值
        clear(1);                  // 清除LCD顯示
        lcd.print("Humidity=");  // LCD印出"Humidity=  "
        lcd.print(humi);           // 印出濕度數值
        lcd.setCursor(15, 1);      // 設定游標位置在第二行第15位置
        lcd.print('%');            // 印出"%"
    }
    else if (presentkey == '7') {          // 如果按鍵7被按下
        lcd.print("Good Morning !"); // LCD印出"Good Morning !"
        
    }
    else if (presentkey == '8') {      // 如果按鍵8被按下
        lcd.print("Good Afternoon !"); // LCD印出"Good Afternoon !"
    }
    else if (presentkey == '9') {      // 如果按鍵9被按下
        lcd.print("Good Night !    "); // LCD印出"Good Night !    "
    }
    // 按鍵說明
    else if (presentkey == '0') {
        lcd.print("Key 1 = PM1.0   "); // 輸出訊息
        delay(1500);                   // 延遲1.5秒
        lcd.setCursor(0, 1);           // 設定游標位置在第二行行首
        lcd.print("Key 2 = PM2.5   "); // 輸出訊息
        delay(1500);                   // 延遲1.5秒
        lcd.setCursor(0, 1);           // 設定游標位置在第二行行首
        lcd.print("Key 3 = PM10    "); // 輸出訊息
        delay(1500);                   // 延遲1.5秒
        lcd.setCursor(0, 1);           // 設定游標位置在第二行行首
        lcd.print("Key 4 = CO    "); // 輸出訊息
        delay(1500);                   // 延遲1.5秒
        lcd.setCursor(0, 1);           // 設定游標位置在第二行行首
        lcd.print("Key 5 = Temp    "); // 輸出訊息
        delay(1500);                   // 延遲1.5秒
        lcd.setCursor(0, 1);           // 設定游標位置在第二行行首
        lcd.print("Key 6 = Humidity"); // 輸出訊息
    }

    if (pm25>25) {
        delay(1500);
        clear(1);
        lcd.print("danger!PM2.5=");
        lcd.print(pm25);
    }
    if (pm100>50) {
        delay(1500);
        clear(1);
        lcd.print("danger!PM10=");
        lcd.print(pm100);
    }
    if (COppm>25) {
        delay(1500);
        clear(1);
        lcd.print("danger!CO=");
        lcd.print(COppm);
    }
} // end loop

// 讀取年月日時分並顯示在LCD
void ReadDS3231() {
    // 宣告要使用的變數
    byte minute, hour, date, month, year;
    // 從感測器讀取數值
    minute = Clock.getMinute();
    hour = Clock.getHour(h12, PM);
    date = Clock.getDate();
    month = Clock.getMonth(Century);
    year = Clock.getYear();

    lcd.setCursor(0, 0);  // 設定游標位置在第一行行首

    // 顯示時間、格式化輸出
    lcd.print("20");
    lcd.print(year, DEC);
    lcd.print('-');
    if (month < 10) {
        lcd.print('0');
    }
    lcd.print(month, DEC);
    lcd.print('-');
    if (date < 10) {
        lcd.print('0');
    }
    lcd.print(date, DEC);
    lcd.print(' ');
    if (hour < 10) {
        lcd.print('0');
    }
    lcd.print(hour, DEC);
    lcd.print(':');
    if (minute < 10) {
        lcd.print('0');
    }
    lcd.print(minute, DEC);
}

// 取得溫度、濕度及空氣粒子濃度並顯示在序列埠監控視窗
void getPM25() {
    // 初始化數值
    idx = 0;
    c = 0;
    memset(buf, 0, pmsDataLen); // 將buf的數值都設為0

    while (true) {
        // 持續讀取直到c不等於0x42
        while (c != 0x42) {
            while (!PM5003T.available());
            c = PM5003T.read();
        }
        while (!PM5003T.available());
        c = PM5003T.read();
        // 如果c等於0x4d就跳出
        if (c == 0x4d) {
            // now we got a correct header
            buf[idx++] = 0x42;
            buf[idx++] = 0x4d;
            break;
        }
    }

    // 將buf填滿
    while (idx != pmsDataLen) {
        while (!PM5003T.available());
        buf[idx++] = PM5003T.read();
    }
    // 變數處理
    pm10 = ( buf[10] << 8 ) | buf[11];
    pm25 = ( buf[12] << 8 ) | buf[13];
    pm100 = ( buf[14] << 8 ) | buf[15];
    temp = ( buf[24] << 8 ) | buf[25];
    temp = temp / 10;
    humi = ( buf[26] << 8 ) | buf[27];
    humi = humi / 10;
    // 輸出至Serial
    Serial.print("pm1.0: ");
    Serial.print(pm10);
    Serial.println(" ug/m3");
    Serial.print("pm2.5: ");
    Serial.print(pm25);
    Serial.println(" ug/m3");
    Serial.print("pm10: ");
    Serial.print(pm100);
    Serial.println(" ug/m3");
    Serial.print("Temperature: ");
    Serial.print(temp);
    Serial.println(" C");
    Serial.print("Humidity: ");
    Serial.print(humi);
    Serial.println(" %");
}

void getCO() {
    MQ7.update(); // Update data, the arduino will be read the voltage on the analog pin
    COppm=MQ7.readSensor(); // Sensor will read PPM concentration using the model and a and b values setted before or in the setup
    Serial.print("CO= ");
    Serial.print(COppm);
    Serial.println(" ppm");
    delay(500); //Sampling frequency
}

// 設定當前環境對應的修正數值
void setEnvCorrectRatio() {
    if (humi > 75) {
        ratio = 1.5623 - 0.0141 * temp; // 濕度大於75
    }
    else if (humi >= 50) {
        ratio = 1.3261 - 0.0119 * temp; // 濕度小於75大於等於50
    }
    else {
        ratio = 1.1507 - 0.0103 * temp; // 濕度50以下
    }
    ratio = analogRead(A0) * ratio / 1917.22; // 取得臭氧原始讀值並套用修正
}

// 清除LCD顯示
void clear(uint8_t row) {
    lcd.setCursor(0, row);
    lcd.print("                ");
    lcd.setCursor(0, row);
}
