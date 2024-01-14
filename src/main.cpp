#include <Arduino.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "Audio.h"
#include "LGFX_ESP32_ST7735.hpp"
#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"
#include "AudioLibs/AudioRealFFT.h"
#include "AudioLibs/FFTDisplay.h"
//#include "AudioCodecs/CodecMP3Helix.h"
// #include "AudioLibs/MaximilianDSP.h"
// #include "libs/maxim.h"

static LGFX_ESP32_ST7735 lcd;
static int count = 0;

SPIClass spiSD(HSPI);
#define SD_SCK  14
#define SD_MISO 12
#define SD_MOSI 13
#define SD_SS   15

// Digital I/O used
#define I2S_DOUT      22
#define I2S_BCLK      26
#define I2S_LRC       25

const int BTN1 = 16;
int btnPushCnt = 1; 

//Audio audio;

#define C_SAMPLE_RATE 96000

AudioInfo info(C_SAMPLE_RATE, 2, 32);
I2SStream i2sOutput;
I2SStream i2sInput;
AudioRealFFT fft; // or AudioKissFFT
FFTDisplay fft_dis(fft);
MultiOutput out;
StreamCopy copier(out, i2sInput); // copies sound into i2s

// copy filtered values
//FilteredStream<int16_t, float> filtered(i2sInput, info.channels);  // Defiles the filter as BaseConverter
//StreamCopy copier(i2sOutput, filtered); // copies sound into i2s

// define FIR filter parameters
//float coef[] = { 0.021, 0.096, 0.146, 0.096, 0.021};

//EncodedAudioStream decoder(&i2sOutput, new MP3DecoderHelix()); // Decoding stream
//StreamCopy copier; 
//File audioFile;

//Maximilian maximilian(i2sOutput);

// Maximilian
// maxiOsc mySine, myPhasor; // This is the oscillator we will use to generate the test tone
// maxiFFT myFFT;

void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("File written");
    } else {
        Serial.println("Write failed");
    }
    file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("Failed to open file for appending");
        return;
    }
    if(file.print(message)){
        Serial.println("Message appended");
    } else {
        Serial.println("Append failed");
    }
    file.close();
}

// display fft result
void fftResult(AudioFFTBase &fft){
    float diff;
    auto result = fft.result();
    if (result.magnitude>100){
        Serial.print(result.frequency);
        Serial.print(" ");
        Serial.print(result.magnitude);  
        Serial.print(" => ");
        Serial.print(result.frequencyAsNote(diff));
        Serial.print( " diff: ");
        Serial.println(diff);
    }
}

void setup(void)
{
	// LCD初期化
    lcd.init();

    // 回転方向を 0～3 の4方向から設定します。(4～7を使用すると上下反転になります。)
    lcd.setRotation(1);

    // // バックライトの輝度を 0～255 の範囲で設定します。
    // lcd.setBrightness(255);
    //   // 画面塗り潰し
    // lcd.fillScreen(TFT_LIGHTGREY);
    // // カーソルを左上にリセット
    // lcd.setCursor(0, 0);
    // // フォント指定
    // lcd.setFont(&fonts::Font0);
    // // フォント色セット
    // lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    // lcd.setTextSize(1);
    // lcd.setTextScroll(true);

    //lcd.setEpdMode(epd_mode_t::epd_fastest);  // 最速更新、白黒反転なし、残像が残りやすい

    lcd.setFont(&fonts::lgfxJapanGothic_12);
    lcd.setTextColor(TFT_BLACK, TFT_WHITE);
    lcd.startWrite();
    lcd.println("HelloWorld");
    // 固定領域の描画
    //lcd.drawString("Temp", 100, 160);
    //lcd.drawString("Humd", 100, 128);
    lcd.endWrite();
    lcd.display();
    
    lcd.setCursor(0, 0);
    //lcd.setTextScroll(true);
    //lcd.setScrollRect(0, 0, lcd.width(), lcd.height());

    Serial.begin(115200);
    while (!Serial) {
    }

    // change to Warning to improve the quality
    //AudioLogger::instance().begin(Serial, AudioLogger::Info); 

    spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_SS); //SCK,MISO,MOSI,SS //HSPI1

    if (!SD.begin(SD_SS, spiSD)){
        Serial.println("Card Mount Failed");
        return;
    }

    // bool retSetPinout = audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    // Serial.printf("audio.setPinout=%s\n", (retSetPinout ? "true" : "false"));
    // audio.setVolume(21); // 0...21

    // bool retConnecttoFS = audio.connecttoFS(SD, "/maou_14_shining_star.mp3");
    // Serial.printf("audio.connecttoFS=%s\n", (retConnecttoFS ? "true" : "false"));

    // writeFile(SD, "/hello.txt", "hello ");
    // Serial.println("writeFile");
    // appendFile(SD, "/hello.txt", "World!\n");
    // Serial.println("appendFile");

    lcd.drawJpgFile(SD,"/image1.jpg");

    pinMode(BTN1, INPUT_PULLUP);  // 入力、プルアップ設定

    // setup filters for all available channels
    //filtered.setFilter(0, new FIR<float>(coef));
    //filtered.setFilter(1, new FIR<float>(coef));

    // start I2S in
    Serial.println("starting I2S...");
    auto config_in = i2sInput.defaultConfig(RX_MODE);
    config_in.copyFrom(info); 
    config_in.i2s_format = I2S_STD_FORMAT;
    config_in.is_master = true;
    config_in.port_no = 0;
    config_in.pin_bck = 17;
    config_in.pin_ws = 4;
    config_in.pin_data = 32;
    config_in.fixed_mclk = C_SAMPLE_RATE * 256;
    config_in.pin_mck = 0;
    config_in.buffer_size = 1024;
    i2sInput.begin(config_in);

    // start I2S out
    auto config_out = i2sOutput.defaultConfig(TX_MODE);
    config_out.copyFrom(info); 
    config_out.i2s_format = I2S_STD_FORMAT;
    config_out.is_master = true;
    config_out.port_no = 1;
    config_out.pin_bck = I2S_BCLK;
    config_out.pin_ws = I2S_LRC;
    config_out.pin_data = I2S_DOUT;
    config_out.buffer_size = 1024;
    i2sOutput.begin(config_out);

    Serial.println("I2S started...");

    //audioFile = SD.open("/maou_14_shining_star.mp3");

    // setup I2S based on sampling rate provided by decoder
    //decoder.setNotifyAudioChange(i2sOutput);
    //decoder.begin();

    // begin copy
    //copier.begin(decoder, audioFile);

    //maximilian.begin(config_out);

    // setup maximilian       
    //myFFT.setup(512, 512, 256);

    // Setup FFT
    auto tcfg = fft.defaultConfig();
    tcfg.length = 8192;
    tcfg.channels = 2;
    tcfg.sample_rate = C_SAMPLE_RATE;
    tcfg.bits_per_sample = 16;
    tcfg.callback = &fftResult;
    fft.begin(tcfg);

    out.add(i2sOutput);
    out.add(fft);
}

int btn1statusBack = HIGH;

// void play(float *output) {    
//     //float myOut=mySine.sinewave(myPhasor.phasorBetween(0.2,100,5000));
//     //output[0] is the left output. output[1] is the right output
    
//     if (myFFT.process(myOut)) {
//         //if you want you can mess with FFT frame values in here
        
//     }
    
//     output[0]=myOut;//simple as that!
//     output[1]=output[0];
// }

void loop(void)
{
    copier.copy();
    //copierFFT.copy();
    //maximilian.copy();
    //audio.loop();
    int btn1status = digitalRead(BTN1);
    //Serial.printf("btn1status=%d\n", btn1status);
    if (btn1status == LOW && btn1status != btn1statusBack) {
        btnPushCnt++;
        if (btnPushCnt > 4) {
            btnPushCnt = 1;
        }
        switch (btnPushCnt) {
            case 1:
                Serial.printf("/image1.jpg\n");
                lcd.drawJpgFile(SD, "/image1.jpg");
                break;
            case 2:
                Serial.printf("/image2.jpg\n");
                lcd.drawJpgFile(SD, "/image2.jpg");
                break;
            case 3:
                Serial.printf("/image3.jpg\n");
                lcd.drawJpgFile(SD, "/image3.jpg");
                break;
            case 4:
                Serial.printf("/image4.jpg\n");
                lcd.drawJpgFile(SD, "/image4.jpg");
                break;
            default:
                btnPushCnt = 0;
        }
    }
    btn1statusBack = btn1status;
}

