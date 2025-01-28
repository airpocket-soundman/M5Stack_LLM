/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 * M5Stack LLM Module で日本語対話。Serial MonitorでBoth BL&CRを設定するとよいです。
 * 
 * 変更元プログラム：https://gist.github.com/ksasao/37425d3463013221e7fd0f9ae5ab1c62
 * Faces keyboard対応 2025/01/25 @shikarunochi
 * BtnA でひらがな、BtnBでカタカナに切り替わります。2025/01/26 @shikarunochi
 * ただし、ひらがな・カタカナのみの入力だと認識してくれない言葉が多いです。
 */
#include <Arduino.h>
#include <M5Unified.h>
#include <M5ModuleLLM.h>
//#include <Wire.h>
#include "RomaKanaHenkan.h"

#define KEYBOARD_I2C_ADDR 0X08
#define KEYBOARD_INT      5
M5ModuleLLM module_llm;
String llm_work_id;

M5Canvas inputArea(&M5.Display);

void setup()
{
    M5.begin();
    Wire.begin();
    pinMode(KEYBOARD_INT, INPUT_PULLUP);

    M5.Display.setTextSize(1);
    M5.Display.setTextScroll(true);
    M5.Lcd.setTextFont(&fonts::efontJA_16);
    M5.Display.setScrollRect(0,0,319,210); //入力エリアを開ける

    /* Init module serial port */
    Serial.begin(115200);
    Serial2.begin(115200, SERIAL_8N1, 16, 17);  // Basic
    // Serial2.begin(115200, SERIAL_8N1, 13, 14);  // Core2
    // Serial2.begin(115200, SERIAL_8N1, 18, 17);  // CoreS3

    /* Init module */
    module_llm.begin(&Serial2);

    /* Make sure module is connected */
    M5.Display.printf(">> Check ModuleLLM connection..\n");
    while (1) {
        if (module_llm.checkConnection()) {
            break;
        }
    }

    /* Reset ModuleLLM */
    M5.Display.printf(">> Reset ModuleLLM..\n");
    module_llm.sys.reset();

    /* Setup LLM module and save returned work id */
    M5.Display.printf(">> Setup llm..\n");
    m5_module_llm::ApiLlmSetupConfig_t llm_config;
    llm_config.max_token_len = 1023; //返送 token サイズを最大に
    llm_work_id = module_llm.llm.setup(llm_config);
    Serial.print("Initialized...");

    inputArea.setColorDepth(8);
    inputArea.createSprite(320,20);
    inputArea.setTextColor(WHITE,BLACK);
    inputArea.setTextSize(1);
    inputArea.setTextFont(&fonts::efontJA_16);
    M5.Display.drawLine(0,211,319,211,BLUE);

}

String inputString = "";
String inputTempRomajiString ="";//ローマ字変換途中
int romajiMode = 0; //0:なし 1:ひらがな 2:カタカナ

void loop()
{
    if (Serial.available() > 0) {
      String input = Serial.readString();
      std::string question = input.c_str();

      M5.Display.setTextColor(TFT_GREEN);
      M5.Display.printf("<< %s\n", question.c_str());
      Serial.printf("<< %s\n", question.c_str());
      M5.Display.setTextColor(TFT_YELLOW);
      M5.Display.printf(">> ");
      Serial.printf(">> ");

      /* Push question to LLM module and wait inference result */
      module_llm.llm.inferenceAndWaitResult(llm_work_id, question.c_str(), [](String& result) {
          /* Show result on screen */
          M5.Display.printf("%s", result.c_str());
          Serial.printf("%s", result.c_str());
      });
      M5.Display.println();
    }

    //https://github.com/m5stack/M5Stack/blob/master/examples/Face/KEYBOARD/KEYBOARD.ino
    if (digitalRead(KEYBOARD_INT) == LOW) {
        Wire.requestFrom(KEYBOARD_I2C_ADDR, 1);  // request 1 byte from keyboard
        M5.Display.setTextColor(TFT_GREEN);
        while (Wire.available()) {
            uint8_t key_val = Wire.read();  // receive a byte as character
            if (key_val != 0) {
                if(key_val != 13){//改行
                    if(key_val==8){//BackSpace
                        if(romajiMode > 0){
                            if(inputTempRomajiString.length()>0){
                                inputTempRomajiString = inputTempRomajiString.substring(0,inputTempRomajiString.length()-1);  
                            }else{
                                if(inputString.length() > 0){
                                    int deleteByte = 1;
                                    //削除バイト数は、簡易判断なので変になることありそうです…。
                                    //for(int i = 0;i < inputString.length();i++){ //確認用
                                    //    Serial.printf("%x:",inputString.charAt(i));
                                    //}
                                    Serial.println();
                               
                                    if(inputString.length() >= 3 && deleteByte == 1){ 
                                        if(inputString.charAt(inputString.length()-3)&0xE0==0xE0){
                                            deleteByte = 3;
                                        }
                                    }                                    
                                    if(inputString.length() >= 2 && deleteByte == 1){ 
                                        if(inputString.charAt(inputString.length()-2)&0xC0==0xC0){
                                            deleteByte = 2;
                                        }
                                    }                                    

                                    inputString = inputString.substring(0,inputString.length()-deleteByte);
                                }
                            }
                        }else{
                            if(inputString.length() > 0){
                                inputString = inputString.substring(0,inputString.length()-1);
                            }
                        }
                    }else{
                        if(romajiMode > 0){
                            inputTempRomajiString = inputTempRomajiString + String((char)key_val);
                            //入力内容がローマ字変換表に該当したら、変換してinputStringに追加する。
                            int romaKanaIndex = 0;
                            while(true){
                                if(romaHenkan[romaKanaIndex][0]==""){
                                    //何にも該当せず、先頭が"n"だった場合、"ん"を追加
                                    if(inputTempRomajiString.length() > 1){
                                        if(inputTempRomajiString.charAt(0) == 'n'){
                                            if(romajiMode == 1){
                                                inputString = inputString + "ん";
                                            }else{
                                                inputString = inputString + "ン";
                                            }
                                            inputTempRomajiString = inputTempRomajiString.substring(1);
                                        }
                                    }
                                    break;
                                }
                                if(romaHenkan[romaKanaIndex][0] == inputTempRomajiString){
                                    if(romajiMode == 1){
                                        inputString = inputString + romaHenkan[romaKanaIndex][1];
                                    }else{
                                        inputString = inputString + romaHenkan[romaKanaIndex][2];
                                    }
                                    inputTempRomajiString = romaHenkan[romaKanaIndex][3];
                                    break;
                                }
                                romaKanaIndex++;
                            }
                        }else{
                            inputString = inputString + String((char)key_val);
                        }
                    }
                    inputArea.fillScreen(BLACK);
                    inputArea.setCursor(0,0);
                    inputArea.setTextColor(WHITE);
                    inputArea.print(inputString);
                    if(romajiMode == 1){
                        inputArea.setTextColor(GREEN);
                    }else{
                        inputArea.setTextColor(RED);
                    }
                    inputArea.print(inputTempRomajiString);
                    inputArea.pushSprite(0,213);
                }else{
                    inputArea.fillScreen(BLACK);
                    inputArea.setCursor(0,0);
                    inputArea.pushSprite(0,213);

                    M5.Display.print("<< " + inputString);
                    M5.Display.println();
                    M5.Display.setTextColor(TFT_YELLOW);   
                    M5.Display.printf(">> ");
                    std::string question = inputString.c_str();
                    Serial.printf("%s\n",inputString.c_str());
                    module_llm.llm.inferenceAndWaitResult(llm_work_id, question.c_str(), [](String& result) {
                        /* Show result on screen */
                        M5.Display.printf("%s", result.c_str());
                        Serial.printf("%s", result.c_str());
                    });
                    M5.Display.println();
                    inputString = "";
                }
            }
        }
    }
    M5.update();
    if(M5.BtnA.wasPressed()){
        if(romajiMode == 1){
            romajiMode = 0;
            M5.Display.drawLine(0,211,319,211,BLUE);
        }else{
            romajiMode = 1;
            M5.Display.drawLine(0,211,319,211,GREEN);
            inputTempRomajiString = "";
        }
    }
    if(M5.BtnB.wasPressed()){
        if(romajiMode == 2){
            romajiMode = 0;
            M5.Display.drawLine(0,211,319,211,BLUE);
        }else{
            romajiMode = 2;
            M5.Display.drawLine(0,211,319,211,RED);
            inputTempRomajiString = "";
        }
    }

    delay(100);
}