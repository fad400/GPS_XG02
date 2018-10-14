// 17/03/11 R3+【NEO_6MGPSシールド】　XG02
// ＳＤカード　初期化失敗時　0.2秒のLED点滅で警告　動作ストップ
// ＧＰＳ時計からUTC->JST変換　03/11　04:04
//  GPS_XG02_test_01 ver:end 03/11 09:10
//  GPS_XG02_test_02 ver:10進変換搭載　F-SW時のタイム表示消えるのは仕様って事で 03/11 22:20
//  GPS_XG02_test_03 ver:RMC_UTCもJST
// GPSシールド用　pin8
// I assume you know how to connect the DS1302.
// NEO-6M:  (TX pin ->  Arduino RX) -> Arduino Digital 4
//          (RX pin ->  Arduino TX) -> Arduino Digital 5

// SD:      CS pin    -> Arduino Digital 8
//          SCK pin   -> Arduino Digital 13
//          MOSI pin  -> Arduino Digital 11
//          MISO pin  -> Arduino Digital 12
//          VCC pin   -> +5
//          GND       -> GND
//  LED           -> Arduino Digital 3
//  SW            -> Arduino Digital 2
//#include <TinyGPS++.h>
#include <TimeLib.h>
#include <SoftwareSerial.h>
//#include <ctype.h>
//#include <Time.h>

#include <SD.h>
#define FLAG_FILE    1 //0000 0001
#define FLAG_LED3    2 //0000 0010
#define FLAG_SW1     4 //0000 0100
#define FLAG_SW2     8 //0000 1000
#define FLAG_A1     16 //0001 0000
#define FLAG_A2     32 //0010 0000


//■■■■下記必須変数
SoftwareSerial g_gps(4,5);// RX, TX
const int chipSelect = 8; //SDカード　シールド CS-10 GPSシールド　CS-8
File myFile;//ファイルポインタ
char filename[13];
int swin1,swin2;//スイッチ　ｏｎ－ｏｆｆでファイル制御
int files_sw1=0;
int all_sw=0;
  char nmea[140];    //メインテーブル
  int comma[30]; // カンマカウント
  int ck =0;//
  int k; 
  int LED3_SW;       //

//■■■■時間
unsigned long LED3_TIME;//LED動作カウンタ　動作含める
unsigned long LED3_INVAL = 500;//0.5秒おきに送信
unsigned long RECON_TIME;//FILE動作カウンタ　動作含める
unsigned long GET_TIME;//FILE動作カウンタ　動作含める
unsigned long FILE_TIME;//FILE動作カウンタ　動作含める
unsigned long FILE_INVAL = 60000;//60000=1分　120000=2分　180000=3分

tmElements_t tm;
//■■■■予備変数
  int n_seti; //　緯度 北緯　26-35-45
  long n_setg;//　緯度 北緯　.xxxxxxxx
  int e_seti; //  経度 東経 126-139-145
  long e_setg;//  経度 東経 .xxxxxxxx
//■■■■■■■■■■■■■■■■■■■■
//    setup()
//■■■■■■■■■■■■■■■■■■■■ 
void setup()
{
  Serial.begin(115200);   //ＵＳＢ速度 
  g_gps.begin(9600);      //ＧＰＳユニット
  pinMode(2, INPUT) ;     //スイッチに接続ピンをデジタル入力に設定
  pinMode(3, OUTPUT);     // LED用
  digitalWrite(3, LOW);   //■■■■動作時ｌｅｄ　ｏｆｆ

 //■■■■ファイル処理
  files_sw1 = 0;  //defでファイルスイッチＯＮ
 
  //■■■■バージョン表記
  Serial.println("GPS_XG02_test_03");

  //■■■■ＳＤカード
  if (!SD.begin(chipSelect)){ //CSピン番号を指定しない場合のデフォルトはSSピン(Unoの場合は10番ピン)です
      // カードの初期化に失敗したか、またはＳＤが入っていない
      Serial.println("SDcard init NG");
      while (1) {
        digitalWrite(3, HIGH);   // off時 LED on
        delay (200); // 2017/3/11 500->200に変更
        digitalWrite(3, LOW);   // ｌｅｄ　ｏｆｆ
        delay (200); // 2017/3/11 500->200に変更
      }
      //return;
    }
    else{
      Serial.println("SDcard init OK");
    }
    //■■■■ＳＤカード end
 
    for(k=0;k<30;k++){
        comma[k]=0;  
    }
    swin2 = swin1 = 0;
  
}
//■■■■■■■■■■■■■■■■■■■■
//    LOOP
//■■■■■■■■■■■■■■■■■■■■ 
void loop()
{
   ////////////////////////////////////////////////// 
   //    入力
   ////////////////////////////////////////////////// 
   swin2 = swin1;//状態保存
   if (digitalRead(2) == LOW ) { //スイッチの状態を調べる　プルアップ抵抗 low時キーＯＮ
    swin1 = 1;//delay (20);
   }
   else {
    swin1 = 0;//delay (2);
   }

   ////////////////////////////////////////////////// 
   //    判定 
   //////////////////////////////////////////////////   
   if (swin1 == 1 && swin2 == 0) { //■立ち上げ時 ON
      Serial.println("---  FILE record sw ON    ---");
      GET_TIME = LED3_TIME = millis();     //■■■■ 時間ゲット
      FILE_TIME = GET_TIME + FILE_INVAL;  //インターバルタイマ
      LED3_TIME = LED3_TIME+LED3_INVAL;   //
      LED3_SW =1;digitalWrite(3, HIGH);   //  LED on

      int breakflag=0;//while文脱出フラグ

      while(1){//●
      led3_sw_flas();//点滅割り込み
      one_line_read();//1行読んで
      if(nmea[3]=='R' && nmea[4]=='M'&& nmea[5] =='C'){//判定 RMCだったら
         k=gps_nmea_rcm();                            //データチェック
         if(k==0){                                    //エラー無しデータまで読み出し
            rmc_dateTime();                           //RMCからtmへ時刻転送
            UTC_DateTimeConv(9);                      //tmからJST(+9)処理1～+23まで
            SdFile::dateTimeCallback(&dateTime);    // 日付と時刻を返す関数を登録
            filecop();                                //ファイル名生成+ファイルop
            myFile.println(nmea);                     //最初の1行書き込み　他のデータは省略　不関数でメモリ対策
            breakflag=1;                              //終了フラグ
            break;
         }
          if(breakflag) break;
        }
      if(breakflag) break;

      if (digitalRead(2) == HIGH ) { //スイッチの状態を調べる HI時キーOFF ＯＮ=>OFF時測位中キャンセル
        swin1 = 0;digitalWrite(3, LOW);
        break;
      } 
  }//●while終了
   RECON_TIME = millis();     //■■■■ 時間ゲット
   Serial.println("---  FILE record start    ---");
   } //■立ち上げ時 ON 終了
    /////////////////////////////////////////// 
    if (swin1 == 0 && swin2 == 1) {//■立下りｏｆｆ
      Serial.println("--- FILE Recording end  ---");
      fileccl();//ファイルクローズ
    }//■ 
    
   ////////////////////////////////////////////////// 
   //    メイン
   ////////////////////////////////////////////////// 


    GET_TIME = millis();
    one_line_read();

}

//■■■■■■■■■■■■■■■■■■■■
//　　　　　　ファイル名生成
//■■■■■■■■■■■■■■■■■■■■
void filenamemake_JST() 
{
  //GPSデータＵＴＣなので日本時間　UTC_DateTimeConv()直後に呼ぶ事
String cy =  String((tm.Year+1970)-2000);    
//Serial.println(cy);
String cm =  String(tm.Month);    
//Serial.println(cm);
String cd =  String(tm.Day);    
//Serial.println(cd);
String ch =  String(tm.Hour);    
//Serial.println(ch);
  filename[0] = '2' ;
  filename[1] = '0' ;
  filename[2] = cy[0];//年
  filename[3] = cy[1];
  //月
  if(tm.Month>=10){filename[4] = cm[0];filename[5] = cm[1];}
    else{ filename[4] = '0';filename[5] = cm[0];}
  //日
  if(tm.Day>=10){filename[6] = cd[0];filename[7] = cd[1];}
    else {filename[6] = '0';filename[7] = cd[0];}
  filename[8] = '.' ;
  filename[9] = 'l' ;
  filename[10] = 'o' ;
  filename[11] = 'g' ;
  filename[12] = '\0' ;
//  Serial.print("filename");  Serial.println(filename);

}
//■■■■■■■■■■■■■■■■■■■■
//    NMEAデータ　チェック1　データなし時
//■■■■■■■■■■■■■■■■■■■■
int NMEA_data_chk1(int a) 
{

  int w;
  w=a+1;
/*Serial.println(a);
  Serial.println(nmea[w]);
*/
  if(nmea[w] == ','){
    return 1;//データ無し
  }

  if(nmea[w] == '9'){  
    return 1;//
  }
  else{ 
    return 0;
  }

}
//■■■■■■■■■■■■■■■■■■■■
//     GPS RMC 日付データ　チェック
//■■■■■■■■■■■■■■■■■■■■
int gps_nmea_rcm() {
  int w;
        int p=0;
        ////日月年UTC　, [8]-[9]
        w = comma[0];
        if(NMEA_data_chk1(w) == 1) {
          //Serial.println("ERR1");
          p++;}//エラーがあれば+1
        w = comma[8];
        if(NMEA_data_chk1(w) == 1) {
          //Serial.println("ERR2");
          p++;}//エラーがあれば

        return p;

}
//■■■■■■■■■■■■■■■■■■■■
//      ファイルオープン処理
//■■■■■■■■■■■■■■■■■■■■
void filecop()
{
      files_sw1 = 1;//ファイルスイッチＯＮ
      filenamemake_JST();//ファイル名生成
      myFile = SD.open(filename, FILE_WRITE);
      delay (200);
      digitalWrite(3, HIGH);   //  LED on
      Serial.println("File.open");
}
//■■■■■■■■■■■■■■■■■■■■
//      ファイルクローズ処理
//■■■■■■■■■■■■■■■■■■■■
void fileccl()
{
      files_sw1 = 0;
      //ファイルクローズ処理
      myFile.close();
      delay (200);
      digitalWrite(3, LOW);   //  LED off　
      Serial.println("File.close");
}
//■■■■■■■■■■■■■■■■■■■■
//      タイムクローズ＆上書きオープン
//■■■■■■■■■■■■■■■■■■■■
void interval_file_oc()
{
      myFile.close();
      myFile = SD.open(filename, FILE_WRITE);
      delay (80);
      Serial.println("File.close&open");
}


//■■■■■■■■■■■■■■■■■■■■
//      スイッチ点滅
//■■■■■■■■■■■■■■■■■■■■
void  led3_sw_flas()
{
  if(millis() > LED3_TIME){
          if(LED3_SW ==1){
            LED3_SW=0;digitalWrite(3,LOW);
          }
          else {
   
          LED3_SW=1;digitalWrite(3,HIGH);
          }
    LED3_TIME = millis();     //■■■■ 時間ゲット
    LED3_TIME = LED3_TIME+LED3_INVAL; 
  }
}
/*
//■■■■■■■■■■■■■■■■■■■■
//     GPS　データリード　(1文字)
//■■■■■■■■■■■■■■■■■■■■
void g_gps.read(){
  while(1){
    nmea[v] = g_gps.read();//■gpsデータ読み出し
    if( -1 == nmea[v] || 0x0a == nmea[v])continue;   //-1の時は読み飛ばし  
    //if( -1 == nmea[v] )continue;   //-1の時は読み飛ばし  
    else{ 
      //Serial.print(nmea[v]);
      break;
    }
  }
}
*/
//■■■■■■■■■■■■■■■■■■■■
//     1行読み込み
//■■■■■■■■■■■■■■■■■■■■
int one_line_read()
{
  int breakflag=0;  //
  int v=0;          //行カウンタリセット
  int ck=0;   //comma[k]位置最大　リセット
  unsigned long  s;
  while(1){//●
    
    //以下 GPS　データリード　(1文字)関数べた書き■■■■
    while(1){
      nmea[v] = g_gps.read();//■gpsデータ読み出し
      if( -1 == nmea[v] || 0x0a == nmea[v])continue;   //-1の時は読み飛ばし  
       //if( -1 == nmea[v] )continue;   //-1の時は読み飛ばし  
      else{ 
        //Serial.print(nmea[v]);
      break;
      }
    } 
   //GPS　データリード　(1文字)関数べた書き終了■■■■
    if(',' == nmea[v]){comma[ck]=v;ck++;}  //カンマカウント
    //■■■■■■■■■■■■■■■■■■■■
    //     
    //■■■■■■■■■■■■■■■■■■■■ 
    if( 0x0d == nmea[v])//改行
    {
      nmea[v]='\0'; //端末処理

if(files_sw1 == 1 && ( nmea[3] == 'R' || nmea[4] == 'G')){  //sw1で書き込み   GPRMC + GPGGAの書き込み
// if(files_sw1 == 1 &&  nmea[3] == 'R'){  //sw1で書き込み   GPRMC の書き込み

        GET_TIME = GET_TIME -  RECON_TIME;
        myFile.print(nmea);//■■■■■ 書き込み処理
        myFile.print("[");
        s = GET_TIME/3600000;
        if(s<10) myFile.print("0");
        myFile.print(s);
        myFile.print(":");
        s = (GET_TIME/60000)%60;
        if(s<10) myFile.print("0");
        myFile.print(s);
        myFile.print(":");
        s =(GET_TIME/1000) % 60;
        if(s<10) myFile.print("0");
        myFile.print(s);
        myFile.print("]");//
        if(nmea[4] == 'G')myFile.print("\n");
        //10進処理省略時ここに改行 myFile.print("\n");
        Serial.print(nmea); Serial.print(' ');
      }
      else{
        Serial.print(nmea); //■■■■■ 表示
        Serial.print("[");
        s = GET_TIME/3600000;
        if(s<10) Serial.print("0");
        Serial.print(s);
        Serial.print(":");
        s = (GET_TIME/60000)%60;
        if(s<10) Serial.print("0");
        Serial.print(s);
        Serial.print(":");
        s =(GET_TIME/1000) % 60;
        if(s<10) Serial.print("0");
        Serial.print(s);
        Serial.print("]");
        //10進処理省略時ここに改行 Serial.print("\n");
      }
      ne_set();// 10進表記
      v=0;    //行カウンタリセット
      ck=0;   //comma[k]位置最大　リセット
      breakflag=1;//ループ終了フラグ
    }//改行処理終了 TRUE
    else{
      v++; if(v>=148)v=0;             //念のため
    }//改行処理終了 FALSE
    //////////////////////////////////////////
   if(breakflag) break; //ループ終了
  }
}

//■■■■■■■■■■■■■■■■■■■■
//   ＳＤカードタイムスタンプ
//■■■■■■■■■■■■■■■■■■■■
void dateTime(uint16_t* date, uint16_t* time)
{
 // (RTC.read(tm));
  uint16_t year = tm.Year+1970;
  uint8_t month = tm.Month; 

  // GPSやRTCから日付と時間を取得
  // FAT_DATEマクロでフィールドを埋めて日付を返す
  *date = FAT_DATE(year, month, tm.Day);

  // FAT_TIMEマクロでフィールドを埋めて時間を返す
  *time = FAT_TIME(tm.Hour, tm.Minute, tm.Second);

}
//■■■■■■■■■■■■■■■■■■■■
//   RMCから年月日時分秒抜き出し  2017年時　経過年数47 
//■■■■■■■■■■■■■■■■■■■■
void rmc_dateTime()
{
  int w;
  char s[5];
  w = comma[8];w++;//(日月年)指定位置カンマから数字先頭へ

//■■年  (20)17-1970  経過年数47 
s[0] = nmea[w+4];
s[1] = nmea[w+5];
s[2]= '\0';
tm.Year = atoi(s)+30; //変換時+30で経過年数へ
//■■月
s[0] = nmea[w+2];
s[1] = nmea[w+3];
s[2]= '\0';
tm.Month = atoi(s);
//■■日
s[0] = nmea[w+0];
s[1] = nmea[w+1];
s[2]= '\0';
tm.Day = atoi(s);

w = comma[0];w++;//(時分秒)指定位置カンマから数字先頭へ
//■■時
s[0] = nmea[w+0];
s[1] = nmea[w+1];
s[2]= '\0';
tm.Hour = atoi(s);
//■■分
s[0] = nmea[w+2];
s[1] = nmea[w+3];
s[2]= '\0';
tm.Minute = atoi(s);
//■■秒
s[0] = nmea[w+4];
s[1] = nmea[w+5];
s[2]= '\0';
tm.Second = atoi(s);//秒　

}
//■■■■■■■■■■■■■■■■■■■■
//  世界時から日本時間へ (とりあえず+時間のみ)　UTC_DateTimeConv(9); //UTCからJSTへ
//■■■■■■■■■■■■■■■■■■■■
int UTC_DateTimeConv(int s)
{
  int ndays[]={31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int w=0;
if(s>23 || s<-23)return 1;//エラー
if(s>0){
  tm.Hour = tm.Hour + s ;//時間加算
  if(tm.Hour>= 24){
    tm.Day++;
    tm.Hour = tm.Hour % 24;
  }
  else{
    return 0;         //日付変わらないならそのまま
  }

  //tm.Day　最大値チェック

  if (tm.Month == 2 && is_leap_year( tm.Year+1970 ) == 1){//2月ならうるう年チェック
//  Serial.println("ok");
    if(tm.Day>29){      //うるう年の2月 
        tm.Day =1;
        tm.Month =3;
      return 0;         //でのまま終了
    }
  }
  else{
   if(tm.Day>=ndays[tm.Month-1]){  
     tm.Day =1;
     tm.Month =tm.Month +1;
   }
  }
  //////////////////////////////
    if(tm.Month>12){            //12月超えたら　年+1
      tm.Year = tm.Year + 1;
      tm.Month = 1;
    }
  }
}
//■■■■■■■■■■■■■■■■■■■■
//  うるう年判定　(1:うるう年 0:平年)
//■■■■■■■■■■■■■■■■■■■■
int is_leap_year(int year)
{
   if (year % 400 == 0) return 1;
   else if (year % 100 == 0) return 0;
   else if (year % 4 == 0) return 1;
   else return 0;
}
//■■■■■■■■■■■■■■■■■■■■
//  N = 北緯  E = 東経 抜き出し
//■■■■■■■■■■■■■■■■■■■■
void ne_set()
{
char up_s[5];
char dn_s[10];

  if(nmea[comma[1]+1] =='A') {
                              // 3603.81884■■■■■
    up_s[0]= nmea[comma[2]+1];// 3
    up_s[1]= nmea[comma[2]+2];// 6
    up_s[2]= '\0';
    
    dn_s[0]= nmea[comma[2]+3];// 0
    dn_s[1]= nmea[comma[2]+4];// 3
    dn_s[2]= nmea[comma[2]+6];// 8
    dn_s[3]= nmea[comma[2]+7];// 1
    dn_s[4]= nmea[comma[2]+8];// 8
    dn_s[5]= nmea[comma[2]+9];// 8 
    dn_s[6]= nmea[comma[2]+10];// 4
    dn_s[7]= '\0';// 0
   n_seti = atoi(up_s);//35
   n_setg = atof(dn_s)*10000/60;
 
   Serial.print(n_seti);Serial.print('.');//整数部
   if(100000000>n_setg)Serial.print('0');
   Serial.print(n_setg);                //小数部
   Serial.print(' '); 
   if(files_sw1 == 1){//■■■■■
      myFile.print('%');
      myFile.print(n_seti);myFile.print('.');//整数部
      if(100000000>n_setg)myFile.print('0');
      myFile.print(n_setg);                //小数部
      myFile.print(' ');      
   }
                              // 13927.39876■■■■■
    up_s[0]= nmea[comma[4]+1];// 1
    up_s[1]= nmea[comma[4]+2];// 3
    up_s[2]= nmea[comma[4]+3];// 9
    up_s[3]= '\0';
    
    dn_s[0]= nmea[comma[4]+4];// 2
    dn_s[1]= nmea[comma[4]+5];// 7
    dn_s[2]= nmea[comma[4]+7];// 3
    dn_s[3]= nmea[comma[4]+8];// 9
    dn_s[4]= nmea[comma[4]+9];// 8
    dn_s[5]= nmea[comma[4]+10];// 7 
    dn_s[6]= nmea[comma[4]+11];// 6
    dn_s[7]= '\0';// 0
   n_seti = atoi(up_s);//139
   n_setg = atof(dn_s)*10000/60;
   Serial.print(n_seti);Serial.print('.');//整数部
   if(100000000>n_setg)Serial.print('0');
   Serial.print(n_setg);                  //小数部
   Serial.print("\n");
   if(files_sw1 == 1){//■■■■■
      myFile.print(n_seti);myFile.print('.');//整数部
      if(100000000>n_setg)myFile.print('0');
      myFile.print(n_setg);                //小数部
      myFile.print('\n');   //$GPRMC 部分のファイル後尾の改行
   }
  }
  else{Serial.print("\n");} //$GPRMC 以外のファイル後尾の改行
}
/*
動作説明
ＳＤカード　初期化失敗時　0.2秒のLED点滅で警告　動作ストップ
ＳＷ　ＯＮにてGPRMC 読み取り　データ有効であれば年月日　時間取得して閏年判定+日本時間に変更して
ファィル名タイムスタンプへ送りファィル追記モードでオープン
データ有効でない時にＳＷがＯＦＦになった場合そのままで待機(データをSerial.printするだけ)
nmeaデータ一文読んでデータヘッダがGPRMC＆GPGGA だった場合記録

データｓａｍｐｌｅ
$GPGGA,043155.00,3607.30097,N,13913.67422,E,1,06,2.40,80.3,M,38.5,M,,*6C[00:00:37]
$GPRMC,043156.00,A,3607.30106,N,13913.67438,E,0.210,,270818,,,A*73[00:00:37]%36.121684336 139.227906336
$GPGGA,043156.00,3607.30106,N,13913.67438,E,1,06,2.40,81.2,M,38.5,M,,*6D[00:00:38]
$GPRMC,043157.00,A,3607.30109,N,13913.67440,E,0.199,,270818,,,A*70[00:00:38]%36.121684832 139.227906656
$GPGGA,043157.00,3607.30109,N,13913.67440,E,1,06,2.40,81.5,M,38.5,M,,*6B[00:00:39]
$GPRMC,043158.00,A,3607.30104,N,13913.67442,E,0.233,,270818,,,A*73[00:00:39]%36.121684000 139.227907008

[]内は記録スタートした時からの時分秒
%以下が　10進数変換した座標データで　「36.121684336 139.227906336」
この部分をグーグルマップで検索かければ位置検索がすぐに出来ます　ちなみに埼玉県の道の駅　花園です
グーグルマップ作図についてはnmea2kml.exe　というソフトにてLOGファィル入れてやれば文末の追加データは無視されて変換されます。

Arduino+NEO_6M+ＳＤカード+スライドスイッチ(プルアップ)+ＬＥＤ(制限抵抗)と最低限の構成で仕上げたソース(ハード)です
物自体はちょうど手に入ったＧＰＳ＋ＳＤシールド使ってますが通常のNEO_6M+ＳＤカードでも配線と設定だけ注意すれば問題ないかと
大体1年ほど記録してますがまれに記録ができてない時がある。原因がどーも？
ハードなのかソフトなのかまだ断定できてないです
車のシュガソケットから電源4-5又で分岐してデータ取ってますが電圧変動か
ＧＰＳ衛星測位時大量に受信して且つ書き込み時にoverflowしてるのか？
車の振動による瞬断等？


これからArduinoで遊ぶ人の参考ににでもなれば幸いです。

ソース公開につき2018/10/14　文末追記
*/
