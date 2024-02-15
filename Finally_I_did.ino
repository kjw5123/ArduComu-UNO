#include "DHT.h" //DHT 라이브러리 호출
#include "ArduinoJson.h" //json 변환해주는 라이브러리 호출
#include "SoftwareSerial.h"
#include "time.h"
#include <Arduino.h>
#include <stdint.h>
#include <LiquidCrystal.h>

SoftwareSerial espSerial(2, 3);
DHT dht(10, DHT22); //DHT 설정(10,DHT22)1
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);
#define no_dust 0.60 //최초 전원 인가 후 깨끗한 환경에서의 지속적으로 측정되는 전압값 넣어주세요 

String WIFI_SSID = "KT_GiGA_2G_Wave2_CEF7";
String PASSWORD = "ad77cc7290";

unsigned long modifiedTime = 0;
unsigned long previousModifiedTime = 0;
unsigned long startCalibration = 0;
unsigned long currentMillis = 0;
int dustout = A4; // 센서의 5번핀 먼지를 측정 아두이노 A4번에 연결

int v_led = A5; // 센서의 3번핀을 아두이노 A5번에 연결  센서 내부 적외선 led를 on/off 제어
int s_ledr = 13; // 공기 상태에 나쁨 RED
int s_ledg = 12; // 공기 상태에 보통 Green
int s_ledb = 11; // 공기 상태에 좋음 Blue
float vo_value = 0; // 센서로 읽은 값 변수 선언
float sensor_voltage = 0; // 센서로 읽은 값을 전압으로 측정한 값
float dust_density = 0; // 실제 미세먼지 밀도

String timeValue;
char jsonTime[9];
String jsonTimeStr;
String MinSec;
String Sec;

unsigned long second;
unsigned long minute;
unsigned long hour;
int millisecond;

int firstColonIdx;
int secondColonIdx;
int pointIdx;
int currentHour;
int currentMinute;
int currentSecond;
int currentMilliSecond;

void setup() {

  Serial.begin(9600);
  espSerial.begin(9600);
  dht.begin();
  pinMode(v_led,OUTPUT);
  digitalWrite(v_led,LOW);

  /*GET 요청을 통해 서버 시간 받아오기*/
  sendCommandToESP8266("AT", 5, "OK");
  sendCommandToESP8266("AT+CWMODE=1", 5, "OK");
  sendCommandToESP8266("AT+CWJAP=\"" + WIFI_SSID + "\",\"" + PASSWORD + "\"", 20, "OK");
  sendCommandToESP8266("AT+CIPMUX=1", 5, "OK");

  String TimeServerConnect = "AT+CIPSTART=0,\"TCP\",\"27.96.130.233\",8080";
  sendCommandToESP8266(TimeServerConnect, 5, "CONNECT");

  String toGetTime = "AT+CIPSEND=0,12";
  sendCommandToESP8266(toGetTime, 5, ">");

  String TimeOrder = "GET /ttime";
  sendCommandToESP8266(TimeOrder, 5, "IPD");
  
  String GETdata = "";
  startCalibration = millis();
  if (espSerial.available()){
    if(espSerial.find('T')){
      GETdata = espSerial.readStringUntil('\"');
    }
  }
  espSerial.flush();
  timeValue = GETdata.substring(0,12);
}

int loopCnt;

void loop()
{
  loopCnt += 1;
  /* DHT 관련*/
  digitalWrite(v_led,LOW); 
  delayMicroseconds(280); // 280us동안 딜레이
  vo_value=analogRead(dustout); // 데이터를 읽음
  delayMicroseconds(40); // 320us - 280us
  digitalWrite(v_led,HIGH); // 적외선 LED OFF
  delayMicroseconds(9680); // 10us(주기) -320us(펄스폭) 한 값
  sensor_voltage = get_voltage(vo_value); // get_voltage 함수 실행
  dust_density = get_dust_density(sensor_voltage); // get_dust_density 함수 실행
  int dust_density1 = dust_density;
  float dust_density2 = dust_density1;

  double h = dht.readHumidity(); //습도값을 h에 저장
  double t = dht.readTemperature(); //온도값을 t에 저장
  double f = dht.readTemperature(true); // 화씨 온도를 측정합니다.
  double hif = dht.computeHeatIndex(f, h);
  double hic = dht.computeHeatIndex(t, h, false);
  int hic1 = hic;
  int h1 = h;

  /* 실행 시간 계산 관련 */
  currentMillis = millis();
  modifiedTime = currentMillis - startCalibration;
  modifiedTime += 10*loopCnt; //대략 매 loop마다 10ms 정도의 오차 발생하는 것을 감안함.
  Serial.println("Modified Time is " + String(modifiedTime - previousModifiedTime));
  previousModifiedTime = modifiedTime;

  millisecond = modifiedTime % 1000;
  second = (modifiedTime / 1000) % 60;
  minute = ((modifiedTime / 1000) % 3600) / 60;
  hour = ((modifiedTime / 1000) / 3600) % 24;
  

  /* 시간 문자열 정수 변환 */
  firstColonIdx = timeValue.indexOf(':');
  currentHour = timeValue.substring(0, firstColonIdx).toInt() + hour;

  MinSec = timeValue.substring(firstColonIdx+1);
  secondColonIdx = MinSec.indexOf(':');
  currentMinute = MinSec.substring(0, secondColonIdx).toInt() + minute;

  Sec = MinSec.substring(secondColonIdx+1);
  pointIdx = Sec.indexOf('.');
  currentSecond = Sec.substring(0, pointIdx).toInt() + second;

  currentMilliSecond = Sec.substring(pointIdx + 1).toInt() + millisecond;

  /* 시간 값 60진법 적용 */
  if(currentMilliSecond >= 1000){
    currentSecond += 1;
    currentMilliSecond -= 1000;
  }
  if(currentSecond >= 60){
    currentMinute += 1;
    currentSecond -= 60;
  }
  if(currentMinute >= 60){
    currentHour += 1;
    currentMinute -= 60;
  }
  if(currentHour >= 24){
    currentHour -= 24;
  }

  Serial.println("Calibrated Time is " + String(currentHour) + ":" + String(currentMinute) + ":" + String(currentSecond) + "." + String(currentMilliSecond));
  // 00:00:00 형식을 유지하기 위해, sprintf로 자릿 수를 지정하여 값을 넣어준다.
  sprintf(jsonTime,"%02d:%02d:%02d",currentHour, currentMinute, currentSecond);
  //jsonTime의 자료형이 char형이라 String끼리 이어붙이기가 안되기에, String으로 변환해준다.
  jsonTimeStr = jsonTime; 

  String jsonData = "{\"time\":\"" + jsonTimeStr + "\",\"temp\":" + String(t, 2) + ",\"humi\":" + String(h, 2) + ",\"dust\":" + String(dust_density, 2) + "}";
  String order = "POST /data HTTP/1.1\r\nHost: 27.96.130.233\r\nContent-Type: application/json\r\nContent-Length: " + String(jsonData.length()) + "\r\n" + "\r\n" + jsonData;
  String Connect = "AT+CIPSTART=0,\"TCP\",\"27.96.130.233\",8080";
  sendCommandToESP8266(Connect, 5, "CONNECT");
  
  /* 데이터 서버로 POST */
  String cipSend = "AT+CIPSEND=0," + String(order.length());
  sendCommandToESP8266(cipSend, 5, ">");
  postHTTP(jsonData);

  delay(10000);
}

/*서버에 json을 보내기 위해 ESP01에 입력해야
하는 Command들을 한번에 묶어버린 함수이다.*/
void postHTTP(String jsonData) {
  espSerial.println("POST /data HTTP/1.1");
  espSerial.println("Host: 27.96.130.233");
  espSerial.println("Content-Type: application/json");
  espSerial.println("Content-Length: " + String(jsonData.length()));
  espSerial.println();
  espSerial.println(jsonData);
}


float get_voltage(float value) {
  float V = value * 5.0 / 1024; // 아날로그값을 전압값으로 바꿈
  return V;
}


float get_dust_density(float voltage) {
  float dust = (voltage - no_dust) / 0.005; // 데이터 시트에 있는 미세먼지 농도(ug) 공식 기준
  return dust;
} 

/*하단의 sendCommandToESP8266함수가 성공적으로
실행됐을 때 숫자가 증가하는 글머리이다.*/
int countTrueCommand;
/*원하는 수신 값을 받기 위해 Command를
송신한 횟수이다.*/  
int countTimeCommand;
boolean found = false;

/* readReplay에 해당하는 문자열이 수신될 때 까지, 최대 maxTime만큼 반복하여
원하는 Command를 ESP에 송신하는 함수이다. 성공 여부에 따라 Success 혹은
Fail을 출력한다.*/
void sendCommandToESP8266(String command, int maxTime, char readReplay[]) {
  Serial.print(countTrueCommand);
  Serial.print(". at command => ");
  Serial.print(command);
  Serial.print(" ");
  
  /*정해진 maxTime 횟수만큼 지정된 답변이
  수신될 때까지 반복하여 Command를 보내는
  반복문이다. 수신이 제대로 되지 않을 경우,
  시도 횟수가 증가하게 되고, maxTime에
  다다르게 되면 Fail을 출력하게 되고 함수가종료된다.*/
  while (countTimeCommand < (maxTime * 1)) { 
    espSerial.println(command);
    if (espSerial.find(readReplay)) {
      found = true;
      break;
    }
    countTimeCommand++;
  }

  if (found == true) {
    Serial.println("Success");
    countTrueCommand++;
    countTimeCommand = 0;
  }

  if (found == false) {
    Serial.println("Fail");
    countTrueCommand = 0;
    countTimeCommand = 0;
  }
  found = false;
}