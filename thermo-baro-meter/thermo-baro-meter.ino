#include <Wire.h>
#include "MsTimer2.h"
#include "Ws2822s.h"

#define PIXEL_NUM 20    //使用するWS2822Sの数
#define LED_PIN 14      //WS2822SのDAIピンにつなげるArduinoのピン番号
#define BME280_ADDRESS 0x76  // BME280のI2Cアドレス

unsigned long int hum_raw, temp_raw, pres_raw;
signed long int t_fine;
unsigned long  runtime_next;
unsigned long  interval = 60 * 60 * 1000L;  // 60分×60秒×1000ms

// 変換用変数
uint16_t dig_T1;
int16_t dig_T2, dig_T3;
uint16_t dig_P1;
int16_t dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
int8_t  dig_H1;
int16_t dig_H2;
int8_t  dig_H3;
int16_t dig_H4;
int16_t dig_H5;
int8_t  dig_H6;

Ws2822s LED(LED_PIN , PIXEL_NUM);

int tempColor_R[] = {0, 6, 45, 80, 100, 110, 120, 125, 125, 125, 125}; // 温度 色変化テーブル(赤)
int tempColor_G[] = {0, 0, 0, 0, 0, 20, 45, 70, 95, 120, 125};         // 温度 色変化テーブル(緑)
int tempColor_B[] = {0, 60, 75, 75, 60, 5, 0, 0, 5, 60, 125};          // 温度 色変化テーブル(青)
int baroColor_R[] = {125, 100, 75, 50, 25, 0, 0, 0, 0, 0, 0};          // 気圧 色変化テーブル(赤)
int baroColor_G[] = {0, 0, 25, 50, 75, 100, 75, 50, 25, 0, 0};         // 気圧 色変化テーブル(緑)
int baroColor_B[] = {0, 0, 0, 0, 0, 0, 25, 50, 75, 100, 125};          // 気圧 色変化テーブル(青)
int tempMem_R[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};          // 温度用メモリ確保(赤)
int tempMem_G[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};          // 温度用メモリ確保(緑)
int tempMem_B[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};          // 温度用メモリ確保(青)
int baroMem_R[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};          // 気圧用メモリ確保(赤)
int baroMem_G[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};          // 気圧用メモリ確保(緑)
int baroMem_B[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};          // 気圧用メモリ確保(青)
int tempTable[] = {20, 30};                      // 温度範囲の指定
long baroTable[] = {950, 1050};                  // 気圧範囲の指定

// LEDに色をセットする関数
void write_color()
{
  LED.send();
}

void setup()
{
  uint8_t osrs_t = 1;             //Temperature oversampling x 1
  uint8_t osrs_p = 1;             //Pressure oversampling x 1
  uint8_t osrs_h = 1;             //Humidity oversampling x 1
  uint8_t mode = 3;               //Normal mode
  uint8_t t_sb = 5;               //Tstandby 1000ms
  uint8_t filter = 0;             //Filter off
  uint8_t spi3w_en = 0;           //3-wire SPI Disable

  uint8_t ctrl_meas_reg = (osrs_t << 5) | (osrs_p << 2) | mode;
  uint8_t config_reg    = (t_sb << 5) | (filter << 2) | spi3w_en;
  uint8_t ctrl_hum_reg  = osrs_h;

  //色配列の初期化
  for (int i = 0; i < PIXEL_NUM; i++) {
    LED.setColor(i, 0, 0, 0);
  }
  //色情報送信用タイマ設定
  MsTimer2::set(100, write_color);
  MsTimer2::start();

  Serial.begin(9600);
  Wire.begin();

  writeReg(0xF2, ctrl_hum_reg);
  writeReg(0xF4, ctrl_meas_reg);
  writeReg(0xF5, config_reg);
  readTrim();                   // Read trim data from BME280
  runtime_next = millis() + interval; // 次にLEDを更新する時間を決める
}


void loop()
{
  double temp_act = 0.0, press_act = 0.0, hum_act = 0.0; //最終的に表示される値を入れる変数
  signed long int temp_cal;
  unsigned long int press_cal, hum_cal;

  readData();

  temp_cal = calibration_T(temp_raw);
  press_cal = calibration_P(pres_raw);
  hum_cal = calibration_H(hum_raw);
  temp_act = (double)temp_cal / 100.0;
  press_act = (double)press_cal / 100.0;
  hum_act = (double)hum_cal / 1024.0;
  Serial.print("TEMP : ");
  Serial.print(temp_act);
  Serial.print(" DegC  PRESS : ");
  Serial.print(press_act);
  Serial.print(" hPa  HUM : ");
  Serial.print(hum_act);
  Serial.println(" %");
  // --------------------------------------
  // 気温をLEDバーの色に変換する
  // --------------------------------------

  // 一つ前のLEDの色データを次のLEDに送る (温度編)
  for (int i = 1; i <= 9; i++) {
    tempMem_R[i - 1] = tempMem_R[i];
    tempMem_G[i - 1] = tempMem_G[i];
    tempMem_B[i - 1] = tempMem_B[i];
  }

  // 設定した最低、最高温度を0-999とした時の、現在の温度を計算する
  int tt = (temp_act * 100 - tempTable[0] * 100) / (tempTable[1] * 100 - tempTable[0] * 100) * 1000;
  if (tt < 0) tt = 0;   // 低すぎるときは0に
  if (tt > 999) tt = 999;    // 高すぎるときは999に

  // 表示用メモリにRGB値を設定する
  // 足し算の前半でおおまかな色を決めて、足し算の後半で滑らかに色が変化するように細かい数字を足す
  tempMem_R[9] = tempColor_R[(int)tt / 100] + ((tempColor_R[(int)tt / 100 + 1] - tempColor_R[(int)tt / 100]) * (tt % 100)) / 100;
  tempMem_G[9] = tempColor_G[(int)tt / 100] + ((tempColor_G[(int)tt / 100 + 1] - tempColor_G[(int)tt / 100]) * (tt % 100)) / 100;
  tempMem_B[9] = tempColor_B[(int)tt / 100] + ((tempColor_B[(int)tt / 100 + 1] - tempColor_B[(int)tt / 100]) * (tt % 100)) / 100;

  // --------------------------------------
  // 気圧をLEDバーの色に変換する
  // --------------------------------------
  // 一つ前のLEDの色データを次のLEDに送る (気圧編)
  for (int i = 8; i >= 0; i--) {
    baroMem_R[i + 1] = baroMem_R[i];
    baroMem_G[i + 1] = baroMem_G[i];
    baroMem_B[i + 1] = baroMem_B[i];
  }

  // 設定した最低、最高気圧を0-999とした時の、現在の気圧を計算する
  int bt = (press_act * 100 - baroTable[0] * 100) / (baroTable[1] * 100 - baroTable[0] * 100) * 1000;
  if (bt < 0) bt = 0;   // 低すぎるときは0に
  if (bt > 999) bt = 999;    // 高すぎるときは999に

  // 表示用メモリにRGB値を設定する
  // 足し算の前半でおおまかな色を決めて、足し算の後半で滑らかに色が変化するように細かい数字を足す
  baroMem_R[0] = baroColor_R[(int)bt / 100] + ((baroColor_R[(int)bt / 100 + 1] - baroColor_R[(int)bt / 100]) * (bt % 100)) / 100;
  baroMem_G[0] = baroColor_G[(int)bt / 100] + ((baroColor_G[(int)bt / 100 + 1] - baroColor_G[(int)bt / 100]) * (bt % 100)) / 100;
  baroMem_B[0] = baroColor_B[(int)bt / 100] + ((baroColor_B[(int)bt / 100 + 1] - baroColor_B[(int)bt / 100]) * (bt % 100)) / 100;

  // LEDに色を設定する
  for (int i = 0; i < 10; i++) {
    LED.setColor(i, baroMem_R[i], baroMem_G[i], baroMem_B[i]);
    LED.setColor(i + 10, tempMem_R[i], tempMem_G[i], tempMem_B[i]);
  }

  while (runtime_next > millis()) {
    // 指定された時間まで待つ
  }
  runtime_next = millis() + interval;  // 次にLEDが変化する時間を設定
}

// --------------------------------------------------------------
// チップ固有の調整用データを読み出す
// --------------------------------------------------------------
void readTrim()
{
  uint8_t data[33], i = 0;
  Wire.beginTransmission(BME280_ADDRESS);
  Wire.write(0x88);
  Wire.endTransmission();
  Wire.requestFrom(BME280_ADDRESS, 24);
  while (Wire.available()) {
    data[i] = Wire.read();
    i++;
  }
  Wire.beginTransmission(BME280_ADDRESS);
  Wire.write(0xA1);
  Wire.endTransmission();
  Wire.requestFrom(BME280_ADDRESS, 1);
  data[i] = Wire.read();
  i++;

  Wire.beginTransmission(BME280_ADDRESS);
  Wire.write(0xE1);
  Wire.endTransmission();
  Wire.requestFrom(BME280_ADDRESS, 7);
  while (Wire.available()) {
    data[i] = Wire.read();
    i++;
  }
  dig_T1 = (data[1] << 8) | data[0];
  dig_T2 = (data[3] << 8) | data[2];
  dig_T3 = (data[5] << 8) | data[4];
  dig_P1 = (data[7] << 8) | data[6];
  dig_P2 = (data[9] << 8) | data[8];
  dig_P3 = (data[11] << 8) | data[10];
  dig_P4 = (data[13] << 8) | data[12];
  dig_P5 = (data[15] << 8) | data[14];
  dig_P6 = (data[17] << 8) | data[16];
  dig_P7 = (data[19] << 8) | data[18];
  dig_P8 = (data[21] << 8) | data[20];
  dig_P9 = (data[23] << 8) | data[22];
  dig_H1 = data[24];
  dig_H2 = (data[26] << 8) | data[25];
  dig_H3 = data[27];
  dig_H4 = (data[28] << 4) | (0x0F & data[29]);
  dig_H5 = (data[30] << 4) | ((data[29] >> 4) & 0x0F);
  dig_H6 = data[31];
}

// --------------------------------------------------------------
// BME280のレジスタに書き込む
// --------------------------------------------------------------
void writeReg(uint8_t reg_address, uint8_t data)
{
  Wire.beginTransmission(BME280_ADDRESS);
  Wire.write(reg_address);
  Wire.write(data);
  Wire.endTransmission();
}

// --------------------------------------------------------------
// BME280のデータを読み出す
// --------------------------------------------------------------
void readData()
{
  int i = 0;
  uint32_t data[8];
  Wire.beginTransmission(BME280_ADDRESS);
  Wire.write(0xF7);
  Wire.endTransmission();
  Wire.requestFrom(BME280_ADDRESS, 8);
  while (Wire.available()) {
    data[i] = Wire.read();
    i++;
  }
  pres_raw = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);
  temp_raw = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4);
  hum_raw  = (data[6] << 8) | data[7];
}

// --------------------------------------------------------------
// 温度を計算する
// --------------------------------------------------------------
signed long int calibration_T(signed long int adc_T)
{
  signed long int var1, var2, T;
  var1 = ((((adc_T >> 3) - ((signed long int)dig_T1 << 1))) * ((signed long int)dig_T2)) >> 11;
  var2 = (((((adc_T >> 4) - ((signed long int)dig_T1)) * ((adc_T >> 4) - ((signed long int)dig_T1))) >> 12) * ((signed long int)dig_T3)) >> 14;

  t_fine = var1 + var2;
  T = (t_fine * 5 + 128) >> 8;
  return T;
}

// --------------------------------------------------------------
// 気圧を計算する
// --------------------------------------------------------------
unsigned long int calibration_P(signed long int adc_P)
{
  signed long int var1, var2;
  unsigned long int P;
  var1 = (((signed long int)t_fine) >> 1) - (signed long int)64000;
  var2 = (((var1 >> 2) * (var1 >> 2)) >> 11) * ((signed long int)dig_P6);
  var2 = var2 + ((var1 * ((signed long int)dig_P5)) << 1);
  var2 = (var2 >> 2) + (((signed long int)dig_P4) << 16);
  var1 = (((dig_P3 * (((var1 >> 2) * (var1 >> 2)) >> 13)) >> 3) + ((((signed long int)dig_P2) * var1) >> 1)) >> 18;
  var1 = ((((32768 + var1)) * ((signed long int)dig_P1)) >> 15);
  if (var1 == 0)
  {
    return 0;
  }
  P = (((unsigned long int)(((signed long int)1048576) - adc_P) - (var2 >> 12))) * 3125;
  if (P < 0x80000000)
  {
    P = (P << 1) / ((unsigned long int) var1);
  }
  else
  {
    P = (P / (unsigned long int)var1) * 2;
  }
  var1 = (((signed long int)dig_P9) * ((signed long int)(((P >> 3) * (P >> 3)) >> 13))) >> 12;
  var2 = (((signed long int)(P >> 2)) * ((signed long int)dig_P8)) >> 13;
  P = (unsigned long int)((signed long int)P + ((var1 + var2 + dig_P7) >> 4));
  return P;
}

// --------------------------------------------------------------
// 湿度を計算する
// --------------------------------------------------------------
unsigned long int calibration_H(signed long int adc_H)
{
  signed long int v_x1;

  v_x1 = (t_fine - ((signed long int)76800));
  v_x1 = (((((adc_H << 14) - (((signed long int)dig_H4) << 20) - (((signed long int)dig_H5) * v_x1)) +
            ((signed long int)16384)) >> 15) * (((((((v_x1 * ((signed long int)dig_H6)) >> 10) *
                (((v_x1 * ((signed long int)dig_H3)) >> 11) + ((signed long int) 32768))) >> 10) + (( signed long int)2097152)) *
                ((signed long int) dig_H2) + 8192) >> 14));
  v_x1 = (v_x1 - (((((v_x1 >> 15) * (v_x1 >> 15)) >> 7) * ((signed long int)dig_H1)) >> 4));
  v_x1 = (v_x1 < 0 ? 0 : v_x1);
  v_x1 = (v_x1 > 419430400 ? 419430400 : v_x1);
  return (unsigned long int)(v_x1 >> 12);
}
