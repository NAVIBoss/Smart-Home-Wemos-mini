#define FirmwareVersion "1.0"                  // версия прошивки
//#define clearMemory                          // записать в память начальные значения
#define ESPName "Viliy SMART HOME"             // имя ESP в сети WiFi
//#define AccessPointEnable                      // запуск в режиме точки доступа без WiFi роутера
#define DebagEnable                          // вывод в порт, не работает подсветка кнопок
#define RelayBus                               // связь с ардуино, управляющей реле

#include <Arduino.h>
const boolean sensKitchen=0;                   // считать сенсор сработавшим когда на входе 0 или 1
const boolean sensAlarm=0;
const boolean sensBath=0;
const boolean sensDoor=0;

#define FixState 1                             // Фиксировать предыдущее состояние
#define FixChange 2                            // Фиксировать измененное состояние
#define RestoreState 3                         // Восстановить все кроме ванной, вернет 1 при удаче, 0 - при невозможности
#define FixHard 4                              // Принудительная фиксация текущего состояния
#define FixCheck 5                             // Проверка на возможность восстановления, вернет 1 при возможности, 0 - при невозможности
#define RestoreAll 6                           // Восстановление всего освещения, вернет 1 при удаче, 0 - при невозможности
#define ResetFIX 7                             // Обнуление всех зафиксированных состояний

boolean processRestoreRelay=0;                  // восстановление состояния реле, не отсылать состояние в Arduino Relay
boolean sceneRestore=0;                         // произнести фразу: Сцена восстановлена
boolean timerVoice=0;                           // произнести фразу: Выход из квартиры по таймеру
boolean cnox=0;                                 // режим погасить все, вкл дежурный на входе на время
boolean timerKoridor=0;                         // режим работы любого таймера
boolean standBy=0;                              // режим StandBy - фиолетовый мигающий индикатор
boolean ventStandBy=0;                          // идет отсчет отключения вытяжки
boolean timerBathRGBEnable=0;                   // таймер автооключения LED подсветки ванной активирован
boolean BathRGBautoOFF=0;                       // подсветка в ванне выключена по таймеру
boolean BathVentautoOFF=0;                      // вентиляция в ванне выключена по таймеру
boolean nigtModeSwitch=0;                       // не снимать ночной режим при его активации
uint16_t BtnHOLD=500;                           // время двойного клика
uint16_t lastVoice=0;                           // номер последней фразы приветствия
boolean ClientConnected=0;                      // клиент RemoteXY подключен
boolean OTApossible=0;                          // хватит ли места в памяти для OTA прошивки
boolean changeColorProcess[9]={0,0,0,0,0,0,0,0,0}; // нужно изменить цвет модуля RGB
boolean ignoreMoveBath=0;                       // игнорировать датчик в ванной 20 сек после переключения света
boolean KitchenVent=0;                          // состояние вытяжки в ванной
uint8_t saveDelay;                              // задержка сохранения параметров в EEPROM
boolean AlarmMode=0;                            // режим тревоги
//boolean Initialization=0;                       // Инициализация при запуске
boolean waitMode=0;                             // режим ожидания (когда никого дома, ждать сработки сенсора входа или нажатие кнопок)
uint32_t waitModeStartTime;                     // таймер отсрочки, чтобы не выклычался режим ожидания при выключении корридора
boolean progMode=0;                             // режим программирования кнопок
uint8_t statusBus;                              // статус шины передачи данных между контроллерами
boolean koridorSensor;                          // состояние входной двери
boolean nigtMode=0;                             // режим сна - LED индикаторы притушены
boolean sceneFixKoridor=0;                      // сцена света коридора зафиксирована
boolean sceneFixRoom=0;                         // сцена света комнаты зафиксирована
boolean sceneFixKitchen=0;                      // сцена света кухни зафиксирована
boolean sceneFixBar=0;                          // сцена света бара зафиксирована
boolean sceneFixBath=0;                         // сцена света ванны зафиксирована
uint8_t ledColorRX[5][3];                       // цвет LED модулей на экране смартфона
struct sendStrukt {                             // структура передаваемых данных через serialBus
uint16_t val=0;
};

enum modeLed {syncInd, progInd, alarmInd, blockInd, cnoxInd, timerInd, waitInd, autoIndOn, autoIndOff, autoOnNight, autoOffNight} modeInd[11];
boolean autoLedOn[10];
boolean autoLedOff[10];
boolean cnoxLedInd[10];
boolean timerLedInd[10];
boolean waitLedInd[10];
boolean autoOnLedNight[10];
boolean autoOffLedNight[10];
boolean alarmLedInd[10];
boolean progLedInd[10];
boolean syncLedInd[10];
boolean blockLedInd[10];

#define saveData1 \
{saveDelay=10;\
saveChange=1;}

#define saveData60 \
{saveDelay=60;\
saveChange=1;}

#define StopHere {CMn("Programm stoped"); for(;;);} // вечный цикл остановит выполнение в этой точке (if (criticalError) StopHere;)

#define EVERY_MS(x) \
static uint32_t tmr;\
bool flag = millis() - tmr >= (x);\
if (flag) tmr += (x);\
if (flag)                                           // Выполнять каждые x мс - EVERY_MS(100) {...}

#define returnMS(x) \
{static uint32_t tms;\
if (millis() - tms >= x) tms = millis();\
else return;}                                       // Возврат, если прошло меньше x мс

#define returnSec(x) \
{static uint32_t tmss;\
if (millis() - tmss >= x*1000) tmss = millis();\
else return;}                                       // Возврат, если прошло меньше x с

#if defined DebagEnable
#define CMn2(x,y) Serial.println(x,y)
#define CM2(x,y) Serial.print(x,y)
#define CMn(x) Serial.println(x)
#define CM(x) Serial.print(x)
#define CMf(x,y) Serial.printf(x,y)
#else
#define CMn2(x,y)
#define CM2(x,y)
#define CMn(x)
#define CM(x)
#define CMf(x,y)
#endif

#define for_i(from, to) for(uint8_t i = (from); i < (to); i++)
#define for_c(from, to) for(uint8_t c = (from); c < (to); c++)
#define for_r(from, to) for(uint8_t r = (from); r < (to); r++)
#define for_t(from, to) for(uint8_t t = (from); t < (to); t++)

#define FASTLED_INTERRUPT_RETRY_COUNT 0
#define FASTLED_ESP8266_RAW_PIN_ORDER
#define FASTLED_FORCE_SOFTWARE_SPI
#define FRAMES_PER_SECOND  120

#include <FastLED.h>                            // управление RGB модулями P9813
const uint8_t NUM_LEDS=5;                       // сколько модулей
const uint8_t DATA_PIN=13;                      // Data PIN
const uint8_t CLOCK_PIN=15;                     // Clock PIN
CRGB leds[NUM_LEDS];                            // управление RGB подсветкой комнаты

#if not defined DebagEnable
const uint8_t DATA_PIN1=14;                     // Data PIN1
const uint8_t CLOCK_PIN1=12;                    // Clock PIN1
#else
const uint8_t DATA_PIN1=16;                     // Data PIN1
const uint8_t CLOCK_PIN1=16;                    // Clock PIN1
#endif
CRGB ledsw[NUM_LEDS];                           // управление подсветкой выключателей

struct moduleRGBVal {
uint8_t Led_Value[3];
uint8_t Led_Value_Fade[3];
uint8_t Led_Value_Fade_Start[3];
uint8_t Led_Value_Old[3];
int16_t delta[3];
int16_t fadeCount=0;
int16_t steps = 0;
uint8_t Led_Pin_[3];
} lVal[9];

#include "GammaCorrectionLib.h"
//#include "PCF8575.h"
//PCF8575 pcf8575(0x20);

#include <ESP8266WiFi.h>

#if defined AccessPointEnable
IPAddress local_IP(192,168,4,22);
IPAddress gateway(192,168,4,1);
IPAddress subnet(255,255,255,0);
#define REMOTEXY_MODE__ESP8266WIFI_LIB_POINT // точка доступа на ESP
#else
#define host_name "Viliy SMART HOME"
#define REMOTEXY_MODE__ESP8266WIFI_LIB // - WiFi роутер
#include "ArduinoOTA.h"
#endif

#include <AsyncElegantOTA.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
AsyncWebServer server(80);
#include <RemoteXY.h>

#if defined AccessPointEnable
#define REMOTEXY_WIFI_SSID ESPName // точка доступа на ESP
#else
#define REMOTEXY_WIFI_SSID "Keenetic-1485" // - WiFi роутер
#endif

#define REMOTEXY_WIFI_PASSWORD "cdtnbr1973"
#define REMOTEXY_SERVER_PORT 6377

// конфигурация интерфейса  
#pragma pack(push, 1)
uint8_t RemoteXY_CONF[] =   // 1680 bytes
  { 255,32,0,244,1,137,6,16,127,5,130,3,46,94,1,3,2,143,130,3,
  44,95,5,1,2,143,67,4,2,99,61,5,1,33,127,51,129,0,38,42,
  6,5,2,215,240,159,147,138,0,129,0,1,10,6,5,2,215,240,159,147,
  138,0,67,4,1,6,20,4,2,33,127,11,130,3,46,35,3,1,2,143,
  130,3,14,51,3,1,2,143,130,3,16,52,3,1,2,143,130,3,33,51,
  1,4,2,143,130,3,31,52,2,1,2,143,130,3,1,51,3,1,2,143,
  70,21,18,68,14,13,2,26,190,0,70,21,18,54,14,13,2,26,190,0,
  70,20,48,21,14,14,2,26,190,0,10,117,19,54,13,12,2,78,87,33,
  32,208,151,208,176,208,187,32,73,32,32,0,31,10,117,19,68,13,12,2,
  78,87,33,32,208,151,208,176,208,187,32,73,73,32,0,31,70,21,48,67,
  14,13,2,26,190,0,10,117,49,67,13,12,2,78,87,33,32,32,208,145,
  208,176,209,128,32,32,0,31,70,21,48,37,14,13,2,26,190,0,10,117,
  49,37,13,12,2,78,87,33,208,154,209,131,209,133,208,189,209,143,32,0,
  31,70,21,1,53,14,13,2,26,190,0,10,117,2,53,13,12,2,78,87,
  33,208,146,208,176,208,189,208,189,208,176,0,31,70,37,48,7,14,13,2,
  26,190,135,48,10,117,49,7,13,12,2,78,87,33,32,208,146,209,133,208,
  190,208,180,32,0,31,70,20,48,51,14,14,2,26,190,0,70,52,1,82,
  14,14,2,26,190,245,135,48,3,10,116,2,82,13,13,2,221,213,215,226,
  135,161,240,159,147,138,226,135,161,0,224,130,3,45,5,1,3,2,143,70,
  38,1,97,14,13,2,26,190,135,48,10,118,2,97,13,12,2,178,199,166,
  32,208,146,208,181,208,189,209,130,32,32,0,31,130,3,46,80,1,3,2,
  143,130,3,59,35,3,1,2,143,130,3,47,65,2,1,2,143,130,3,33,
  95,3,1,2,143,10,116,49,21,13,13,2,221,213,215,226,148,134,226,135,
  134,32,240,159,145,128,0,224,10,116,49,51,13,13,2,221,213,215,226,135,
  163,240,159,147,138,226,135,163,0,224,130,3,46,65,1,3,2,143,130,3,
  46,51,1,4,2,143,130,3,16,65,1,3,2,143,130,3,33,65,1,3,
  2,143,130,3,16,80,1,3,2,143,130,3,59,5,3,1,2,143,130,3,
  46,5,2,1,2,143,130,3,1,5,3,1,2,143,70,20,18,82,14,14,
  2,26,190,0,130,3,33,80,1,3,2,143,130,3,16,53,1,1,2,143,
  10,116,19,82,13,13,2,221,213,215,226,135,161,240,159,147,138,226,135,161,
  0,224,70,18,35,49,10,46,2,26,190,0,3,5,35,49,10,46,2,217,
  144,70,22,1,246,13,13,1,26,190,0,67,0,2,46,61,5,1,33,127,
  61,129,0,49,21,10,3,1,33,209,129,208,181,208,186,209,131,208,189,208,
  180,0,2,1,52,55,10,9,1,64,26,31,31,79,78,0,79,70,70,0,
  129,0,40,56,9,4,1,201,208,144,208,178,209,130,208,190,0,129,0,2,
  68,52,4,1,201,208,161,208,190,209,129,209,130,208,190,209,143,208,189,208,
  184,208,181,32,82,71,66,32,208,191,208,190,209,129,208,187,208,181,32,208,
  178,208,186,208,187,0,130,3,253,52,69,1,1,143,130,3,0,66,63,1,
  1,143,67,0,26,76,34,5,1,33,127,41,129,0,3,90,42,4,1,201,
  208,158,209,130,208,186,208,187,209,142,209,135,208,184,209,130,209,140,32,208,
  179,208,184,209,128,208,187,209,143,208,189,208,180,209,139,0,2,1,49,91,
  13,8,1,64,26,31,31,79,78,0,79,70,70,0,130,3,0,88,63,1,
  1,143,129,0,8,94,38,4,1,201,208,189,208,176,32,208,190,208,186,208,
  189,208,181,32,208,191,208,190,32,209,130,208,176,208,185,208,188,208,181,209,
  128,209,131,0,70,96,1,48,2,2,2,26,191,2,135,219,36,109,0,49,
  51,70,144,1,45,2,2,2,26,136,3,70,18,1,36,40,9,1,26,190,
  0,3,133,1,36,40,9,1,64,26,70,18,1,74,24,9,1,26,190,0,
  3,131,1,74,24,9,1,64,26,70,18,1,25,61,6,1,26,190,0,4,
  128,1,25,61,6,1,64,73,70,18,1,13,61,6,1,26,190,0,4,128,
  1,13,61,6,1,64,73,130,3,36,53,1,13,1,143,67,4,42,38,21,
  5,1,33,127,41,70,151,0,21,63,11,1,127,127,0,67,2,41,8,19,
  4,1,33,127,41,70,20,48,81,14,14,2,26,190,0,10,116,49,81,13,
  13,2,221,213,215,226,135,163,240,159,147,138,226,135,163,0,224,130,3,59,
  65,3,1,2,143,130,3,46,35,1,3,2,143,130,3,59,95,3,1,2,
  143,130,3,33,94,1,2,2,143,70,21,34,97,12,13,2,26,190,0,70,
  20,1,67,14,14,2,26,190,0,10,116,2,67,13,13,2,168,199,33,208,
  151,208,181,209,128,208,186,208,176,208,187,208,190,0,31,70,21,18,97,14,
  13,2,26,190,0,10,117,19,97,13,12,2,78,87,33,208,161,209,130,208,
  181,208,189,208,176,0,31,70,21,48,97,13,13,2,26,190,0,10,117,49,
  97,12,12,2,151,171,201,65,76,76,32,79,70,70,0,139,131,3,2,246,
  12,12,2,2,33,226,139,150,0,70,22,48,246,13,13,0,26,190,0,131,
  2,49,246,12,12,1,2,33,226,139,151,0,70,21,1,246,14,13,2,26,
  190,0,10,117,2,246,13,12,2,222,213,215,32,208,161,104,105,108,108,32,
  0,224,70,21,17,246,14,13,2,26,190,0,10,117,18,246,13,12,2,96,
  100,33,240,159,146,161,240,159,146,175,0,31,70,37,33,246,13,13,2,26,
  190,245,48,10,117,34,246,12,12,2,52,59,33,226,157,132,32,226,152,131,
  32,226,157,132,0,181,10,117,35,97,11,12,2,84,87,33,32,240,159,148,
  140,32,0,31,10,5,46,246,15,14,1,127,127,67,2,33,1,26,5,1,
  201,127,31,67,2,33,253,26,5,1,201,127,31,70,37,16,246,14,13,1,
  26,190,34,64,10,117,17,246,13,12,1,40,45,32,65,108,97,114,109,0,
  31,67,0,2,7,36,4,1,33,127,41,65,118,60,68,2,3,1,65,118,
  60,72,2,3,1,65,118,60,76,2,3,1,65,118,60,80,2,3,1,65,
  118,60,84,2,3,1,65,118,2,85,10,2,1,65,118,13,85,10,2,1,
  65,118,24,85,10,2,1,65,118,35,85,10,2,1,65,118,46,85,10,2,
  1,130,3,16,94,1,3,2,143,70,160,1,48,2,2,2,26,202,62,0,
  69,1,1,40,4,4,2,6,0,1,6,45,45,2,217,26,69,0,40,5,
  4,4,2,230,70,151,39,5,5,4,2,127,127,0,129,0,2,56,21,4,
  1,201,208,147,208,190,208,187,208,190,209,129,208,190,208,178,208,190,208,185,
  0,2,1,25,55,10,9,1,64,26,31,31,79,78,0,79,70,70,0,129,
  0,2,60,20,4,1,201,208,191,208,190,208,188,208,190,209,137,208,189,208,
  184,208,186,0,129,0,38,60,13,4,1,201,209,128,208,181,208,182,208,184,
  208,188,0,67,2,23,32,37,3,1,33,127,41,67,2,23,248,37,4,1,
  33,127,41,70,151,253,236,68,24,0,1,37,3,66,129,0,88,63,1,1,
  108,143,130,3,16,107,1,3,2,143,70,151,254,97,67,26,0,1,37,3 };
  
// структура определяет все переменные и события вашего интерфейса управления 
struct {

    // input variables
  uint8_t Spot0; // =1 если включено, иначе =0 
  uint8_t Spot1; // =1 если включено, иначе =0 
  uint8_t Bar; // =1 если включено, иначе =0 
  uint8_t Kitchen_Work; // =1 если включено, иначе =0 
  uint8_t Bath; // =1 если включено, иначе =0 
  uint8_t Koridor; // =1 если включено, иначе =0 
  uint8_t Bath_RGB; // =1 если включено, иначе =0 
  uint8_t Vent; // =1 если включено, иначе =0 
  uint8_t Mirror_RGB; // =1 если включено, иначе =0 
  uint8_t Kitchen_RGB; // =1 если включено, иначе =0 
  uint8_t Window_RGB; // =1 если включено, иначе =0 
  uint8_t Select_RGB; // =0 если переключатель в положении A, =1 если в положении B, =2 если в положении C, ... 
  uint8_t AutoMode_switch; // =1 если переключатель включен и =0 если отключен 
  uint8_t TimerWindow_switch; // =1 если переключатель включен и =0 если отключен 
  uint8_t Timer_select; // =0 если переключатель в положении A, =1 если в положении B, =2 если в положении C, ... 
  uint8_t Start_RGB; // =0 если переключатель в положении A, =1 если в положении B, =2 если в положении C, ... 
  int8_t sek; // =0..100 положение слайдера 
  int8_t min; // =0..100 положение слайдера 
  uint8_t Bar_RGB; // =1 если включено, иначе =0 
  uint8_t Bath_Mirror; // =1 если включено, иначе =0 
  uint8_t Wall; // =1 если включено, иначе =0 
  uint8_t All_OFF; // =1 если включено, иначе =0 
  uint8_t Relax; // =1 если включено, иначе =0 
  uint8_t MaxLight; // =1 если включено, иначе =0 
  uint8_t NewYear; // =1 если включено, иначе =0 
  uint8_t Torsher; // =1 если включено, иначе =0 
  uint8_t Hide; // =1 если включено, иначе =0 
  uint8_t WaitMode; // =1 если включено, иначе =0 
  uint8_t Set_rgb_r; // =0..255 значение Красного цвета 
  uint8_t Set_rgb_g; // =0..255 значение Зеленого цвета 
  uint8_t Set_rgb_b; // =0..255 значение Синего цвета 
  uint8_t Sound_switch; // =1 если переключатель включен и =0 если отключен 

    // output variables
  char CountDown_Window[51];  // =строка UTF8 оканчивающаяся нулем 
  char RGB_text[11];  // =строка UTF8 оканчивающаяся нулем 
  uint8_t Spot1_led; // led state 0 .. 1 
  uint8_t Spot0_led; // led state 0 .. 1 
  uint8_t Mirror_RGB_led; // led state 0 .. 1 
  uint8_t Bar_led; // led state 0 .. 1 
  uint8_t Kitchen_Work_led; // led state 0 .. 1 
  uint8_t Bath_led; // led state 0 .. 1 
  uint8_t Koridor_led; // led state 0 .. 2 
  uint8_t Kitchen_RGB_led; // led state 0 .. 1 
  uint8_t Bath_RGB_led; // led state 0 .. 3 
  uint8_t Vent_led; // led state 0 .. 2 
  uint8_t Window_RGB_led; // led state 0 .. 1 
  uint8_t Select_RGB_led; // led state 0 .. 1 
  uint8_t Main_led; // led state 0 .. 1 
  char Timer_select_text[61];  // =строка UTF8 оканчивающаяся нулем 
  char Start_RGB_text[41];  // =строка UTF8 оканчивающаяся нулем 
  uint8_t Mode_led; // led state 0 .. 6 
  uint8_t TimerWin_led; // led state 0 .. 1 
  uint8_t Timer_select_led; // led state 0 .. 1 
  uint8_t Start_RGB_led; // led state 0 .. 1 
  uint8_t sek_led; // led state 0 .. 1 
  uint8_t min_led; // led state 0 .. 1 
  char Timer_value_text[41];  // =строка UTF8 оканчивающаяся нулем 
  uint8_t HideSec_led; // led state 0 .. 1 
  char Value_text[41];  // =строка UTF8 оканчивающаяся нулем 
  uint8_t Bar_RGB_led; // led state 0 .. 1 
  uint8_t Torsher_led; // led state 0 .. 1 
  uint8_t Bath_Mirror_led; // led state 0 .. 1 
  uint8_t Wall_led; // led state 0 .. 1 
  uint8_t All_OFF_led; // led state 0 .. 1 
  uint8_t Set_led; // led state 0 .. 1 
  uint8_t Relax_led; // led state 0 .. 1 
  uint8_t MaxLight_led; // led state 0 .. 1 
  uint8_t NewYear_led; // led state 0 .. 2 
  char Timer_Name2[31];  // =строка UTF8 оканчивающаяся нулем 
  char Timer_Name1[31];  // =строка UTF8 оканчивающаяся нулем 
  uint8_t WaitMode_led; // led state 0 .. 2 
  char Button_text[41];  // =строка UTF8 оканчивающаяся нулем 
  uint8_t But0_led_r; // =0..255 яркость красного цвета индикатора 
  uint8_t But0_led_g; // =0..255 яркость зеленого цвета индикатора 
  uint8_t But0_led_b; // =0..255 яркость синего цвета индикатора 
  uint8_t But1_led_r; // =0..255 яркость красного цвета индикатора 
  uint8_t But1_led_g; // =0..255 яркость зеленого цвета индикатора 
  uint8_t But1_led_b; // =0..255 яркость синего цвета индикатора 
  uint8_t But2_led_r; // =0..255 яркость красного цвета индикатора 
  uint8_t But2_led_g; // =0..255 яркость зеленого цвета индикатора 
  uint8_t But2_led_b; // =0..255 яркость синего цвета индикатора 
  uint8_t But3_led_r; // =0..255 яркость красного цвета индикатора 
  uint8_t But3_led_g; // =0..255 яркость зеленого цвета индикатора 
  uint8_t But3_led_b; // =0..255 яркость синего цвета индикатора 
  uint8_t But4_led_r; // =0..255 яркость красного цвета индикатора 
  uint8_t But4_led_g; // =0..255 яркость зеленого цвета индикатора 
  uint8_t But4_led_b; // =0..255 яркость синего цвета индикатора 
  uint8_t RGB0_led_r; // =0..255 яркость красного цвета индикатора 
  uint8_t RGB0_led_g; // =0..255 яркость зеленого цвета индикатора 
  uint8_t RGB0_led_b; // =0..255 яркость синего цвета индикатора 
  uint8_t RGB1_led_r; // =0..255 яркость красного цвета индикатора 
  uint8_t RGB1_led_g; // =0..255 яркость зеленого цвета индикатора 
  uint8_t RGB1_led_b; // =0..255 яркость синего цвета индикатора 
  uint8_t RGB2_led_r; // =0..255 яркость красного цвета индикатора 
  uint8_t RGB2_led_g; // =0..255 яркость зеленого цвета индикатора 
  uint8_t RGB2_led_b; // =0..255 яркость синего цвета индикатора 
  uint8_t RGB3_led_r; // =0..255 яркость красного цвета индикатора 
  uint8_t RGB3_led_g; // =0..255 яркость зеленого цвета индикатора 
  uint8_t RGB3_led_b; // =0..255 яркость синего цвета индикатора 
  uint8_t RGB4_led_r; // =0..255 яркость красного цвета индикатора 
  uint8_t RGB4_led_g; // =0..255 яркость зеленого цвета индикатора 
  uint8_t RGB4_led_b; // =0..255 яркость синего цвета индикатора 
  uint8_t Mode_led1; // led state 0 .. 2 
  int16_t sound; // =0 нет звука, иначе ID звука, для примера 1001, смотри список звуков в приложении 
  int16_t sound_Led; // =0 нет звука, иначе ID звука, для примера 1001, смотри список звуков в приложении 
  uint8_t Block_Sound; // led state 0 .. 1 
  char IP_text[41];  // =строка UTF8 оканчивающаяся нулем 
  char firmware[41];  // =строка UTF8 оканчивающаяся нулем 
  uint8_t Alarm_led1; // led state 0 .. 1 
  int8_t level; // =0..100 положение уровня 
  uint8_t Alarm_led2; // led state 0 .. 1 

    // other variable
  uint8_t connect_flag;  // =1 if wire connected, else =0 

} RemoteXY;
#pragma pack(pop)

/////////////////////////////////////////////  99->107 4->1 254->248
//           END RemoteXY include          //
/////////////////////////////////////////////

void RGB_PowerOn(uint8_t module);
void RGB_PowerOff(uint8_t module);
void CountDownСnox(uint8_t Mode);
void sBus(uint16_t data);
void voice(uint8_t ID);
#if defined RelayBus
void rBus(uint16_t data);
#endif
boolean LightOFFAll(boolean in);
boolean LightOFFAllWithoutBathRoom(boolean in);

#include <EEPROM.h>
boolean saveChange = 0;                         // нужно сохранить данные
struct Save {
uint16_t saveCount=0;                           // счетчик записи в одну и ту же ячейку
boolean autoMode = 1;                           // задействовать датчики | только ручное управление
uint16_t standByTime = 3600;                    // время StandBy режима (сек) | переключили кнопками или IR
double pressSpeed = 0.80;                       // коэффициент задержек срабатывания кнопки 0.8 - оригинал 0.8 - 2.1 - 4.0 и т.д.
uint8_t RGB_ModulePower[5] = {0,0,0,0,0};       // состояние RGB модулей P9813 ON/OFF
boolean GammaCorrection = 1;                    // гамма коррекция RGB
int8_t FadeDelay = 10;                          // задержка - время перехода цвета
uint8_t RGB_r[5];                               // цвет RGB подсветки зеркала на входе / окон / кухни / ванной
uint8_t RGB_g[5];
uint8_t RGB_b[5];
uint8_t StartMode = 0;                          // состояние RGB подсветки при старте: 0 - восстановить, 1 - выкл, 2 - вкл
uint16_t Timer[4] = {15,3600,60,3600};          // таймеры отключения RGB и новогодних розеток на окнах (сек) - вход, ванна RGB, ванна вентиляция, розетки окна
boolean PowerWindowOFF = 1;                     // отключить гирлянды на окнах по таймеру
boolean Zumer=1;                                // Звуковое сопровождение
boolean Sound=1;                                // Озвучивать режимы голосом на смартфоне
} save;

struct SaveHome {
boolean s_Koridor;
boolean s_Mirror_RGB;
boolean s_Spot0;
boolean s_Spot1;
boolean s_Torsher;
boolean s_Wall;
boolean s_NewYear;
boolean s_Window_RGB;
boolean s_Kitchen_Work;
boolean s_Kitchen_RGB;
boolean s_Bar;
boolean s_Bar_RGB;
boolean s_Bath;
boolean s_Bath_Mirror;
boolean s_Bath_RGB;
} lastLight;

#include "Save.h"
XIIIMSave dataToEEPROM;

#include "GBUS.h"
const uint8_t Pin_Spot0=13;
const uint8_t Pin_Spot1=4;
const uint8_t Pin_NewYear=15;
const uint8_t Pin_Torsher=14;
const uint8_t Pin_Bar=6;
const uint8_t Pin_Vent=7;
const uint8_t Pin_BathMirror=11;
const uint8_t Pin_BathRoom=8;
const uint8_t Pin_Kitchen=10;
const uint8_t Pin_Koridor=12;
const uint8_t Pin_Wall=5;
#if defined RelayBus
#define RelaySwitch(x,y) RelaySw(x,y)

#include <SoftwareSerial.h>
SoftwareSerial relaySerial(4, 5); // RX, TX - Arduino управления реле
GBUS relayBus(&relaySerial, 1, 20);  // адрес 1, буфер 20 байт
#else
#define RelaySwitch(x,y)
#endif
#if defined DebagEnable
#if not defined RelayBus
#include <SoftwareSerial.h>
#endif
SoftwareSerial mySerial(12, 14); // RX, TX - Ардуино управления кнопками и пультов
GBUS bus(&mySerial, 3, 20);  // адрес 3, буфер 20 байт
#else
GBUS bus(&Serial, 3, 20);  // адрес 3, буфер 25 байт
#endif

void SaveData() {static boolean changeStart, timer, statusChek; if(!saveChange && !timer) {changeStart=0; return;} static uint32_t saveTimer, skipTimer;
if(!timer && timerBathRGBEnable) {timer=1; saveChange=0; statusChek=1; skipTimer=millis(); CMn("---- >>> Таймер! Скипаем сохранение"); return;} // игнорируем изменения, если работают таймеры
else if(statusChek) {statusChek=0; skipTimer=millis(); timer=0; CMn("----- Таймер отключен, засекаем время -----");}
if(millis()-skipTimer<1000) return;
if(timer) {CMn("Работал таймер, вернули сохранение"); timer=0; if(!RemoteXY.Bath_RGB) {CMn("RGB ванны осталась выключена"); if(!saveChange) return;} else {
save.RGB_ModulePower[4]=1; CMn("Включили RGB ванны, сохраняем");}}
if(!changeStart) {changeStart=1; saveTimer=millis(); CMn("\\/\\/ Запущен таймер сохранения \\/\\/");}
if(millis()-saveTimer<saveDelay*1000) return; dataToEEPROM.SaveData(); saveChange=0;}

void RelaySw(uint8_t pin, boolean in) {//1-PIN 1 On, 21-PIN 1 Off : 2-PIN 2 On, 22-PIN 2 Off
uint8_t spin;
CM("Реле "); CM(pin); CM(" <- "); CMn(in);
switch(pin) {
case 1: spin=1; break;
case 2: spin=2; break;
case 3: spin=3; break;
case 4: spin=4; break;
case 5: spin=5; break;
case 6: spin=6; break;
case 7: spin=7; break;
case 8: spin=8; break;
case 9: spin=9; break;
case 10: spin=10; break;
case 11: spin=11; break;
case 12: spin=12; break;
case 13: spin=13; break;
case 14: spin=14; break;
case 15: spin=15; break;
case 16: spin=16; break;
default: return;
}
#if defined RelayBus
rBus(in ? spin : spin+20);
#else
#define rBus(x)
#endif
}

void Process_Out() {static uint8_t change[15];
if(processRestoreRelay) {
change[0]=RemoteXY.Spot0;
change[1]=RemoteXY.Spot1;
change[2]=RemoteXY.Torsher;
change[3]=RemoteXY.Kitchen_Work;
change[4]=RemoteXY.Bar;
change[5]=RemoteXY.Koridor;
change[6]=RemoteXY.Bath;
change[7]=RemoteXY.Vent;
change[8]=RemoteXY.Bath_Mirror;
change[9]=RemoteXY.NewYear;
change[10]=RemoteXY.Wall;
processRestoreRelay=0; CMn("Восстановление, не отсылаем состояние"); return;}
if(millis()-waitModeStartTime>200) CountDownСnox(200);
if(change[0]!=RemoteXY.Spot0) {change[0]=RemoteXY.Spot0; RelaySwitch(Pin_Spot0, RemoteXY.Spot0); CM("Зал споты 1 группа: "); CMn(RemoteXY.Spot0);}
if(change[1]!=RemoteXY.Spot1) {change[1]=RemoteXY.Spot1; RelaySwitch(Pin_Spot1, RemoteXY.Spot1); CM("Зал споты 2 группа: "); CMn(RemoteXY.Spot1);}
if(change[2]!=RemoteXY.Torsher) {change[2]=RemoteXY.Torsher; RelaySwitch(Pin_Torsher, RemoteXY.Torsher); CM("Розетка под торшер: "); CMn(RemoteXY.Torsher);}
if(change[3]!=RemoteXY.Kitchen_Work) {change[3]=RemoteXY.Kitchen_Work; RelaySwitch(Pin_Kitchen, RemoteXY.Kitchen_Work); CM("Кухня рабочая зона: "); CMn(RemoteXY.Kitchen_Work);}
if(change[4]!=RemoteXY.Bar) {change[4]=RemoteXY.Bar; RelaySwitch(Pin_Bar, RemoteXY.Bar); CM("Освещение барной стойки: "); CMn(RemoteXY.Bar);}
if(change[5]!=RemoteXY.Koridor) {change[5]=RemoteXY.Koridor; RelaySwitch(Pin_Koridor, RemoteXY.Koridor); CM("Корридор вход: "); CMn(RemoteXY.Koridor);}
if(change[6]!=RemoteXY.Bath) {change[6]=RemoteXY.Bath; RelaySwitch(Pin_BathRoom, RemoteXY.Bath); CM("Ванна яркий свет: "); CMn(RemoteXY.Bath);}
if(change[7]!=RemoteXY.Vent) {change[7]=RemoteXY.Vent; RelaySwitch(Pin_Vent, RemoteXY.Vent); CM("Вентиляция в ванной: "); CMn(RemoteXY.Vent);}
if(change[8]!=RemoteXY.Bath_Mirror) {change[8]=RemoteXY.Bath_Mirror; RelaySwitch(Pin_BathMirror, RemoteXY.Bath_Mirror); CM("Зеркало в ванной: "); CMn(RemoteXY.Bath_Mirror);}
if(change[9]!=RemoteXY.NewYear) {change[9]=RemoteXY.NewYear; RelaySwitch(Pin_NewYear, RemoteXY.NewYear); CM("Розетка над окнами: "); CMn(RemoteXY.NewYear);}
if(change[10]!=RemoteXY.Wall) {change[10]=RemoteXY.Wall; RelaySwitch(Pin_Wall, RemoteXY.Wall); CM("Подсветка на стене: "); CMn(RemoteXY.Wall);}}

//void voice() {static uint8_t num; if(num>122) return; if(num==122) {num++; CMn("Озвучены все фразы");} if(nextVoice) {num++; voice(num); nextVoice=0;}}

void voice(uint8_t ID=0) {if(ID) {CM("~~~~~~~> Sound ID "); CMn(ID);}
if(!RemoteXY.sound && !save.Sound && ID!=104) return; static uint8_t lastClick, lastDoubleClick; static uint32_t timer, voiceTime;
static boolean vClientConnected; static uint16_t lastID, dump;
if(ID && lastID==ID) {RemoteXY.sound=0; lastID=0;}
if(ID>=68 && ID<=93) RemoteXY.sound=0; else if(RemoteXY.sound && ID && ID!=200) {dump=ID; return;} else if(!ID && !RemoteXY.sound && dump) {ID=dump; dump=0;}
if(ID && lastID!=ID) {lastID=ID;
switch (ID) {
case 1: voiceTime=1165; break;
case 2: voiceTime=1193; break;
case 3: voiceTime=1660; break;
case 4: voiceTime=1793; break;
case 5: voiceTime=1765; break;
case 6: voiceTime=1886; break;
case 7: voiceTime=1973; break;
case 8: voiceTime=2060; break;
case 9: voiceTime=1568; break;
case 10: voiceTime=1660; break;
case 11: voiceTime=1707; break;
case 12: voiceTime=1822; break;
case 13: voiceTime=1770; break;
case 14: voiceTime=1944; break;
case 15: voiceTime=2285; break;
case 16: voiceTime=2378; break;
case 17: voiceTime=2083; break;
case 18: voiceTime=2083; break;
case 19: voiceTime=2135; break;
case 20: voiceTime=2119; break;
case 21: voiceTime=2002; break;
case 22: voiceTime=2118; break;
case 23: voiceTime=2633; break;
case 24: voiceTime=1232; break;
case 25: voiceTime=1307; break;
case 26: voiceTime=1932; break;
case 27: voiceTime=1504; break;
case 28: voiceTime=1527; break;
case 29: voiceTime=1603; break;
case 30: voiceTime=1665; break;
case 31: voiceTime=1736; break;
case 32: voiceTime=1817; break;
case 33: voiceTime=1863; break;
case 34: voiceTime=1979; break;
case 35: voiceTime=1707; break;
case 36: voiceTime=1875; break;
case 37: voiceTime=2447; break;
case 38: voiceTime=2581; break;
case 39: voiceTime=2268; break;
case 40: voiceTime=2309; break;
case 41: voiceTime=2054; break;
case 42: voiceTime=3611; break;
case 43: voiceTime=3813; break;
case 44: voiceTime=4424; break;
case 45: voiceTime=1721; break;
case 46: voiceTime=1354; break;
case 47: voiceTime=1514; break;
case 48: voiceTime=2740; break;
case 49: voiceTime=2785; break;
case 50: voiceTime=2211; break;
case 51: voiceTime=2166; break;
case 52: voiceTime=2546; break;
case 53: voiceTime=2175; break;
case 54: voiceTime=3070; break;
case 55: voiceTime=4299; break;
case 56: voiceTime=4102; break;
case 57: voiceTime=2638; break;
case 58: voiceTime=3384; break;
case 59: voiceTime=2147; break;
case 60: voiceTime=3139; break;
case 61: voiceTime=1784; break;
case 62: voiceTime=1615; break;
case 63: voiceTime=1869; break;
case 64: voiceTime=1119; break;
case 65: voiceTime=2114; break;
case 66: voiceTime=1645; break;
case 67: voiceTime=3121; break;
case 68: voiceTime=2447; break;
case 69: voiceTime=1720; break;
case 70: voiceTime=1732; break;
case 71: voiceTime=2016; break;
case 72: voiceTime=1819; break;
case 73: voiceTime=2420; break;
case 74: voiceTime=2344; break;
case 75: voiceTime=2766; break;
case 76: voiceTime=2607; break;
case 77: voiceTime=2288; break;
case 78: voiceTime=2715; break;
case 79: voiceTime=2485; break;
case 80: voiceTime=2028; break;
case 81: voiceTime=2739; break;
case 82: voiceTime=2500; break;
case 83: voiceTime=2876; break;
case 84: voiceTime=2551; break;
case 85: voiceTime=2425; break;
case 86: voiceTime=2751; break;
case 87: voiceTime=1947; break;
case 88: voiceTime=2350; break;
case 89: voiceTime=2324; break;
case 90: voiceTime=2380; break;
case 91: voiceTime=2443; break;
case 92: voiceTime=2061; break;
case 93: voiceTime=2589; break;
case 94: voiceTime=2285; break;
case 95: voiceTime=2536; break;
case 96: voiceTime=1391; break;
case 97: voiceTime=1511; break;
case 98: voiceTime=1403; break;
case 99: voiceTime=1702; break;
case 100: voiceTime=1360; break;
case 101: voiceTime=1597; break;
case 102: voiceTime=1513; break;
case 103: voiceTime=1530; break;
case 104: voiceTime=2050; break;
case 105: voiceTime=0; break;
case 106: voiceTime=1862; break;
case 107: voiceTime=2004; break;
case 108: voiceTime=1933; break;
case 109: voiceTime=2000; break;
case 110: voiceTime=2071; break;
case 111: voiceTime=2040; break;
case 112: voiceTime=2578; break;
case 113: voiceTime=2367; break;
case 114: voiceTime=2462; break;
case 115: voiceTime=115; break;
case 116: voiceTime=231; break;
case 117: voiceTime=1293; break;
case 118: voiceTime=115; break;
case 119: voiceTime=231; break;
case 120: voiceTime=1807; break;
case 121: voiceTime=2140; break;
case 122: voiceTime=1443; break;
case 123: voiceTime=2220; break;
case 124: voiceTime=1930; break;
case 125: voiceTime=2181; break;
case 126: voiceTime=1764; break;
case 127: voiceTime=1868; break;
case 128: voiceTime=2248; break;
case 129: voiceTime=2316; break;
case 130: voiceTime=2321; break;
case 131: voiceTime=2039; break;
case 132: voiceTime=2135; break;
case 133: voiceTime=1965; break;
case 134: voiceTime=1975; break;
case 135: voiceTime=1388; break;
case 200: RemoteXY.sound=0; voiceTime=0; ID=0; break;
default: voiceTime=5500; break;
} if(ID==115) {if(lastClick!=118) ID=118; lastClick=ID;} if(ID==116) {if(lastDoubleClick!=119) ID=119; lastDoubleClick=ID;} timer=millis();
RemoteXY.sound=ID; CM("Воспроизводим звук "); CMn(ID); if(vClientConnected!=ClientConnected) {if(ClientConnected) voiceTime+=2000; vClientConnected=ClientConnected;}}
if(RemoteXY.sound && voiceTime && millis()-timer>voiceTime+500) {RemoteXY.sound=0; voiceTime=0;}}

void deltaCalc(uint8_t module) {int16_t coltemp = 0;
for_i(0,3) {lVal[module].Led_Value_Fade_Start[i] = lVal[module].Led_Value_Old[i]; coltemp = abs(lVal[module].Led_Value[i] - lVal[module].Led_Value_Fade_Start[i]); 
if (lVal[module].steps < coltemp) lVal[module].steps = coltemp;} lVal[module].fadeCount = 0;
CM("Модуль"); CM(module); CM(" Old: "); for_i(0,3) {CM(lVal[module].Led_Value_Old[i]); CM(" ");}
CM(" New: "); for_i(0,3) {CM(lVal[module].Led_Value[i]); CM(" ");} CM("Шагов = "); CM(lVal[module].steps); CM(" Delta: ");
if (!lVal[module].steps) {CMn("Нечего менять, выходим"); return;}
for_i(0,3) {lVal[module].delta[i] = (abs(lVal[module].Led_Value[i] - lVal[module].Led_Value_Old[i]))*1000 / lVal[module].steps; 
CM(lVal[module].delta[i]); CM(" ");}
CMn(); changeColorProcess[module] = 1;}

void SetRGBLed() {static uint32_t fadeTimer; if(!AlarmMode) {if (millis() - fadeTimer < save.FadeDelay) return;} else 
{if (millis() - fadeTimer < 3) return;} fadeTimer = millis();
for_t(0,5) {if (changeColorProcess[t]) {//CM("RGB"); CM(t); CM(": "); for_i(0,3) {CM(lVal[t].Led_Value[i]); CM(" ");} CM(" <- ");
for_i(0,3) {if (lVal[t].Led_Value_Fade[i] < lVal[t].Led_Value[i]) lVal[t].Led_Value_Fade[i] = lVal[t].Led_Value_Fade_Start[i] + round(lVal[t].delta[i]*lVal[t].fadeCount/1000); 
else if (lVal[t].Led_Value_Fade[i] > lVal[t].Led_Value[i]) lVal[t].Led_Value_Fade[i] = lVal[t].Led_Value_Fade_Start[i] - round(lVal[t].delta[i]*lVal[t].fadeCount/1000);
if (lVal[t].Led_Value_Fade[i] > 255) lVal[t].Led_Value_Fade[i] = 255; if (lVal[t].Led_Value_Fade[i] < 0) lVal[t].Led_Value_Fade[i] = 0;
lVal[t].Led_Value_Old[i] = lVal[t].Led_Value_Fade[i]; ledColorRX[t][i]=lVal[t].Led_Value_Fade[i]; //CM(lVal[t].Led_Value_Fade[i]); CM(" ");
}
if(save.GammaCorrection) {leds[t].r=GammaCorrection::CorrectTable8(lVal[t].Led_Value_Fade[0]); leds[t].g=GammaCorrection::CorrectTable8(lVal[t].Led_Value_Fade[1]);
leds[t].b=GammaCorrection::CorrectTable8(lVal[t].Led_Value_Fade[2]);} else {leds[t].r=lVal[t].Led_Value_Fade[0]; leds[t].g=lVal[t].Led_Value_Fade[1];
leds[t].b=lVal[t].Led_Value_Fade[2];}
boolean alarm_proc=0; for_i(5,10) if(modeInd[i]==alarmInd) {alarm_proc=1; break;} if(alarm_proc) { // индикаторы на выключателях

if(save.GammaCorrection) {ledsw[t].r=GammaCorrection::CorrectTable8(lVal[t].Led_Value_Fade[0]); ledsw[t].g=GammaCorrection::CorrectTable8(lVal[t].Led_Value_Fade[1]);
ledsw[t].b=GammaCorrection::CorrectTable8(lVal[t].Led_Value_Fade[2]);} else {ledsw[t].r=lVal[t].Led_Value_Fade[0]; ledsw[t].g=lVal[t].Led_Value_Fade[1];
ledsw[t].b=lVal[t].Led_Value_Fade[2];}} FastLED.show(); //CMn();

lVal[t].fadeCount++;
if (lVal[t].fadeCount > lVal[t].steps) {changeColorProcess[t] = 0; lVal[t].steps = 0; 

for_i(0,3) {lVal[t].Led_Value_Old[i] = lVal[t].Led_Value[i]; ledColorRX[t][i]=lVal[t].Led_Value[i];
if(save.GammaCorrection) lVal[t].Led_Value[i]=GammaCorrection::CorrectTable8(lVal[t].Led_Value_Fade[i]);}


leds[t].r=lVal[t].Led_Value[0]; leds[t].g=lVal[t].Led_Value[1]; leds[t].b=lVal[t].Led_Value[2]; 
if(alarm_proc) {ledsw[t].r=lVal[t].Led_Value[0]; ledsw[t].g=lVal[t].Led_Value[1]; ledsw[t].b=lVal[t].Led_Value[2];} 
FastLED.show();}}}}


void autoOnLed() {boolean proc=0; for_i(5,10) if(modeInd[i]==autoIndOn) {proc=1; break;} if(!proc) return;
static boolean firstStart; static uint8_t rs, gs, bs, re, ge, be, rt, gt, bt, rw, gw, bw, count, mode; static uint16_t deltar, deltag, deltab; static uint32_t timer, waitTime, fadeTime;
rs=102; gs=153; bs=255; re=74; ge=106; be=171;
if (!firstStart) {deltar=abs(rs-re)*1000/255; deltag=abs(gs-ge)*1000/255; deltab=abs(bs-be)*1000/255; firstStart=1;}
if(millis()-timer<fadeTime) return; if(millis()-timer<waitTime) return; timer=millis(); waitTime=0;
if(!mode) count++; else count--; if(count==255) {mode=1; waitTime=500;} if(count==0) {mode=0; waitTime=85;} if(count<170) fadeTime=5; else if (count>170) fadeTime=8; else fadeTime=7;
rs>re ? rt=GammaCorrection::CorrectTable8(round(re+deltar*count/1000)) : rt=GammaCorrection::CorrectTable8(round(rs+deltar*count/1000));
gs>re ? gt=GammaCorrection::CorrectTable8(round(ge+deltag*count/1000)) : gt=GammaCorrection::CorrectTable8(round(gs+deltag*count/1000));
bs>be ? bt=GammaCorrection::CorrectTable8(round(be+deltab*count/1000)) : bt=GammaCorrection::CorrectTable8(round(bs+deltab*count/1000));
for_i(5,10) if(modeInd[i]==autoIndOn && modeInd[i]!=syncInd) {ledsw[i-5].r=rt; ledsw[i-5].g=gt; ledsw[i-5].b=bt;} if(rw!=rt || gw!=gt || bw!=bt) {rw=rt; gw=gt; bw=bt; FastLED.show();}}

void autoOffLed() {boolean proc=0; for_i(5,10) if(modeInd[i]==autoIndOff) {proc=1; break;} if(!proc) return;
static boolean firstStart; static uint8_t rs, gs, bs, re, ge, be, rt, gt, bt, rw, gw, bw, count, mode; static uint16_t deltar, deltag, deltab; static uint32_t timer, waitTime, fadeTime;
rs=252; gs=108; bs=15; re=168; ge=74; be=12;
if (!firstStart) {deltar=abs(rs-re)*1000/255; deltag=abs(gs-ge)*1000/255; deltab=abs(bs-be)*1000/255; firstStart=1;}
if(millis()-timer<fadeTime) return; if(millis()-timer<waitTime) return; timer=millis(); waitTime=0;
if(!mode) count++; else count--; if(count==255) {mode=1; waitTime=500;} if(count==0) {mode=0; waitTime=85;} if(count<170) fadeTime=5; else if (count>170) fadeTime=8; else fadeTime=7;
rs>re ? rt=GammaCorrection::CorrectTable8(round(re+deltar*count/1000)) : rt=GammaCorrection::CorrectTable8(round(rs+deltar*count/1000));
gs>re ? gt=GammaCorrection::CorrectTable8(round(ge+deltag*count/1000)) : gt=GammaCorrection::CorrectTable8(round(gs+deltag*count/1000));
bs>be ? bt=GammaCorrection::CorrectTable8(round(be+deltab*count/1000)) : bt=GammaCorrection::CorrectTable8(round(bs+deltab*count/1000));
for_i(5,10) if(modeInd[i]==autoIndOff && modeInd[i]!=syncInd) {ledsw[i-5].r=rt; ledsw[i-5].g=gt; ledsw[i-5].b=bt;} if(rw!=rt || gw!=gt || bw!=bt) {rw=rt; gw=gt; bw=bt; FastLED.show();}}

void cnoxLed() {boolean proc=0; for_i(5,10) if(modeInd[i]==cnoxInd) {proc=1; break;} if(!proc) return;
static boolean firstStart; static uint8_t rs, gs, bs, re, ge, be, rt, gt, bt, rw, gw, bw, count, mode; static uint16_t deltar, deltag, deltab; static uint32_t timer, waitTime, fadeTime;
rs=208; gs=255; bs=0; re=73; ge=89; be=0;
if (!firstStart) {deltar=abs(rs-re)*1000/255; deltag=abs(gs-ge)*1000/255; deltab=abs(bs-be)*1000/255; firstStart=1;}
if(millis()-timer<fadeTime) return; if(millis()-timer<waitTime) return; timer=millis(); waitTime=0;
if(!mode) count++; else count--; if(count==255) {mode=1; waitTime=500;} if(count==0) {mode=0; waitTime=85;} if(count<170) fadeTime=5; else if (count>170) fadeTime=8; else fadeTime=7;
rs>re ? rt=GammaCorrection::CorrectTable8(round(re+deltar*count/1000)) : rt=GammaCorrection::CorrectTable8(round(rs+deltar*count/1000));
gs>re ? gt=GammaCorrection::CorrectTable8(round(ge+deltag*count/1000)) : gt=GammaCorrection::CorrectTable8(round(gs+deltag*count/1000));
bs>be ? bt=GammaCorrection::CorrectTable8(round(be+deltab*count/1000)) : bt=GammaCorrection::CorrectTable8(round(bs+deltab*count/1000));
for_i(5,10) if(modeInd[i]==cnoxInd && modeInd[i]!=syncInd) {ledsw[i-5].r=rt; ledsw[i-5].g=gt; ledsw[i-5].b=bt;} if(rw!=rt || gw!=gt || bw!=bt) {rw=rt; gw=gt; bw=bt; FastLED.show();}}

void waitLed() {boolean proc=0; for_i(5,10) if(modeInd[i]==waitInd) {proc=1; break;} if(!proc) return;
static boolean firstStart; static uint8_t rs, gs, bs, re, ge, be, rt, gt, bt, rw, gw, bw, count, mode; static uint16_t deltar, deltag, deltab; static uint32_t timer, waitTime, fadeTime;
rs=255; gs=0; bs=179; re=168; ge=0; be=118;
if (!firstStart) {deltar=abs(rs-re)*1000/255; deltag=abs(gs-ge)*1000/255; deltab=abs(bs-be)*1000/255; firstStart=1;}
if(millis()-timer<fadeTime) return; if(millis()-timer<waitTime) return; timer=millis(); waitTime=0;
if(!mode) count++; else count--; if(count==255) {mode=1; waitTime=500;} if(count==0) {mode=0; waitTime=85;} if(count<170) fadeTime=5; else if (count>170) fadeTime=8; else fadeTime=7;
rs>re ? rt=GammaCorrection::CorrectTable8(round(re+deltar*count/1000)) : rt=GammaCorrection::CorrectTable8(round(rs+deltar*count/1000));
gs>re ? gt=GammaCorrection::CorrectTable8(round(ge+deltag*count/1000)) : gt=GammaCorrection::CorrectTable8(round(gs+deltag*count/1000));
bs>be ? bt=GammaCorrection::CorrectTable8(round(be+deltab*count/1000)) : bt=GammaCorrection::CorrectTable8(round(bs+deltab*count/1000));
for_i(5,10) if(modeInd[i]==waitInd && modeInd[i]!=syncInd) {ledsw[i-5].r=rt; ledsw[i-5].g=gt; ledsw[i-5].b=bt;} if(rw!=rt || gw!=gt || bw!=bt) {rw=rt; gw=gt; bw=bt; FastLED.show();}}

void timerLed() {boolean proc=0; for_i(5,10) if(modeInd[i]==timerInd) {proc=1; break;} if(!proc) return;
static boolean firstStart, next; static uint8_t rs, gs, bs, re, ge, be, rt, gt, bt, rw, gw, bw, count, mode; static uint16_t deltar, deltag, deltab; static uint32_t timer, waitTime, fadeTime;
rs=0; gs=255; bs=0; re=0; be=0;
if (!firstStart) {deltar=abs(rs-re)*1000/255; deltab=abs(bs-be)*1000/255; next=0; firstStart=1;}
if(millis()-timer<fadeTime) return; if(millis()-timer<waitTime) return; timer=millis(); waitTime=0;
if(!mode) count++; else count--; if(count==255) {mode=1;  waitTime=70;} if(count==0) {mode=0; next ? waitTime=500 : waitTime=70; 
next ? ge=0 : ge=89; deltag=abs(gs-ge)*1000/255; next=!next;} if(count<85) fadeTime=0; else if (count>170) fadeTime=2; else fadeTime=1;
rs>re ? rt=GammaCorrection::CorrectTable8(round(re+deltar*count/1000)) : rt=GammaCorrection::CorrectTable8(round(rs+deltar*count/1000));
gs>re ? gt=GammaCorrection::CorrectTable8(round(ge+deltag*count/1000)) : gt=GammaCorrection::CorrectTable8(round(gs+deltag*count/1000));
bs>be ? bt=GammaCorrection::CorrectTable8(round(be+deltab*count/1000)) : bt=GammaCorrection::CorrectTable8(round(bs+deltab*count/1000));
for_i(5,10) if(modeInd[i]==timerInd && modeInd[i]!=syncInd) {ledsw[i-5].r=rt; ledsw[i-5].g=gt; ledsw[i-5].b=bt;} if(rw!=rt || gw!=gt || bw!=bt) {rw=rt; gw=gt; bw=bt; FastLED.show();}}

void blockLed() {boolean proc=0; for_i(5,10) if(modeInd[i]==blockInd) {proc=1; break;} if(!proc) return;
static boolean firstStart; static uint8_t rs, gs, bs, re, ge, be, rt, gt, bt, rw, gw, bw, count, mode; static uint16_t deltar, deltag, deltab; static uint32_t timer, waitTime, fadeTime;
rs=255; gs=0; bs=0; re=90; ge=0; be=0;
if (!firstStart) {deltar=abs(rs-re)*1000/255; deltag=abs(gs-ge)*1000/255; deltab=abs(bs-be)*1000/255; firstStart=1;}
if(millis()-timer<fadeTime) return; if(millis()-timer<waitTime) return; timer=millis(); waitTime=0;
if(!mode) count++; else count--; if(count==255) {mode=1; waitTime=500;} if(count==0) {mode=0; waitTime=85;} if(count<170) fadeTime=5; else if (count>170) fadeTime=8; else fadeTime=7;
rs>re ? rt=GammaCorrection::CorrectTable8(round(re+deltar*count/1000)) : rt=GammaCorrection::CorrectTable8(round(rs+deltar*count/1000));
gs>re ? gt=GammaCorrection::CorrectTable8(round(ge+deltag*count/1000)) : gt=GammaCorrection::CorrectTable8(round(gs+deltag*count/1000));
bs>be ? bt=GammaCorrection::CorrectTable8(round(be+deltab*count/1000)) : bt=GammaCorrection::CorrectTable8(round(bs+deltab*count/1000));
for_i(5,10) if(modeInd[i]==blockInd && modeInd[i]!=syncInd) {ledsw[i-5].r=rt; ledsw[i-5].g=gt; ledsw[i-5].b=bt;} if(rw!=rt || gw!=gt || bw!=bt) {rw=rt; gw=gt; bw=bt; FastLED.show();}}

void autoOnNightLed() {static boolean proc; if(!proc) returnMS(100); proc=0; for_i(5,10) if(modeInd[i]==autoOnNight) {proc=1; break;} if(!proc) return;
static boolean firstStart; static uint8_t rs, gs, bs, re, ge, be, rt, gt, bt, rw, gw, bw, count, mode; static uint16_t deltar, deltag, deltab; static uint32_t timer, waitTime, fadeTime;
rs=40; gs=59; bs=99; re=24; ge=37; be=61;
if (!firstStart) {deltar=abs(rs-re)*1000/255; deltag=abs(gs-ge)*1000/255; deltab=abs(bs-be)*1000/255; firstStart=1;}
if(millis()-timer<fadeTime) return; if(millis()-timer<waitTime) return; timer=millis(); waitTime=0;
if(!mode) count++; else count--; if(count==255) {mode=1; waitTime=500;} if(count==0) {mode=0; waitTime=85;} if(count<170) fadeTime=5; else if (count>170) fadeTime=8; else fadeTime=7;
rs>re ? rt=GammaCorrection::CorrectTable8(round(re+deltar*count/1000)) : rt=GammaCorrection::CorrectTable8(round(rs+deltar*count/1000));
gs>re ? gt=GammaCorrection::CorrectTable8(round(ge+deltag*count/1000)) : gt=GammaCorrection::CorrectTable8(round(gs+deltag*count/1000));
bs>be ? bt=GammaCorrection::CorrectTable8(round(be+deltab*count/1000)) : bt=GammaCorrection::CorrectTable8(round(bs+deltab*count/1000));
for_i(5,10) if(modeInd[i]==autoOnNight && modeInd[i]!=syncInd) {ledsw[i-5].r=rt; ledsw[i-5].g=gt; ledsw[i-5].b=bt;} if(rw!=rt || gw!=gt || bw!=bt) {rw=rt; gw=gt; bw=bt; FastLED.show();}}

void autoOffNightLed() {static boolean proc; if(!proc) returnMS(100); proc=0; for_i(5,10) if(modeInd[i]==autoOffNight) {proc=1; break;} if(!proc) return;
static boolean firstStart; static uint8_t rs, gs, bs, re, ge, be, rt, gt, bt, rw, gw, bw, count, mode; static uint16_t deltar, deltag, deltab; static uint32_t timer, waitTime, fadeTime;
rs=89; gs=55; bs=0; re=64; ge=42; be=0;
if (!firstStart) {deltar=abs(rs-re)*1000/255; deltag=abs(gs-ge)*1000/255; deltab=abs(bs-be)*1000/255; firstStart=1;}
if(millis()-timer<fadeTime) return; if(millis()-timer<waitTime) return; timer=millis(); waitTime=0;
if(!mode) count++; else count--; if(count==255) {mode=1; waitTime=500;} if(count==0) {mode=0; waitTime=85;} if(count<170) fadeTime=5; else if (count>170) fadeTime=8; else fadeTime=7;
rs>re ? rt=GammaCorrection::CorrectTable8(round(re+deltar*count/1000)) : rt=GammaCorrection::CorrectTable8(round(rs+deltar*count/1000));
gs>re ? gt=GammaCorrection::CorrectTable8(round(ge+deltag*count/1000)) : gt=GammaCorrection::CorrectTable8(round(gs+deltag*count/1000));
bs>be ? bt=GammaCorrection::CorrectTable8(round(be+deltab*count/1000)) : bt=GammaCorrection::CorrectTable8(round(bs+deltab*count/1000));
for_i(5,10) if(modeInd[i]==autoOffNight && modeInd[i]!=syncInd) {ledsw[i-5].r=rt; ledsw[i-5].g=gt; ledsw[i-5].b=bt;} if(rw!=rt || gw!=gt || bw!=bt) {rw=rt; gw=gt; bw=bt; FastLED.show();}}

void syncIndikatorLed() {static boolean proc; proc=0; for_i(5,10) if(modeInd[i]==syncInd) {proc=1; break;} if(!proc) return;
static boolean led[5]; static uint8_t mode[5];
for_i(0,5) if(modeInd[i+5]==syncInd && !led[i]) {led[i]=1;} //CMn("Sync start");}
static uint32_t timeLag; if(millis()-timeLag < 50) return; timeLag=millis();
for_i(0,5) if(led[i]) {switch (mode[i]) {
case 1: case 3: case 5: ledsw[i]=CRGB::Black; break;
case 0: ledsw[i]=CRGB::Green; break;
case 2: ledsw[i]=CRGB::Purple; break;
case 4: ledsw[i]=CRGB::Blue; break;} mode[i]++;
if(mode[i]>6) {led[i]=0; mode[i]=0; syncLedInd[i+5]=0;}}} // CM("Sync "); CM(i+5); CMn(" end");

void programLed() {boolean proc=0; for_i(5,10) if(modeInd[i]==progInd) {proc=1; break;} if(!proc) return;
static uint32_t timer, timeLag; if(millis()-timer<timeLag) return; timer=millis(); static uint8_t mode;
switch (mode) {
case 0: timeLag=30; break;
case 2: timeLag=70; break;
case 3: timeLag=30; break;
case 4: timeLag=400; break;
case 5: timeLag=30; break;
case 6: timeLag=70; break;
case 7: timeLag=30; break;
case 8: timeLag=400; break;}
for_i(5,10) switch (mode) {
case 0: case 3: ledsw[i-5].setRGB(85,255,0); break;
case 2: case 4: case 6: case 8: ledsw[i-5]=CRGB::Black; break;
case 5: case 7: ledsw[i-5]=CRGB::Purple; break;}
FastLED.show(); mode++; if(mode>8) mode=0;}

void Alarm(uint8_t alarm=2) {
if(!waitMode) return; if(alarm!=1 && !AlarmMode) return; static boolean redOn, alarmOn; static uint32_t timer, waitTime; static uint8_t saveInd[10];
if(!AlarmMode && !alarmOn && alarm==1) {CMn("Alarm Mode ON"); alarmOn=1; AlarmMode=1; redOn=1; waitTime=0; RemoteXY.Alarm_led1=1; RemoteXY.Alarm_led2=1;
for_i(5,10) {saveInd[i]=modeInd[i]; CM(i); CM("-"); CM(saveInd[i]); CM(" ");} for_i(5,10) alarmLedInd[i]=1;}
if(AlarmMode && alarmOn && !alarm) {CMn("Alarm Mode ON"); alarmOn=0;}
if(millis()-timer<waitTime) return; timer=millis();
for_i(0,6) {if(!redOn) {lVal[i].Led_Value[0]=0; lVal[i].Led_Value[1]=0; lVal[i].Led_Value[2]=0;}
else {lVal[i].Led_Value[0]=255; lVal[i].Led_Value[1]=0; lVal[i].Led_Value[2]=0;} deltaCalc(i);}
redOn=!redOn; if(redOn) waitTime=750; else waitTime=1200; if(alarmOn) return;
RemoteXY.Alarm_led1=0; RemoteXY.Alarm_led2=0;
for_i(0,6) {lVal[i].Led_Value[0]=save.RGB_r[i]; lVal[i].Led_Value[1]=save.RGB_g[i]; lVal[i].Led_Value[2]=save.RGB_b[i];}
if(!RemoteXY.Mirror_RGB) {for_i(0,3) lVal[0].Led_Value[i]=0; CMn("Зеркало RGB OFF");} else CMn("Зеркало RGB ON");
if(!RemoteXY.Window_RGB) {for_i(0,3) lVal[1].Led_Value[i]=0; CMn("Окно OFF");} else CMn("Окно RGB ON");
if(!RemoteXY.Kitchen_RGB) {for_i(0,3) lVal[2].Led_Value[i]=0; CMn("Кухня RGB OFF");} else CMn("Кухня RGB ON");
if(!RemoteXY.Bar_RGB) {for_i(0,3) lVal[3].Led_Value[i]=0; CMn("Бар RGB OFF");} else CMn("Бар RGB ON");
if(!RemoteXY.Bath_RGB) {for_i(0,3) lVal[4].Led_Value[i]=0; CMn("Ванна RGB OFF");} else CMn("Ванна RGB ON");
AlarmMode=0; for_i(5,10) alarmLedInd[i]=0; CMn("Alarm Mode OFF"); for_i(0,5) deltaCalc(i);}

void selectInd() {static uint8_t IndikatorMode, IndikatorMode1;

#if defined DebagEnable
static boolean change, sInd[10], nightMD, cnx, wat; static modeLed mInd[10];
change=0; for_i(5,10) if(mInd[i]!=modeInd[i]) {change=1; mInd[i]=modeInd[i];} if(change) {change=0; CM("ModeInd: "); for_i(5,10) {CM(modeInd[i]); CM(" ");} CMn();}
change=0; for_i(5,10) if(sInd[i]!=syncLedInd[i]) {change=1; sInd[i]=syncLedInd[i]; break;} if(change) {change=0; CM("SyncInd: "); for_i(5,10) {CM(syncLedInd[i]); CM(" ");} CMn();}
if(nightMD!=nigtMode) {nightMD=nigtMode; CM("Night Mode: "); CMn(nigtMode ? "On" : "Off");}
if(cnx!=cnox) {cnx=cnox; CM("cnox: "); CMn(cnx!=cnox);}
if(wat!=RemoteXY.WaitMode) {wat=RemoteXY.WaitMode; CM("WaitMode: "); CMn(RemoteXY.WaitMode);}
#endif

if(cnox) IndikatorMode=6; else if(timerKoridor) IndikatorMode=3; else if(saveChange) IndikatorMode=4; else if(RemoteXY.WaitMode) IndikatorMode=5;
else if(!RemoteXY.AutoMode_switch) IndikatorMode=2; else if(RemoteXY.AutoMode_switch) IndikatorMode=1; switch (IndikatorMode) {
case 1: if(RemoteXY.Mode_led!=1) RemoteXY.Mode_led=1; break;
case 2: if(RemoteXY.Mode_led!=2) RemoteXY.Mode_led=2; break;
case 3: if(RemoteXY.Mode_led!=3) RemoteXY.Mode_led=3; if(RemoteXY.Koridor_led!=2) RemoteXY.Koridor_led=2; break;
case 4: if(RemoteXY.Mode_led!=4) RemoteXY.Mode_led=4; break;
case 5: if(RemoteXY.Mode_led!=5) RemoteXY.Mode_led=5; break;
case 6: if(RemoteXY.Mode_led!=6) RemoteXY.Mode_led=6; break;}
if(nigtMode) {RemoteXY.AutoMode_switch ? IndikatorMode1=1 : IndikatorMode1=2;} else IndikatorMode1=0; switch(IndikatorMode1) {
case 0: if(RemoteXY.Mode_led1) RemoteXY.Mode_led1=0; break;
case 1: if(RemoteXY.Mode_led1!=1) RemoteXY.Mode_led1=1; break;
case 2: if(RemoteXY.Mode_led1!=2) RemoteXY.Mode_led1=2; break;}

if(RemoteXY.Koridor_led==2 && !timerKoridor) RemoteXY.Koridor_led=1;

for_i(5,10) {if(nigtMode) {autoLedOn[i]=0; autoLedOff[i]=0; if(save.autoMode) {autoOnLedNight[i]=1; autoOffLedNight[i]=0;} else {autoOnLedNight[i]=0; autoOffLedNight[i]=1;}} else
{autoOnLedNight[i]=0; autoOffLedNight[i]=0; if(save.autoMode) {autoLedOn[i]=1; autoLedOff[i]=0;} else {autoLedOn[i]=0; autoLedOff[i]=1;}}}

for_i(5,10) {if(syncLedInd[i]) modeInd[i]=syncInd; else if(progLedInd[i]) modeInd[i]=progInd; else if(alarmLedInd[i]) {modeInd[i]=alarmInd;} else if(cnoxLedInd[i]) modeInd[i]=cnoxInd;
else if(timerLedInd[i]) modeInd[i]=timerInd; else if(waitLedInd[i]) modeInd[i]=waitInd; else if(blockLedInd[i]) modeInd[i]=blockInd; else {
if(nigtMode) {RemoteXY.AutoMode_switch ? modeInd[i]=autoOnNight : modeInd[i]=autoOffNight;} else {RemoteXY.AutoMode_switch ? modeInd[i]=autoIndOn : modeInd[i]=autoIndOff;}}}}

void RGB_PowerOn(uint8_t module) {if(save.RGB_ModulePower[module]) return; save.RGB_ModulePower[module]=1;
CM("Power: ON RGB module "); CMn(module); if(millis()-waitModeStartTime>200) {CountDownСnox(200); CMn("Переключение RGB, снимаем cnox");}
lVal[module].Led_Value[0] = save.RGB_r[module];
lVal[module].Led_Value[1] = save.RGB_g[module];
lVal[module].Led_Value[2] = save.RGB_b[module];
if(!lVal[module].Led_Value[0] && !lVal[module].Led_Value[1] && !lVal[module].Led_Value[2]) {
RemoteXY.Set_rgb_r=135; RemoteXY.Set_rgb_g=0; RemoteXY.Set_rgb_b=145; 
save.RGB_r[module]=RemoteXY.Set_rgb_r; save.RGB_g[module]=RemoteXY.Set_rgb_g; save.RGB_b[module]=RemoteXY.Set_rgb_b;
save.RGB_r[module]=RemoteXY.Set_rgb_r; lVal[module].Led_Value[0]=RemoteXY.Set_rgb_r;
save.RGB_g[module]=RemoteXY.Set_rgb_g; lVal[module].Led_Value[1]=RemoteXY.Set_rgb_g;
save.RGB_b[module]=RemoteXY.Set_rgb_b; lVal[module].Led_Value[2]=RemoteXY.Set_rgb_b;}
deltaCalc(module);}

void RGB_PowerOff(uint8_t module) {if(!save.RGB_ModulePower[module]) return; save.RGB_ModulePower[module]=0;
CM("Power: OFF RGB module "); CMn(module); if(millis()-waitModeStartTime>200) {CountDownСnox(200); CMn("Переключение RGB, снимаем cnox");}
lVal[module].Led_Value[0] = 0;
lVal[module].Led_Value[1] = 0;
lVal[module].Led_Value[2] = 0;
deltaCalc(module);}

boolean FIXLight(uint8_t in=0, uint8_t button=0) { // 1-фикс прежнего, 2-фикс изменения 3-восстановить кроме ванны 4-фикс принудительный 5-статус 6-восстановить всё 
// 7-обнулить фиксы 8-фикс ванны 9-восстановить ванну|| возврат 1 - можно восстановить, 2 - нельзя
static boolean sceneAfter;                                 // сцена света после зафиксирована
static boolean Koridor[2], f_Koridor[2];                   // состояние яркого света в корридоре
static boolean Mirror_RGB[2], f_Mirror_RGB[2];             // состояние RGB подсветки зеркала в коридоре
static boolean Spot0[2], f_Spot0[2];                       // состояние Спотов 1
static boolean Spot1[2], f_Spot1[2];                       // состояние Спотов 2
static boolean Window_RGB[2], f_Window_RGB[2];             // состояние RGB подсветки окон
static boolean Torsher[2], f_Torsher[2];                   // состояние Торшера
static boolean Wall[2], f_Wall[2];                         // состояние подсветки на стене
static boolean NewYear[2], f_NewYear[2];                   // состояние розеток вверху окон
static boolean Kitchen_Work[2], f_Kitchen_Work[2];         // состояние рабочей зоны кухни
static boolean Kitchen_RGB[2], f_Kitchen_RGB[2];           // состояние RGB подсветки кухни
static boolean Bar[2], f_Bar[2];                           // состояние ламп над барной стойкой
static boolean Bar_RGB[2], f_Bar_RGB[2];                   // состояние RGB подсветки барной стойки
static boolean Bath[2], f_Bath[2];                         // состояние освещения в ванне
static boolean Bath_Mirror[2], f_Bath_Mirror[2];           // состояние подсветки зеркала в ванне
static boolean Bath_RGB[2], f_Bath_RGB[2];                 // состояние RGB подсветки в ванне
static boolean stateFIX[2];                                // состояние зафиксировано
static uint8_t lastFix;                                    // запоминаем текущее состояние после сохранения прежнего

//if(button) {CM("----- > Button FIX act: "); CM(in); CM(" button: "); CMn(button);}

switch(in) {
case 0:
for_i(0,2) {if(stateFIX[i] && (
Koridor[i]!=RemoteXY.Koridor ||
Mirror_RGB[i]!=RemoteXY.Mirror_RGB ||
Spot0[i]!=RemoteXY.Spot0 ||
Spot1[i]!=RemoteXY.Spot1 ||
Window_RGB[i]!=RemoteXY.Window_RGB ||
Torsher[i]!=RemoteXY.Torsher ||
Wall[i]!=RemoteXY.Wall ||
NewYear[i]!=RemoteXY.NewYear ||
Kitchen_Work[i]!=RemoteXY.Kitchen_Work ||
Kitchen_RGB[i]!=RemoteXY.Kitchen_RGB ||
Bar[i]!=RemoteXY.Bar ||
Bar_RGB[i]!=RemoteXY.Bar_RGB ||
Bath[i]!=RemoteXY.Bath || 
Bath_Mirror[i]!=RemoteXY.Bath_Mirror ||
Bath_RGB[i]!=RemoteXY.Bath_RGB))
{stateFIX[i]=0; CM("xxx ----- FIX "); CM(i); CMn(" нарушен");}}
break;
case 1: sceneAfter=1;
f_Koridor[button]=RemoteXY.Koridor;
f_Mirror_RGB[button]=RemoteXY.Mirror_RGB;
f_Spot0[button]=RemoteXY.Spot0;
f_Spot1[button]=RemoteXY.Spot1;
f_Window_RGB[button]=RemoteXY.Window_RGB;
f_Torsher[button]=RemoteXY.Torsher;
f_Wall[button]=RemoteXY.Wall;
f_NewYear[button]=RemoteXY.NewYear;
f_Kitchen_Work[button]=RemoteXY.Kitchen_Work;
f_Kitchen_RGB[button]=RemoteXY.Kitchen_RGB;
f_Bar[button]=RemoteXY.Bar;
f_Bar_RGB[button]=RemoteXY.Bar_RGB; lastFix=button; CM("ооо ------ FIX "); CM(lastFix); CMn(" прежнего сост");
f_Bath[button]=RemoteXY.Bath;
f_Bath_Mirror[button]=RemoteXY.Bath_Mirror;
f_Bath_RGB[button]=RemoteXY.Bath_RGB;
break;
case 2: if(sceneAfter) {sceneAfter=0;
Koridor[lastFix]=RemoteXY.Koridor;
Mirror_RGB[lastFix]=RemoteXY.Mirror_RGB;
Spot0[lastFix]=RemoteXY.Spot0;
Spot1[lastFix]=RemoteXY.Spot1;
Window_RGB[lastFix]=RemoteXY.Window_RGB;
Torsher[lastFix]=RemoteXY.Torsher;
Wall[lastFix]=RemoteXY.Wall;
NewYear[lastFix]=RemoteXY.NewYear;
Kitchen_Work[lastFix]=RemoteXY.Kitchen_Work;
Kitchen_RGB[lastFix]=RemoteXY.Kitchen_RGB;
Bar[lastFix]=RemoteXY.Bar;
Bar_RGB[lastFix]=RemoteXY.Bar_RGB;
Bath[lastFix]=RemoteXY.Bath;
Bath_Mirror[lastFix]=RemoteXY.Bath_Mirror;
Bath_RGB[lastFix]=RemoteXY.Bath_RGB; CM("+++ ----- FIX "); CM(lastFix); CMn(" текущего сост");
stateFIX[lastFix]=1;} break;
case 4: 
Koridor[button]=RemoteXY.Koridor;
Mirror_RGB[button]=RemoteXY.Mirror_RGB;
Spot0[button]=RemoteXY.Spot0;
Spot1[button]=RemoteXY.Spot1;
Window_RGB[button]=RemoteXY.Window_RGB;
Torsher[button]=RemoteXY.Torsher;
Wall[button]=RemoteXY.Wall;
NewYear[button]=RemoteXY.NewYear;
Kitchen_Work[button]=RemoteXY.Kitchen_Work;
Kitchen_RGB[button]=RemoteXY.Kitchen_RGB;
Bar[button]=RemoteXY.Bar;
Bar_RGB[button]=RemoteXY.Bar_RGB;
Bath[button]=RemoteXY.Bath;
Bath_Mirror[button]=RemoteXY.Bath_Mirror;
Bath_RGB[button]=RemoteXY.Bath_RGB; CM("### ----- принудительный FIX "); CM(button); CMn(" текущего сост");
stateFIX[button]=1; break;
case 3: if(!stateFIX[button]) {CM("*** ----- FIX "); CM(button); CMn(" не восстановить"); return(0);} else {
RemoteXY.Koridor=f_Koridor[button];
RemoteXY.Mirror_RGB=f_Mirror_RGB[button];
RemoteXY.Spot0=f_Spot0[button];
RemoteXY.Spot1=f_Spot1[button];
RemoteXY.Window_RGB=f_Window_RGB[button];
RemoteXY.Torsher=f_Torsher[button];
RemoteXY.Wall=f_Wall[button];
RemoteXY.NewYear=f_NewYear[button];
RemoteXY.Kitchen_Work=f_Kitchen_Work[button];
RemoteXY.Kitchen_RGB=f_Kitchen_RGB[button];
RemoteXY.Bar=f_Bar[button];
RemoteXY.Bar_RGB=f_Bar_RGB[button];
stateFIX[button]=0; CM("*** ----- FIX "); CM(button); CMn(" восстановление");
sceneRestore=1; return(1);} break;
case 6: if(!stateFIX[button]) {CM("@@@ ----- FIX "); CM(button); CMn(" всё не восстановить"); return(0);} else {
RemoteXY.Koridor=f_Koridor[button];
RemoteXY.Mirror_RGB=f_Mirror_RGB[button];
RemoteXY.Spot0=f_Spot0[button];
RemoteXY.Spot1=f_Spot1[button];
RemoteXY.Window_RGB=f_Window_RGB[button];
RemoteXY.Torsher=f_Torsher[button];
RemoteXY.Wall=f_Wall[button];
RemoteXY.NewYear=f_NewYear[button];
RemoteXY.Kitchen_Work=f_Kitchen_Work[button];
RemoteXY.Kitchen_RGB=f_Kitchen_RGB[button];
RemoteXY.Bar=f_Bar[button];
RemoteXY.Bar_RGB=f_Bar_RGB[button];
RemoteXY.Bath=f_Bath[button];
RemoteXY.Bath_Mirror=f_Bath_Mirror[button];
RemoteXY.Bath_RGB=f_Bath_RGB[button];
stateFIX[button]=0; CM("@@@ ----- FIX "); CM(button); CMn(" всё - восстановление");
sceneRestore=1; return(1);} break;
case 8: if(!stateFIX[button]) {CM("ванна --- FIX "); CM(button); CMn(" не восстановить"); return(0);} else {
RemoteXY.Bath=f_Bath[button];
RemoteXY.Bath_Mirror=f_Bath_Mirror[button];
RemoteXY.Bath_RGB=f_Bath_RGB[button];
stateFIX[button]=0; CM("ванна --- FIX "); CM(button); CMn(" восстановление");
sceneRestore=1; return(1);} break;
case 5: CM("!!! ----- состояние FIX "); CM(button);
if(stateFIX[button]) {CMn(" зафиксировано"); return(1);} else {CMn(" не зафиксировано"); return(0);} break;
case 7: for_i(0,2) stateFIX[i]=0; CM("XXX ----- FIX сохранения очищены"); break;} return(0);}

boolean FIXLightWithoutBath(uint8_t in=0, uint8_t button=0) { // 1-фикс прежнего, 2-фикс изменения 3-восстановить кроме ванны 4-фикс принудительный 5-статус 
// 7-обнулить фиксы 8-фикс ванны 9-восстановить ванну|| возврат 1 - можно восстановить, 2 - нельзя
static boolean sceneAfter;                             // сцена света после зафиксирована
static boolean Koridor[13], f_Koridor[13];               // состояние яркого света в корридоре
static boolean Mirror_RGB[13], f_Mirror_RGB[13];         // состояние RGB подсветки зеркала в коридоре
static boolean Spot0[13], f_Spot0[13];                   // состояние Спотов 1
static boolean Spot1[13], f_Spot1[13];                   // состояние Спотов 2
static boolean Window_RGB[13], f_Window_RGB[13];         // состояние RGB подсветки окон
static boolean Torsher[13], f_Torsher[13];               // состояние Торшера
static boolean Wall[13], f_Wall[13];                     // состояние подсветки на стене
static boolean NewYear[13], f_NewYear[13];               // состояние розеток вверху окон
static boolean Kitchen_Work[13], f_Kitchen_Work[13];     // состояние рабочей зоны кухни
static boolean Kitchen_RGB[13], f_Kitchen_RGB[13];       // состояние RGB подсветки кухни
static boolean Bar[13], f_Bar[13];                       // состояние ламп над барной стойкой
static boolean Bar_RGB[13], f_Bar_RGB[13];               // состояние RGB подсветки барной стойки
static boolean stateFIX[13];                            // состояние зафиксировано
static uint8_t lastFix;                                // запоминаем текущее состояние после сохранения прежнего

//if(button) {CM("----- > Button FIX act: "); CM(in); CM(" button: "); CMn(button);}

switch(in) {
case 0:
for_i(0,13) {if(stateFIX[i] && (
Koridor[i]!=RemoteXY.Koridor ||
Mirror_RGB[i]!=RemoteXY.Mirror_RGB ||
Spot0[i]!=RemoteXY.Spot0 ||
Spot1[i]!=RemoteXY.Spot1 ||
Window_RGB[i]!=RemoteXY.Window_RGB ||
Torsher[i]!=RemoteXY.Torsher ||
Wall[i]!=RemoteXY.Wall ||
NewYear[i]!=RemoteXY.NewYear ||
Kitchen_Work[i]!=RemoteXY.Kitchen_Work ||
Kitchen_RGB[i]!=RemoteXY.Kitchen_RGB ||
Bar[i]!=RemoteXY.Bar ||
Bar_RGB[i]!=RemoteXY.Bar_RGB))
{stateFIX[i]=0; CM("noBath ----- FIX "); CM(i); CMn(" нарушен");}}
break;
case 1: sceneAfter=1;
f_Koridor[button]=RemoteXY.Koridor;
f_Mirror_RGB[button]=RemoteXY.Mirror_RGB;
f_Spot0[button]=RemoteXY.Spot0;
f_Spot1[button]=RemoteXY.Spot1;
f_Window_RGB[button]=RemoteXY.Window_RGB;
f_Torsher[button]=RemoteXY.Torsher;
f_Wall[button]=RemoteXY.Wall;
f_NewYear[button]=RemoteXY.NewYear;
f_Kitchen_Work[button]=RemoteXY.Kitchen_Work;
f_Kitchen_RGB[button]=RemoteXY.Kitchen_RGB;
f_Bar[button]=RemoteXY.Bar;
f_Bar_RGB[button]=RemoteXY.Bar_RGB; lastFix=button; CM("noBath ------ FIX "); CM(lastFix); CMn(" прежнего сост");
break;
case 2: if(sceneAfter) {sceneAfter=0;
Koridor[lastFix]=RemoteXY.Koridor;
Mirror_RGB[lastFix]=RemoteXY.Mirror_RGB;
Spot0[lastFix]=RemoteXY.Spot0;
Spot1[lastFix]=RemoteXY.Spot1;
Window_RGB[lastFix]=RemoteXY.Window_RGB;
Torsher[lastFix]=RemoteXY.Torsher;
Wall[lastFix]=RemoteXY.Wall;
NewYear[lastFix]=RemoteXY.NewYear;
Kitchen_Work[lastFix]=RemoteXY.Kitchen_Work;
Kitchen_RGB[lastFix]=RemoteXY.Kitchen_RGB;
Bar[lastFix]=RemoteXY.Bar;
Bar_RGB[lastFix]=RemoteXY.Bar_RGB; CM("noBath ----- FIX "); CM(lastFix); CMn(" текущего сост");
stateFIX[lastFix]=1;} break;
case 3: if(!stateFIX[button]) {CM("*** ----- FIX "); CM(button); CMn(" не восстановить"); return(0);} else {
RemoteXY.Koridor=f_Koridor[button];
RemoteXY.Mirror_RGB=f_Mirror_RGB[button];
RemoteXY.Spot0=f_Spot0[button];
RemoteXY.Spot1=f_Spot1[button];
RemoteXY.Window_RGB=f_Window_RGB[button];
RemoteXY.Torsher=f_Torsher[button];
RemoteXY.Wall=f_Wall[button];
RemoteXY.NewYear=f_NewYear[button];
RemoteXY.Kitchen_Work=f_Kitchen_Work[button];
RemoteXY.Kitchen_RGB=f_Kitchen_RGB[button];
RemoteXY.Bar=f_Bar[button];
RemoteXY.Bar_RGB=f_Bar_RGB[button];
stateFIX[button]=0; CM("noBath ----- FIX "); CM(button); CMn(" восстановление");
sceneRestore=1; return(1);} break;
case 5: CM("noBath ----- состояние FIX "); CM(button);
if(stateFIX[button]) {CMn(" зафиксировано"); return(1);} else {CMn(" не зафиксировано"); return(0);} break;
case 7: for_i(0,13) stateFIX[i]=0; CM("noBath ----- FIX сохранения очищены"); break;} return(0);}

boolean FIXBath(uint8_t in=0, uint8_t button=0) { // 1-фикс прежнего, 2-фикс изменения 3-восстановить || возврат 1 - можно восстановить, 2 - нельзя
static boolean sceneAfter;                             // сцена света после зафиксирована
static boolean Bath[2], f_Bath[2];                     // состояние освещения в ванне
static boolean Bath_Mirror[2], f_Bath_Mirror[2];       // состояние подсветки зеркала в ванне
static boolean Bath_RGB[2], f_Bath_RGB[2];             // состояние RGB подсветки в ванне
static boolean stateFIX[2];                            // состояние зафиксировано
static uint8_t lastFix;                                // запоминаем текущее состояние после сохранения прежнего

if(button) {CM("----- > Button FIX act: "); CM(in); CM(" button: "); CMn(button);}

switch(in) {
case 0:
for_i(0,2) {if(stateFIX[i] && (
Bath[i]!=RemoteXY.Bath || 
Bath_Mirror[i]!=RemoteXY.Bath_Mirror ||
Bath_RGB[i]!=RemoteXY.Bath_RGB))
{stateFIX[i]=0; CM("ванна ----- FIX "); CM(i); CMn(" нарушен");}}
break;
case 1: sceneAfter=1;
f_Bath[button]=RemoteXY.Bath;
f_Bath_Mirror[button]=RemoteXY.Bath_Mirror;
f_Bath_RGB[button]=RemoteXY.Bath_RGB;
lastFix=button; CM("ванна ------ FIX "); CM(lastFix); CMn(" прежнего сост");
break;
case 2: if(sceneAfter) {sceneAfter=0;
Bath[lastFix]=RemoteXY.Bath;
Bath_Mirror[lastFix]=RemoteXY.Bath_Mirror;
Bath_RGB[lastFix]=RemoteXY.Bath_RGB;
stateFIX[lastFix]=1; CM("ванна ----- FIX "); CM(lastFix); CMn(" текущего сост");} break; 
case 3: if(!stateFIX[button]) {CM("ванна ----- FIX "); CM(button); CMn(" не восстановить"); return(0);} else {
RemoteXY.Bath=f_Bath[button];
RemoteXY.Bath_Mirror=f_Bath_Mirror[button];
RemoteXY.Bath_RGB=f_Bath_RGB[button];
stateFIX[button]=0; CM("ванна ----- FIX "); CM(button); CMn(" восстановление");
sceneRestore=1; return(1);} break;
case 5: CM("ванна ----- состояние FIX "); CM(button);
if(stateFIX[button]) {CMn(" зафиксировано"); return(1);} else {CMn(" не зафиксировано"); return(0);} break;
case 7: for_i(0,2) stateFIX[i]=0; CM("ванна ----- FIX сохранения очищены"); break;} return(0);}

boolean FIXRoom(uint8_t in=0, uint8_t button=0) { // 1-фикс прежнего, 2-фикс изменения 3-восстановить || возврат 1 - можно восстановить, 2 - нельзя
static boolean sceneAfter;                             // сцена света после зафиксирована
static boolean Spot0[2], f_Spot0[2];                   // состояние Спотов 1
static boolean Spot1[2], f_Spot1[2];                   // состояние Спотов 2
static boolean Window_RGB[2], f_Window_RGB[2];         // состояние RGB подсветки окон
static boolean Torsher[2], f_Torsher[2];               // состояние Торшера
static boolean Wall[2], f_Wall[2];                     // состояние подсветки на стене
static boolean NewYear[2], f_NewYear[2];               // состояние розеток вверху окон
static boolean stateFIX[2];                            // состояние зафиксировано
static uint8_t lastFix;                                // запоминаем текущее состояние после сохранения прежнего

if(button) {CM("----- > Button FIX act: "); CM(in); CM(" button: "); CMn(button);}

switch(in) {
case 0:
for_i(0,2) {if(stateFIX[i] && (
Spot0[i]!=RemoteXY.Spot0 ||
Spot1[i]!=RemoteXY.Spot1 ||
Window_RGB[i]!=RemoteXY.Window_RGB ||
Torsher[i]!=RemoteXY.Torsher ||
Wall[i]!=RemoteXY.Wall ||
NewYear[i]!=RemoteXY.NewYear))
{stateFIX[i]=0; CM("room ----- FIX "); CM(i); CMn(" нарушен");}}
break;
case 1: sceneAfter=1;
f_Spot0[button]=RemoteXY.Spot0;
f_Spot1[button]=RemoteXY.Spot1;
f_Window_RGB[button]=RemoteXY.Window_RGB;
f_Torsher[button]=RemoteXY.Torsher;
f_Wall[button]=RemoteXY.Wall;
f_NewYear[button]=RemoteXY.NewYear;
lastFix=button; CM("room ------ FIX "); CM(lastFix); CMn(" прежнего сост");
break;
case 2: if(sceneAfter) {sceneAfter=0;
Spot0[lastFix]=RemoteXY.Spot0;
Spot1[lastFix]=RemoteXY.Spot1;
Window_RGB[lastFix]=RemoteXY.Window_RGB;
Torsher[lastFix]=RemoteXY.Torsher;
Wall[lastFix]=RemoteXY.Wall;
NewYear[lastFix]=RemoteXY.NewYear;
stateFIX[lastFix]=1; CM("room ----- FIX "); CM(lastFix); CMn(" текущего сост");} break; 
case 3: if(!stateFIX[button]) {CM("room ----- FIX "); CM(button); CMn(" не восстановить"); return(0);} else {
RemoteXY.Spot0=f_Spot0[button];
RemoteXY.Spot1=f_Spot1[button];
RemoteXY.Window_RGB=f_Window_RGB[button];
RemoteXY.Torsher=f_Torsher[button];
RemoteXY.Wall=f_Wall[button];
RemoteXY.NewYear=f_NewYear[button];
stateFIX[button]=0; CM("room ----- FIX "); CM(button); CMn(" восстановление");
sceneRestore=1; return(1);} break;
case 5: CM("room ----- состояние FIX "); CM(button);
if(stateFIX[button]) {CMn(" зафиксировано"); return(1);} else {CMn(" не зафиксировано"); return(0);} break;
case 7: for_i(0,2) stateFIX[i]=0; CM("room ----- FIX сохранения очищены"); break;} return(0);}

void Event_Remote(uint8_t in=0) {RemoteXY_Handler(); if(!dataToEEPROM.readFirst) return;
static boolean RelaxMode;                              // состояние режима Relax
static boolean MaxLight;                               // состояние режима всё включено
static boolean Spot0;                                  // состояние Спотов 1
static boolean Spot1;                                  // состояние Спотов 2
static boolean Torsher;                                // состояние Торшера
static boolean Wall;                                   // состояние подсветки на стене
static boolean Kitchen_Work;                           // состояние рабочей зоны кухни
static boolean Bar;                                    // состояние ламп над барной стойкой
static boolean Koridor;                                // состояние яркого света в корридоре
static boolean Bath;                                   // состояние яркого света в ванной
static boolean Bath_Mirror;                            // состояние зеркала в ванной
static boolean Vent;                                   // состояние вытяжки в ванной
static boolean All_OFF;                                // выключить всё
static boolean NewYear;                                // состояние розеток вверху окон
static uint8_t Select_RGB;                             // положение селктора RGB
static boolean init=0;                                 // инициализация при включении
boolean relaxChange=0;                                 // если меняли Chill, то не скидывать выбор RGB
static uint8_t Tmr_select;                             // положение переключателя выбора таймеров
static int8_t setMin[5], setSek[5];                    // значения таймеров задежки
uint16_t randomSound;                                  // перебор фраз приветствия если совпала с последней

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::: голосовой помощник
static boolean r_AlarmMode;                            // последнее состояние режима тревоги
static boolean r_waitMode;                             // последнее состояние режима ожидания
static boolean r_allOFFWithBath=1;                     // все освещение в квартире выключено
static boolean r_allOFFWithoutBath=1;                  // все освещение в квартире кроме ванны выключено
static boolean r_Relax;                                // последнее состояние режима CHILL
static boolean r_Relax_break;                          // режим CHILL прерван
static boolean voiceSkip=1;                            // если озвучили Sound, остальные неприоритетные подсказки игнорируем, 1 при первом запуске инициализирует переменные
static boolean skipSoundBlock;                         // если переключился CHILL или MAX Light - не воспроизводить звуки в блоке
static boolean r_Mirror_RGB;                           // последнее состояние RGB подсветки зеркала в коридоре
static boolean r_Kitchen_RGB;                          // последнее состояние RGB подсветки кухни
static boolean r_Bar_RGB;                              // последнее состояние RGB подсветки барной стойки
static boolean r_Window_RGB;                           // последнее состояние RGB подсветки окон
static boolean r_MaxLight;                             // последнее состояние режима MAX Light
static boolean r_MaxLight_break;                       // режим MAX Light прерван
static boolean r_Spot0;                                // последнее состояние 1 группы яркого освещения
static boolean r_Spot1;                                // последнее состояние 2 группы яркого освещения
static boolean r_Koridor;                              // последнее состояние освещения коридора
static boolean r_Kitchen_Work;                         // последнее состояние освещения рабочей поверхности кухни
static boolean r_Bar;                                  // последнее состояние освещения барной стойки
static boolean r_Torsher;                              // последнее состояние торщера
static boolean r_Wall;                                 // последнее состояние подсветки на стене
static boolean r_NewYear;                              // последнее состояние розеток для гирлянд на окнах
static boolean r_TimerWindow_switch;                   // последнее состояние переключателя таймера отключения розеток для гирлянд на окнах
static boolean r_Bath;                                 // последнее состояние освещения в ванне
static boolean r_Bath_Mirror;                          // последнее состояние подсветки зеркала в ванне
static boolean r_Bath_RGB;                             // последнее состояние RGB подсветки в ванне
static boolean r_Vent;                                 // последнее состояние вытяжки в ванне
static uint8_t r_Select_RGB;                           // последнее состояние выбора зоны RGB подсветки
static boolean r_nigtMode;                             // последнее состояние режима сна
static boolean r_autoMode;                             // последнее состояние режима авто
static uint8_t r_Tmr_select;                           // последнее состояние переключателя таймеров зон
static uint8_t r_Start_RGB;                            // последнее состояние RGB подсветки при запуске
static uint8_t r_cnox;                                 // последнее состояние режима cnox
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::: голосовой помощник

if(!init) { CMn("Инициализация RemoteXY");             // Инициализация RemoteXY при первом запуске

RemoteXY.Select_RGB=0; strcpy(RemoteXY.RGB_text,"Вход"); 
RemoteXY.AutoMode_switch=save.autoMode;
strcpy(RemoteXY.Timer_Name1,"Таймеры");
strcpy(RemoteXY.Timer_Name2,"освещения");
strcpy(RemoteXY.Value_text,"минут");
strcpy(RemoteXY.Timer_select_text,"освещение в коридоре");
//for_i(0,5) {save.autoMode ? modeInd[i]=autoIndOn : modeInd[i]=autoIndOff;}
//strcpy(RemoteXY.Timer_select_text2,"на входе");
RemoteXY.min=map(save.Timer[0]/60,0,60,0,100); RemoteXY.sek=map(save.Timer[0]%60,0,60,0,100);
setMin[RemoteXY.Timer_select]=RemoteXY.min; setSek[RemoteXY.Timer_select]=RemoteXY.sek;
sprintf(RemoteXY.Timer_value_text,"%d м. %d с.",save.Timer[0]/60,save.Timer[0]%60);
RemoteXY.TimerWindow_switch=save.PowerWindowOFF;
if (!RemoteXY.TimerWindow_switch) strcpy(RemoteXY.CountDown_Window,"таймер не активен"); else
if(save.autoMode) strcpy(RemoteXY.CountDown_Window,"авто отключение по таймеру"); else {RemoteXY.level=0; strcpy(RemoteXY.CountDown_Window,"нужно включить авто режим");}
RemoteXY.Start_RGB=save.StartMode;
if(save.StartMode==0) strcpy(RemoteXY.Start_RGB_text,"Восстановить"); else if (save.StartMode == 1) strcpy(RemoteXY.Start_RGB_text,"Выключить"); else if (save.StartMode == 2) strcpy(RemoteXY.Start_RGB_text,"Включить");
sprintf(RemoteXY.Button_text,"Двойной клик: %d мс.",BtnHOLD);
RemoteXY.Set_rgb_r=save.RGB_r[0];
RemoteXY.Set_rgb_g=save.RGB_g[0];
RemoteXY.Set_rgb_b=save.RGB_b[0];
if(save.Sound) RemoteXY.Sound_switch=1;
voiceSkip=1;
RemoteXY.All_OFF=1; All_OFF=RemoteXY.All_OFF;
strcpy(RemoteXY.firmware,"Firmware "); strcat(RemoteXY.firmware,FirmwareVersion);
#if defined AccessPointEnable
strcpy(RemoteXY.IP_text,"IP ADRESS: 192.168.4.22");
#endif
} // инициализация переменных RemoteXY при первом запуске

if(ClientConnected!=RemoteXY.connect_flag) {ClientConnected=RemoteXY.connect_flag; if(ClientConnected) {CMn("Клиент подключен"); randomSound=random(68,95);
if(randomSound==lastVoice) randomSound++; if(randomSound>95) randomSound=68; lastVoice=randomSound; if(RemoteXY.Sound_switch) voice(randomSound);} else CMn("Клиент отсоединен");}

if(All_OFF!=RemoteXY.All_OFF) {All_OFF=RemoteXY.All_OFF; RemoteXY.All_OFF_led=1; if(RemoteXY.All_OFF) {LightOFFAll(0); RemoteXY.All_OFF=1;} CMn("Всё выключили");}
if(All_OFF && (RemoteXY.Spot0 || RemoteXY.Spot1 || RemoteXY.Window_RGB || RemoteXY.Torsher || RemoteXY.Wall || // проверка
RemoteXY.Mirror_RGB || RemoteXY.Kitchen_Work || RemoteXY.Kitchen_RGB || RemoteXY.Bar || RemoteXY.Bar_RGB || RemoteXY.Koridor ||
RemoteXY.Bath || RemoteXY.Bath_Mirror || RemoteXY.Bath_RGB || RemoteXY.Vent || RemoteXY.NewYear)) {CMn("Что-то не выключено"); All_OFF=0; RemoteXY.All_OFF=0; RemoteXY.All_OFF_led=1;}
if(!All_OFF && !RemoteXY.Spot0 && !RemoteXY.Spot1 && !RemoteXY.Window_RGB && !RemoteXY.Torsher && !RemoteXY.Wall && // проверка
!RemoteXY.Mirror_RGB && !RemoteXY.Kitchen_Work && !RemoteXY.Kitchen_RGB && !RemoteXY.Bar && !RemoteXY.Bar_RGB && !RemoteXY.Koridor &&
!RemoteXY.Bath && !RemoteXY.Bath_Mirror && !RemoteXY.Bath_RGB && !RemoteXY.Vent && !RemoteXY.NewYear) {CMn("Проверили, все выключено"); All_OFF=1; RemoteXY.All_OFF=1; RemoteXY.All_OFF_led=1;}

if(RelaxMode!=RemoteXY.Relax) {RelaxMode=RemoteXY.Relax; RemoteXY.Relax_led=1; if(RelaxMode) {RemoteXY.Spot0=0; // переключение
RemoteXY.Spot1=0; RemoteXY.Torsher=0; RemoteXY.Wall=0; RemoteXY.Mirror_RGB=1; RemoteXY.Kitchen_RGB=1; RemoteXY.Bar_RGB=1; RemoteXY.Window_RGB=1; RemoteXY.Kitchen_Work=0;
RemoteXY.Bar=0; RemoteXY.Koridor=0; RemoteXY.Select_RGB=1; relaxChange=1;} else {RemoteXY.Mirror_RGB=0; RemoteXY.Kitchen_RGB=0; RemoteXY.Bar_RGB=0; RemoteXY.Window_RGB=0;} Process_Out();}

if(RemoteXY.Relax && (!RemoteXY.Mirror_RGB || !RemoteXY.Kitchen_RGB || !RemoteXY.Bar_RGB || !RemoteXY.Window_RGB || RemoteXY.Spot0 ||
RemoteXY.Spot1 || RemoteXY.Torsher || RemoteXY.Wall || RemoteXY.Kitchen_Work || RemoteXY.Bar || RemoteXY.Koridor)) { // проверка
RelaxMode=0; RemoteXY.Relax=0; RemoteXY.Relax_led=1;}

if(!RemoteXY.Relax && RemoteXY.Mirror_RGB && RemoteXY.Kitchen_RGB && RemoteXY.Window_RGB && RemoteXY.Bar_RGB && 
!RemoteXY.Spot0 && !RemoteXY.Spot1 && !RemoteXY.Torsher && !RemoteXY.Wall && !RemoteXY.Kitchen_Work && !RemoteXY.Bar && !RemoteXY.Koridor) { // проверка
RelaxMode=1; RemoteXY.Relax=1; RemoteXY.Relax_led=1;}

if(waitMode!=RemoteXY.WaitMode) {waitMode=RemoteXY.WaitMode; cnox=0; timerKoridor=0; waitModeStartTime=millis(); 
if(RemoteXY.WaitMode) {LightOFFAll(0); for_i(5,10) waitLedInd[i]=1; RemoteXY.WaitMode=1; timerLedInd[5]=0; cnoxLedInd[5]=0;
RemoteXY.WaitMode_led=1; voice(3); CMn("Дежурный режим ВКЛ");} else {RemoteXY.WaitMode_led=1;
for_i(5,10) {waitLedInd[i]=0; autoOnLedNight[i]=0; autoOffLedNight[i]=0; if(save.autoMode) {autoLedOn[i]=1; autoLedOff[i]=0;} else {autoLedOn[i]=0; autoLedOff[i]=1;}}
voice(4); CMn("Дежурный режим ВЫКЛ");}}

if(MaxLight!=RemoteXY.MaxLight) {MaxLight=RemoteXY.MaxLight; RemoteXY.MaxLight_led=1; if(MaxLight) {RemoteXY.Spot0=1; // переключение
RemoteXY.Spot1=1; RemoteXY.Window_RGB=0; RemoteXY.Torsher=0; RemoteXY.Wall=0; RemoteXY.Bar=0; RemoteXY.Relax=0; RemoteXY.Mirror_RGB=0; 
RemoteXY.Kitchen_RGB=0; RemoteXY.Bar_RGB=0; RemoteXY.Koridor=1; RemoteXY.NewYear=0;} else {
RemoteXY.Spot0=0; RemoteXY.Spot1=0; RemoteXY.Koridor=0;} Process_Out();}
if(!RemoteXY.MaxLight && (RemoteXY.Spot0 && RemoteXY.Spot1 && RemoteXY.Koridor)) {MaxLight=1; RemoteXY.MaxLight=1; RemoteXY.MaxLight_led=1;} // проверка
if(RemoteXY.MaxLight && (!RemoteXY.Spot0 || !RemoteXY.Spot1 || !RemoteXY.Koridor)) {MaxLight=0; RemoteXY.MaxLight=0; RemoteXY.MaxLight_led=1;} // проверка

if(save.autoMode!=RemoteXY.AutoMode_switch) {save.autoMode=RemoteXY.AutoMode_switch; FIXLight(ResetFIX); if(!RemoteXY.WaitMode) {save.autoMode ? voice(45) : voice(46);} saveData1;
if (!RemoteXY.TimerWindow_switch) strcpy(RemoteXY.CountDown_Window,"таймер не активен"); else
save.autoMode ? strcpy(RemoteXY.CountDown_Window,"авто отключение по таймеру") : strcpy(RemoteXY.CountDown_Window,"нужно включить авто режим");
if(save.autoMode) CMn("Авто режим ВКЛ"); else CMn("Авто режим ВЫКЛ");}

if(Tmr_select!=RemoteXY.Timer_select) {Tmr_select=RemoteXY.Timer_select; RemoteXY.Timer_select_led=1;
if(setMin[RemoteXY.Timer_select]!=RemoteXY.min) RemoteXY.min_led=1; if(setSek[RemoteXY.Timer_select]!=RemoteXY.sek) RemoteXY.sek_led=1;
if(RemoteXY.Timer_select<4) CMn("Регулируем таймеры освещения"); else CMn("Регулируем скорость RGB перетекания");
RemoteXY.HideSec_led=0; switch(RemoteXY.Timer_select) {
case 0: strcpy(RemoteXY.Timer_select_text,"освещение в коридоре"); break;
case 1: strcpy(RemoteXY.Timer_select_text, "подсветка RGB в ванне"); break;
case 2: strcpy(RemoteXY.Timer_select_text, "вентиляция в ванне"); break;
case 3: strcpy(RemoteXY.Timer_select_text, "розетки над окнами"); break;
case 4: strcpy(RemoteXY.Timer_select_text, "одного RGB цвета в другой"); RemoteXY.HideSec_led=1; break;}
if(RemoteXY.Timer_select<4) {strcpy(RemoteXY.Timer_Name1,"Таймеры"); strcpy(RemoteXY.Timer_Name2,"освещения");} else {strcpy(RemoteXY.Timer_Name1,"Скорость"); strcpy(RemoteXY.Timer_Name2,"перетекания");}
RemoteXY.Timer_select<4 ? strcpy(RemoteXY.Value_text,"минут") : strcpy(RemoteXY.Value_text,"скорость");
if(RemoteXY.Timer_select<4) {RemoteXY.min=map(save.Timer[RemoteXY.Timer_select]/60,0,60,0,100); RemoteXY.sek=map(save.Timer[RemoteXY.Timer_select]%60,0,60,0,100);} else {
RemoteXY.min=map(save.FadeDelay,0,20,0,100); {char val[3]; itoa(save.FadeDelay,val,10); strcpy(RemoteXY.Timer_value_text,"      "); strcat(RemoteXY.Timer_value_text,val);}}
setMin[RemoteXY.Timer_select]=RemoteXY.min;
if(RemoteXY.Timer_select<4) {setSek[RemoteXY.Timer_select]=RemoteXY.sek;
sprintf(RemoteXY.Timer_value_text,"%d м. %d с.",save.Timer[RemoteXY.Timer_select]/60,save.Timer[RemoteXY.Timer_select]%60);} else {
char val[3]; itoa(21-save.FadeDelay,val,10); strcpy(RemoteXY.Timer_value_text,"      "); strcat(RemoteXY.Timer_value_text,val);}}

if(setMin[RemoteXY.Timer_select]!=RemoteXY.min || setSek[RemoteXY.Timer_select]!=RemoteXY.sek) {if(RemoteXY.Timer_select<4) {
save.Timer[RemoteXY.Timer_select]=map(RemoteXY.min,0,100,0,60)*60+map(RemoteXY.sek,0,100,0,59); 
if(save.Timer[RemoteXY.Timer_select]<1) {save.Timer[RemoteXY.Timer_select]=1; RemoteXY.sek=1;}
CM("SaveTime = "); CM(save.Timer[RemoteXY.Timer_select]); CMn(" сек");
CM(" Min = "); CM(map(RemoteXY.min,0,100,0,60)); CM(" Sek = "); CMn(map(RemoteXY.sek,0,100,0,59));
sprintf(RemoteXY.Timer_value_text,"%02d м. %02d с.",save.Timer[RemoteXY.Timer_select]/60,save.Timer[RemoteXY.Timer_select]%60);} else {
save.FadeDelay=map(RemoteXY.min,0,100,20,0); CM("RGB flow = "); CM(save.FadeDelay);
char val[3]; itoa(21-save.FadeDelay,val,10); strcpy(RemoteXY.Timer_value_text,"      "); strcat(RemoteXY.Timer_value_text,val);} 
setMin[RemoteXY.Timer_select]=RemoteXY.min; setSek[RemoteXY.Timer_select]=RemoteXY.sek; saveData60;}

if(Spot0!=RemoteXY.Spot0) {Spot0=RemoteXY.Spot0; RemoteXY.Spot0_led=1; syncLedInd[6]=1; Process_Out();}
if(Spot1!=RemoteXY.Spot1) {Spot1=RemoteXY.Spot1; RemoteXY.Spot1_led=1; syncLedInd[6]=1; Process_Out();}
if(Torsher!=RemoteXY.Torsher) {Torsher=RemoteXY.Torsher; RemoteXY.Torsher_led=1; syncLedInd[6]=1; Process_Out();}
if(Kitchen_Work!=RemoteXY.Kitchen_Work) {Kitchen_Work=RemoteXY.Kitchen_Work; RemoteXY.Kitchen_Work_led=1; syncLedInd[7]=1; Process_Out();}
if(Wall!=RemoteXY.Wall) {Wall=RemoteXY.Wall; RemoteXY.Wall_led=1; syncLedInd[6]=1; Process_Out();}
if(Bar!=RemoteXY.Bar) {Bar=RemoteXY.Bar; RemoteXY.Bar_led=1; syncLedInd[8]=1; Process_Out();}
if(Koridor!=RemoteXY.Koridor) {Koridor=RemoteXY.Koridor; RemoteXY.Koridor_led=1; syncLedInd[5]=1; Process_Out();}
if(Bath!=RemoteXY.Bath) {Bath=RemoteXY.Bath; RemoteXY.Bath_led=1; syncLedInd[9]=1; Process_Out();}
if(Bath_Mirror!=RemoteXY.Bath_Mirror) {Bath_Mirror=RemoteXY.Bath_Mirror; RemoteXY.Bath_Mirror_led=1; syncLedInd[9]=1; Process_Out();}
if(Vent!=RemoteXY.Vent) {Vent=RemoteXY.Vent; RemoteXY.Vent_led=1; syncLedInd[9]=1; Process_Out();}
if(NewYear!=RemoteXY.NewYear) {NewYear=RemoteXY.NewYear; RemoteXY.NewYear_led=1; syncLedInd[6]=1; if(!NewYear) {RemoteXY.level=0;
if (!RemoteXY.TimerWindow_switch) strcpy(RemoteXY.CountDown_Window,"таймер не активен"); else
if(save.autoMode) strcpy(RemoteXY.CountDown_Window,"авто отключение по таймеру"); else {RemoteXY.level=0; strcpy(RemoteXY.CountDown_Window,"нужно включить авто режим");}} syncLedInd[6]=1; Process_Out();}

if(Select_RGB!=RemoteXY.Select_RGB) {Select_RGB=RemoteXY.Select_RGB; RemoteXY.Select_RGB_led=1;
switch (Select_RGB) {
case 0: strcpy(RemoteXY.RGB_text,"Вход"); RemoteXY.Mirror_RGB_led=1; break;
case 1: strcpy(RemoteXY.RGB_text,"Окна"); RemoteXY.Window_RGB_led=1; break;
case 2: strcpy(RemoteXY.RGB_text,"Кухня"); RemoteXY.Kitchen_RGB_led=1; break;
case 3: strcpy(RemoteXY.RGB_text,"Бар"); RemoteXY.Bar_RGB_led=1; break;
case 4: strcpy(RemoteXY.RGB_text,"Ванна"); RemoteXY.Bath_RGB_led=1; break;}
RemoteXY.Set_rgb_r=save.RGB_r[Select_RGB];
RemoteXY.Set_rgb_g=save.RGB_g[Select_RGB];
RemoteXY.Set_rgb_b=save.RGB_b[Select_RGB]; CM("Модуль RGB "); CM(Select_RGB); CM(" "); CM(RemoteXY.Set_rgb_r); CM(" "); CM(RemoteXY.Set_rgb_g); CM(" "); CMn(RemoteXY.Set_rgb_b);}

if(save.RGB_r[Select_RGB]!=RemoteXY.Set_rgb_r || save.RGB_g[Select_RGB]!=RemoteXY.Set_rgb_g || save.RGB_b[Select_RGB]!=RemoteXY.Set_rgb_b) {
RemoteXY.Set_rgb_r; save.RGB_g[Select_RGB]=RemoteXY.Set_rgb_g; save.RGB_b[Select_RGB]=RemoteXY.Set_rgb_b;
save.RGB_r[Select_RGB]=RemoteXY.Set_rgb_r; lVal[Select_RGB].Led_Value[0]=RemoteXY.Set_rgb_r;
save.RGB_g[Select_RGB]=RemoteXY.Set_rgb_g; lVal[Select_RGB].Led_Value[1]=RemoteXY.Set_rgb_g;
save.RGB_b[Select_RGB]=RemoteXY.Set_rgb_b; lVal[Select_RGB].Led_Value[2]=RemoteXY.Set_rgb_b; saveData60;
if((RemoteXY.Select_RGB==0 && save.RGB_ModulePower[0]) || (RemoteXY.Select_RGB==1 && save.RGB_ModulePower[1]) ||
(RemoteXY.Select_RGB==2 && save.RGB_ModulePower[2]) || (RemoteXY.Select_RGB==3 && save.RGB_ModulePower[3]) || (RemoteXY.Select_RGB==4 && save.RGB_ModulePower[4])) deltaCalc(RemoteXY.Select_RGB);}

if(save.RGB_ModulePower[4]!=RemoteXY.Bath_RGB) {RemoteXY.Bath_RGB ? RGB_PowerOn(4) : RGB_PowerOff(4); syncLedInd[9]=1;
RemoteXY.Bath_RGB_led=1; if(save.RGB_ModulePower[4] && !relaxChange) RemoteXY.Select_RGB=4; CMn("Select RGB -> 4"); saveData1;}
if(save.RGB_ModulePower[2]!=RemoteXY.Kitchen_RGB) {RemoteXY.Kitchen_RGB ? RGB_PowerOn(2) : RGB_PowerOff(2); syncLedInd[7]=1;
RemoteXY.Kitchen_RGB_led=1; if(save.RGB_ModulePower[2] && !relaxChange) RemoteXY.Select_RGB=2; CMn("Select RGB -> 2"); saveData1;}
if(save.RGB_ModulePower[3]!=RemoteXY.Bar_RGB) {RemoteXY.Bar_RGB ? RGB_PowerOn(3) : RGB_PowerOff(3); syncLedInd[8]=1;
RemoteXY.Bar_RGB_led=1; if(save.RGB_ModulePower[3] && !relaxChange) RemoteXY.Select_RGB=3; CMn("Select RGB -> 3"); saveData1;}
if(save.RGB_ModulePower[0]!=RemoteXY.Mirror_RGB) {RemoteXY.Mirror_RGB ? RGB_PowerOn(0) : RGB_PowerOff(0); syncLedInd[5]=1;
RemoteXY.Mirror_RGB_led=1; if(save.RGB_ModulePower[0] && !relaxChange) RemoteXY.Select_RGB=0; CMn("Select RGB -> 0"); saveData1;}
if(save.RGB_ModulePower[1]!=RemoteXY.Window_RGB) {RemoteXY.Window_RGB ? RGB_PowerOn(1) : RGB_PowerOff(1); syncLedInd[6]=1; 
RemoteXY.Window_RGB_led=1; if(save.RGB_ModulePower[1] && !relaxChange) RemoteXY.Select_RGB=1; CMn("Select RGB -> 1"); saveData1;}


if(save.PowerWindowOFF!=RemoteXY.TimerWindow_switch) {save.PowerWindowOFF=RemoteXY.TimerWindow_switch; saveData1;
if (!RemoteXY.TimerWindow_switch) {strcpy(RemoteXY.CountDown_Window,"таймер не активен"); RemoteXY.level=0;} else
if(save.autoMode) strcpy(RemoteXY.CountDown_Window,"авто отключение по таймеру"); else {strcpy(RemoteXY.CountDown_Window,"нужно включить авто режим"); RemoteXY.level=0;} syncLedInd[6]=1;}

if (save.StartMode!=RemoteXY.Start_RGB) {save.StartMode=RemoteXY.Start_RGB; RemoteXY.Start_RGB_led=1;
switch(save.StartMode) {
case 0: strcpy(RemoteXY.Start_RGB_text,"Восстановить"); break;
case 1: strcpy(RemoteXY.Start_RGB_text,"Выключить"); break;
case 2: strcpy(RemoteXY.Start_RGB_text,"Включить"); break;} saveData1;}

FIXLight(FixChange); FIXBath(FixChange); FIXRoom(FixChange); FIXLightWithoutBath(FixChange);
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::: голосовой помощник
if(save.Sound!=RemoteXY.Sound_switch) {save.Sound=RemoteXY.Sound_switch; if(save.Sound) {randomSound=random(68,95); if(randomSound==lastVoice) randomSound++;
if(randomSound>95) randomSound=68; lastVoice=randomSound; voice(randomSound); RemoteXY.Block_Sound=0;} else {voice(104); RemoteXY.Block_Sound=1;} voiceSkip=1; saveData1;}

if(save.Sound) { // воспроизводим голос в порядке приоритета, если он не отключен и клиент подсоединен
if(r_AlarmMode!=AlarmMode) {r_AlarmMode=AlarmMode; if(AlarmMode) voice(105); else voice(200); voiceSkip=1;} if(AlarmMode) voiceSkip=1;

if(r_autoMode!=save.autoMode) {r_autoMode=save.autoMode; {if(!voiceSkip) save.autoMode ? voice(45) : voice(46); voiceSkip=1;}}
if(r_waitMode!=RemoteXY.WaitMode) {r_waitMode=RemoteXY.WaitMode; {if(!voiceSkip) RemoteXY.WaitMode ? voice(3) : voice(4); voiceSkip=1;}}
if(timerVoice) {timerVoice=0; if(!voiceSkip) voice(125); voiceSkip=1;}
if(r_cnox!=cnox) {r_cnox=cnox; if(cnox && !voiceSkip) {voice(124); voiceSkip=1;}}
if(!r_nigtMode && nigtMode) {r_nigtMode=nigtMode; {if(!voiceSkip && nigtMode) voice(47); voiceSkip=1;}}
if(r_allOFFWithBath!=LightOFFAll(1)) {if(r_allOFFWithBath) r_allOFFWithBath=0; else {r_allOFFWithBath=1; if(!voiceSkip) voice(41); voiceSkip=1;}}
if(r_allOFFWithoutBath!=LightOFFAllWithoutBathRoom(1)) {if(r_allOFFWithoutBath) r_allOFFWithoutBath=0; else {r_allOFFWithoutBath=1; if(!voiceSkip) voice(60); voiceSkip=1;}}

if(sceneRestore) {sceneRestore=0; if(!voiceSkip) voice(123); voiceSkip=1;}
if(r_MaxLight_break && RemoteXY.MaxLight) {r_MaxLight_break=0; r_MaxLight=1; if(!voiceSkip && !skipSoundBlock) voice(38); voiceSkip=1;}
if(!r_MaxLight && RemoteXY.MaxLight) {r_MaxLight=RemoteXY.MaxLight; r_MaxLight_break=0; if(!voiceSkip) voice(23); voiceSkip=1;}

if(r_Relax_break && RemoteXY.Relax) {r_Relax_break=0; r_Relax=1; if(!voiceSkip) voice(36); voiceSkip=1;} // CHILL OUT вкл
if(!r_Relax && RemoteXY.Relax) {r_Relax=RemoteXY.Relax; r_Relax_break=0; if(!voiceSkip) voice(26); voiceSkip=1;}

if(!r_MaxLight && RemoteXY.MaxLight) {r_MaxLight=RemoteXY.MaxLight; r_MaxLight_break=0; if(!voiceSkip && !skipSoundBlock) voice(23); voiceSkip=1;} // MAX Light вкл
if(r_MaxLight_break && !RemoteXY.Spot0 && !RemoteXY.Spot1 && !RemoteXY.Koridor) r_MaxLight_break=0;

if((!r_Spot0 || !r_Spot1) && RemoteXY.Spot0 && RemoteXY.Spot1) {if(!voiceSkip) voice(135); voiceSkip=1;} // яркое освещение комнаты

if(!r_Relax && RemoteXY.Relax) {r_Relax=RemoteXY.Relax; r_Relax_break=0; if(!voiceSkip) voice(26); voiceSkip=1;} // Relax вкл
if(r_Relax_break && !RemoteXY.Mirror_RGB && !RemoteXY.Kitchen_RGB && !RemoteXY.Bar_RGB && !RemoteXY.Window_RGB) r_Relax_break=0;
// ---------- включение ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
if((!r_Spot0 || !r_Spot1 || !r_Torsher || !r_Wall || !r_Window_RGB) && RemoteXY.Spot0 && RemoteXY.Spot1 && RemoteXY.Torsher && RemoteXY.Wall && RemoteXY.Window_RGB) {if(!voiceSkip) voice(131); voiceSkip=1;}
if(!r_Spot0 && RemoteXY.Spot0) {r_Spot0=RemoteXY.Spot0; if(!voiceSkip) voice(108); voiceSkip=1;}
if(!r_Spot1 && RemoteXY.Spot1) {r_Spot1=RemoteXY.Spot1; if(!voiceSkip) voice(110); voiceSkip=1;}
if(!r_Torsher && RemoteXY.Torsher) {r_Torsher=RemoteXY.Torsher; if(!voiceSkip) voice(24); voiceSkip=1;}
if(!r_Wall && RemoteXY.Wall) {r_Wall=RemoteXY.Wall; if(!voiceSkip) voice(11); voiceSkip=1;}
if(!r_NewYear && RemoteXY.NewYear) {r_NewYear=RemoteXY.NewYear; if(!voiceSkip) voice(13); voiceSkip=1;}
if((!r_Kitchen_Work || !r_Kitchen_RGB) && RemoteXY.Kitchen_Work && RemoteXY.Kitchen_RGB) {if(!voiceSkip) voice(126); voiceSkip=1;}
if(!r_Kitchen_Work && RemoteXY.Kitchen_Work) {r_Kitchen_Work=RemoteXY.Kitchen_Work; if(!voiceSkip) voice(15); voiceSkip=1;}
if((!r_Bar || !r_Bar_RGB) && RemoteXY.Bar && RemoteXY.Bar_RGB) {if(!voiceSkip) voice(129); voiceSkip=1;}
if(!r_Bar && RemoteXY.Bar) {r_Bar=RemoteXY.Bar; if(!voiceSkip) voice(19); voiceSkip=1;}
if((!r_Bath || !r_Bath_Mirror || !r_Bath_RGB) && RemoteXY.Bath && RemoteXY.Bath_Mirror && RemoteXY.Bath_RGB) {if(!voiceSkip) voice(133); voiceSkip=1;}
if(!r_Bath && RemoteXY.Bath) {r_Bath=RemoteXY.Bath; if(!voiceSkip) voice(27); voiceSkip=1;}
if(!r_Bath_Mirror && RemoteXY.Bath_Mirror) {r_Bath_Mirror=RemoteXY.Bath_Mirror; if(!voiceSkip) voice(31); voiceSkip=1;}
if(!r_Bath_RGB && RemoteXY.Bath_RGB) {r_Bath_RGB=RemoteXY.Bath_RGB; if(!voiceSkip) {voice(29); BathRGBautoOFF=0;} voiceSkip=1;}
if(!r_Koridor && RemoteXY.Koridor) {r_Koridor=RemoteXY.Koridor; if(!voiceSkip) voice(5); voiceSkip=1;}

if(r_MaxLight && !RemoteXY.MaxLight && (RemoteXY.Spot0 || RemoteXY.Spot1 || RemoteXY.Koridor)) {r_MaxLight_break=1; r_MaxLight=0; if(!voiceSkip) voice(37); voiceSkip=1;} // MAX Light прерван
if(r_MaxLight && !RemoteXY.MaxLight) {r_MaxLight=RemoteXY.MaxLight; r_MaxLight_break=0; if(!voiceSkip && !skipSoundBlock) voice(107); voiceSkip=1;} // MAX Light выкл
if(r_Relax && !RemoteXY.Relax && (RemoteXY.Mirror_RGB || RemoteXY.Kitchen_RGB || RemoteXY.Bar_RGB || RemoteXY.Window_RGB)) {r_Relax_break=1; r_Relax=0; if(!voiceSkip) voice(35); voiceSkip=1;} // CHILL прерван
if(r_Relax && !RemoteXY.Relax) {r_Relax=RemoteXY.Relax; r_Relax_break=0; if(!voiceSkip) voice(106); voiceSkip=1;} // Relax выкл
if((r_Spot0 || r_Spot1 || r_Torsher || r_Wall || r_NewYear || r_Window_RGB) && !RemoteXY.Spot0 && !RemoteXY.Spot1 && !RemoteXY.Torsher && !RemoteXY.Wall && !RemoteXY.Window_RGB && !RemoteXY.NewYear) {if(!voiceSkip) voice(132); voiceSkip=1;}
if((r_Kitchen_Work || r_Kitchen_RGB) && !RemoteXY.Kitchen_Work && !RemoteXY.Kitchen_RGB) {if(!voiceSkip) voice(127); voiceSkip=1;}
if((r_Bar || r_Bar_RGB) && !RemoteXY.Bar && !RemoteXY.Bar_RGB) {if(!voiceSkip) voice(130); voiceSkip=1;}
if((r_Koridor || r_Mirror_RGB) && !RemoteXY.Koridor && !RemoteXY.Mirror_RGB) if(!voiceSkip) {voice(128); voiceSkip=1;} // все освещение в коридоре выкл
if((r_Bath || r_Bath_Mirror || r_Bath_RGB) && !RemoteXY.Bath && !RemoteXY.Bath_Mirror && !RemoteXY.Bath_RGB) {if(!voiceSkip) voice(134); voiceSkip=1;}

if(!r_Window_RGB && RemoteXY.Window_RGB) {r_Window_RGB=RemoteXY.Window_RGB; if(!voiceSkip) voice(9); voiceSkip=1;}
if(!r_Bar_RGB && RemoteXY.Bar_RGB) {r_Bar_RGB=RemoteXY.Bar_RGB; if(!voiceSkip) voice(21); voiceSkip=1;}
if(!r_Kitchen_RGB && RemoteXY.Kitchen_RGB) {r_Kitchen_RGB=RemoteXY.Kitchen_RGB; if(!voiceSkip) voice(17); voiceSkip=1;}
if(!r_Mirror_RGB && RemoteXY.Mirror_RGB) {r_Mirror_RGB=RemoteXY.Mirror_RGB; if(!voiceSkip) voice(39); voiceSkip=1;}

// ---------- выключение ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
if(r_Spot0 && !RemoteXY.Spot0) {r_Spot0=RemoteXY.Spot0; if(!voiceSkip) voice(109); voiceSkip=1;}
if(r_Spot1 && !RemoteXY.Spot1) {r_Spot1=RemoteXY.Spot1; if(!voiceSkip) voice(111); voiceSkip=1;}
if(r_Torsher && !RemoteXY.Torsher) {r_Torsher=RemoteXY.Torsher; if(!voiceSkip) voice(25); voiceSkip=1;}
if(r_Wall && !RemoteXY.Wall) {r_Wall=RemoteXY.Wall; if(!voiceSkip) voice(12); voiceSkip=1;}
if(r_NewYear && !RemoteXY.NewYear) {r_NewYear=RemoteXY.NewYear; if(!voiceSkip) voice(14); voiceSkip=1;}
if(r_Kitchen_Work && !RemoteXY.Kitchen_Work) {r_Kitchen_Work=RemoteXY.Kitchen_Work; if(!voiceSkip) voice(16); voiceSkip=1;}
if(r_Bar && !RemoteXY.Bar) {r_Bar=RemoteXY.Bar; if(!voiceSkip) voice(20); voiceSkip=1;}
if(r_Bath &&!RemoteXY.Bath) {r_Bath=RemoteXY.Bath; if(!voiceSkip) voice(28); voiceSkip=1;}
if(r_Bath_Mirror && !RemoteXY.Bath_Mirror) {r_Bath_Mirror=RemoteXY.Bath_Mirror; if(!voiceSkip) voice(32); voiceSkip=1;}
if(r_Bath_RGB && !RemoteXY.Bath_RGB) {r_Bath_RGB=RemoteXY.Bath_RGB; if(!voiceSkip) {BathRGBautoOFF ? voice(113) : voice(30); BathRGBautoOFF=0;} voiceSkip=1;}
if(r_Koridor && !RemoteXY.Koridor) {r_Koridor=RemoteXY.Koridor; if(!voiceSkip) voice(6); voiceSkip=1;}
if(r_Vent!=RemoteXY.Vent) {r_Vent=RemoteXY.Vent; if(!voiceSkip) if(RemoteXY.Vent) voice(33); else {BathVentautoOFF ? voice(114) : voice(34); BathVentautoOFF=0;} voiceSkip=1;}

if(r_Window_RGB && !RemoteXY.Window_RGB) {r_Window_RGB=RemoteXY.Window_RGB; if(!voiceSkip) voice(10); voiceSkip=1;}
if(r_Bar_RGB && !RemoteXY.Bar_RGB) {r_Bar_RGB=RemoteXY.Bar_RGB; if(!voiceSkip) voice(22); voiceSkip=1;}
if(r_Kitchen_RGB && !RemoteXY.Kitchen_RGB) {r_Kitchen_RGB=RemoteXY.Kitchen_RGB; if(!voiceSkip) voice(18); voiceSkip=1;}
if(r_Mirror_RGB && !RemoteXY.Mirror_RGB) {r_Mirror_RGB=RemoteXY.Mirror_RGB; if(!voiceSkip) voice(40); voiceSkip=1;}

if(r_Select_RGB!=RemoteXY.Select_RGB) {r_Select_RGB=RemoteXY.Select_RGB; if(!voiceSkip) {switch(RemoteXY.Select_RGB) {
case 0: voice(112); break; case 1: voice(50); break; case 2: voice(51); break; case 3: voice(52); break; case 4: voice(53); break;}} voiceSkip=1;}
if(r_Tmr_select!=RemoteXY.Timer_select) {r_Tmr_select=RemoteXY.Timer_select; if(!voiceSkip) {switch(RemoteXY.Timer_select) {
case 0: voice(54); break; case 1: voice(55); break; case 2: voice(56); break; case 3: voice(57); break; case 4: voice(58); break; }} voiceSkip=1;}
if(r_TimerWindow_switch!=RemoteXY.TimerWindow_switch) {r_TimerWindow_switch=RemoteXY.TimerWindow_switch;
if(!voiceSkip) {if(!RemoteXY.TimerWindow_switch) voice(49); else save.autoMode ? voice(48) : voice(67);} voiceSkip=1;}
if(r_Start_RGB!=RemoteXY.Start_RGB) {r_Start_RGB=RemoteXY.Start_RGB;
if(!voiceSkip) {switch(RemoteXY.Start_RGB) {case 0: voice(44); break; case 1: voice(43); break; case 2: voice(42); break;}} voiceSkip=1;}

if(r_nigtMode && !nigtMode) {r_nigtMode=nigtMode; {if(!voiceSkip) if(!nigtMode) voice(122); voiceSkip=1;}}}

if(!init) {init=1; CMn("Инициализация RemoteXY завершена");} //static uint32_t voiceCount; if(voiceSkip) {CM("Skip: "); CMn(voiceCount); voiceCount++;} 
voiceSkip=0;
} // --------------------------------------------------------------------------------------------------- Event RemoteXY END

void CountDownESPLed(boolean in=0) {static boolean syncProcess, RemoteXYconnect; static uint32_t timer, lagTime; static uint8_t mode;
if(!syncProcess && in) {syncProcess=1; CMn("Sync ESP Led");} if(millis()-timer<lagTime) return; timer=millis();
if(syncProcess) {mode++; switch(mode) {
case 1: case 3: digitalWrite(2,1); lagTime=70; break;
case 2: case 4: digitalWrite(2,0); lagTime=30; break;
case 5: digitalWrite(2,1); lagTime=70; break;
case 6: mode=0; syncProcess=0; RemoteXY.connect_flag ? digitalWrite(2,0) : digitalWrite(2,1); {} break;} return;}
if(RemoteXYconnect!=RemoteXY.connect_flag) {RemoteXYconnect=RemoteXY.connect_flag; if(RemoteXYconnect) digitalWrite(2,0);} if(RemoteXYconnect) return;
#if not defined AccessPointEnable
if(WiFi.isConnected()) {returnMS(750); digitalWrite(2,!digitalRead(2)); return;}
mode++; switch(mode) {
case 1: case 3: digitalWrite(2,0); lagTime=30; break;
case 2: digitalWrite(2,1); lagTime=70; break;
case 4: digitalWrite(2,1); lagTime=250; mode=0; break;}
#else
returnMS(750); digitalWrite(2,!digitalRead(2));
#endif
}

void CountDownLed() {static boolean state=0; static uint32_t timer;
if(!RemoteXY.Torsher_led && !RemoteXY.Wall_led && !RemoteXY.Relax_led && !RemoteXY.Spot0_led && !RemoteXY.Spot1_led && !RemoteXY.Select_RGB_led &&
!RemoteXY.Mirror_RGB_led && !RemoteXY.Kitchen_Work_led && !RemoteXY.Kitchen_RGB_led && !RemoteXY.Bar_RGB_led && RemoteXY.NewYear_led!=1 &&
RemoteXY.Koridor_led!=1 && !RemoteXY.Bath_Mirror_led && !RemoteXY.Bath_led && RemoteXY.Vent_led!=1 && !RemoteXY.Bar_led && !RemoteXY.All_OFF_led &&
RemoteXY.Bath_RGB_led!=1 && !RemoteXY.Window_RGB_led && !RemoteXY.MaxLight_led && !RemoteXY.Start_RGB_led && !RemoteXY.Timer_select_led &&
!RemoteXY.min_led && !RemoteXY.sek_led && RemoteXY.WaitMode_led!=1) return;
if(!state) {state=1; timer=millis(); CountDownESPLed(1);} if(millis()-timer<200) return; //CMn("Отключаем оранж led, моргаем EPS Led");
state=0; RemoteXY.Torsher_led=0; RemoteXY.Wall_led=0; RemoteXY.Relax_led=0; RemoteXY.Spot0_led=0; RemoteXY.Spot1_led=0; RemoteXY.Select_RGB_led=0;
RemoteXY.Mirror_RGB_led=0; RemoteXY.Kitchen_Work_led=0; RemoteXY.Kitchen_RGB_led=0; RemoteXY.Bar_RGB_led=0; if(RemoteXY.NewYear_led==1) RemoteXY.NewYear_led=0;
(timerKoridor) ? RemoteXY.Koridor_led=2 : RemoteXY.Koridor_led=0; RemoteXY.Bath_led=0; RemoteXY.Bath_Mirror_led=0; if(RemoteXY.Vent_led==1) RemoteXY.Vent_led=0; RemoteXY.Bar_led=0; RemoteXY.All_OFF_led=0;
RemoteXY.Window_RGB_led=0; RemoteXY.MaxLight_led=0; if(!timerBathRGBEnable) {ignoreMoveBath ? RemoteXY.Bath_RGB_led=2 : RemoteXY.Bath_RGB_led=0;} else {
timerBathRGBEnable ? RemoteXY.Bath_RGB_led=3 : RemoteXY.Bath_RGB_led=0;} RemoteXY.Start_RGB_led=0; RemoteXY.Timer_select_led=0; RemoteXY.min_led=0; 
RemoteXY.sek_led=0; RemoteXY.WaitMode ? RemoteXY.WaitMode_led=2 : RemoteXY.WaitMode_led=0; state=0;}

void CountDownСnox(uint8_t Mode=0) {static boolean moveStart, cnoxSave, doorOpen, nightSwith; static uint32_t timer, nightTimer;
if(!doorOpen && save.autoMode && cnox && koridorSensor) doorOpen=1; // входная дверь открыта и нажата кнопка выхода в авто режиме
if(!nightSwith && nigtModeSwitch) {nightSwith=1; nightTimer=millis(); CMn("nigtModeSwitch ON");} if(nightSwith && millis()-nightTimer>200) {nightSwith=0; nigtModeSwitch=0; CMn("nigtModeSwitch reset");} switch (Mode) {
case 200: moveStart=0; RemoteXY.WaitMode=0; timerKoridor=0; cnox=0; CM("Дежурный"); if(nigtModeSwitch) {CM(" режим, ");} else {nigtMode=0; CM(", ночной режимы, ");} CMn("cnox сняты");
for_i(5,10) {waitLedInd[i]=0; autoOnLedNight[i]=0; autoOffLedNight[i]=0;} cnoxLedInd[5]=0; timerLedInd[5]=0; cnoxSave=0;
if(save.autoMode) {for_i(5,10) autoLedOn[i]=1;} else {for_i(5,10) autoLedOff[i]=1;} break;
case 1: moveStart=0; RemoteXY.WaitMode=0; timerKoridor=1; if(save.autoMode) {cnox=1; cnoxSave=1; cnoxLedInd[5]=1; CMn("Режим cnox ВКЛ");} else {
moveStart=1; timerVoice=1; timer=millis(); cnoxLedInd[5]=0; timerLedInd[5]=1; CMn("Режим cnox -> Wait");}
CM("cnoxSave="); CMn(cnoxSave); RemoteXY.Spot0=0; RemoteXY.Spot1=0; RemoteXY.Window_RGB=0; RemoteXY.Torsher=0; RemoteXY.Wall=0;
RemoteXY.Mirror_RGB=0; RemoteXY.Kitchen_Work=0; RemoteXY.Kitchen_RGB=0; RemoteXY.Bar=0; RemoteXY.Bar_RGB=0; waitModeStartTime=millis(); RemoteXY.Koridor=1;
RemoteXY.Bath=0; RemoteXY.Bath_Mirror=0; RemoteXY.Bath_RGB=0; RemoteXY.Vent=0; RemoteXY.NewYear=0; FIXLight(FixHard,1); break;
case 2: if(RemoteXY.WaitMode) {RemoteXY.WaitMode=0; timerKoridor=0; cnox=0; for_i(5,10) {waitLedInd[i]=0; autoOnLedNight[i]=0; autoOffLedNight[i]=0;}
if(save.autoMode) {for_i(5,10) autoLedOn[i]=1;} else {for_i(5,10) autoLedOff[i]=1;} waitModeStartTime=millis();
timerLedInd[5]=0; cnoxLedInd[5]=0; RemoteXY.Koridor=1; FIXLight(FixHard,1); CMn("Дежурный реж ВЫКЛ");} else if (!RemoteXY.WaitMode && cnox &&
!moveStart && !save.autoMode) {moveStart=1; timer=millis(); waitLedInd[5]=1; CMn("Датчик! Засекаем время");} break;
case 3: if(!RemoteXY.WaitMode && cnox) {cnoxLedInd[5]=0; timerLedInd[5]=0; moveStart=0; doorOpen=0; RemoteXY.WaitMode=1; for_i(5,10) waitLedInd[i]=1;
waitModeStartTime=millis(); timerKoridor=0; RemoteXY.Koridor=0; cnox=0; CMn("Дежурный режим ВКЛ");} break;}

if(Mode!=200) {returnSec(1);}
if(!cnox) {if(cnoxSave && !RemoteXY.WaitMode) {if(save.autoMode) {autoLedOn[5]=1;} else {autoLedOff[5]=1;} CMn("Режим cnox снят");} cnoxSave=0;}
if(!RemoteXY.WaitMode && moveStart && millis()-timer>save.Timer[0]*1000) {cnoxLedInd[5]=0; moveStart=0; timerLedInd[5]=0;
for_i(5,10) waitLedInd[i]=1; waitModeStartTime=millis(); timerKoridor=0; RemoteXY.Koridor=0; RemoteXY.WaitMode=1; cnox=0; CMn("Дежурный режим ВКЛ");}}

void CountDownBathSensor() {if(ignoreMoveBath) returnSec(1); static uint32_t timer; static boolean l_Bath, l_Bath_RGB, l_Bath_Mirror; static boolean firstStart;
if(l_Bath!=RemoteXY.Bath) {ignoreMoveBath=1; l_Bath=RemoteXY.Bath; RemoteXY.Bath_RGB_led=2; blockLedInd[9]=1; timer=millis(); CMn("Датчик в ванной запрещен");}
if(l_Bath_RGB!=RemoteXY.Bath_RGB) {ignoreMoveBath=1; l_Bath_RGB=RemoteXY.Bath_RGB; RemoteXY.Bath_RGB_led=2; blockLedInd[9]=1; timer=millis(); CMn("Датчик в ванной запрещен");}
if(l_Bath_Mirror!=RemoteXY.Bath_Mirror) {ignoreMoveBath=1; l_Bath_Mirror=RemoteXY.Bath_Mirror; RemoteXY.Bath_RGB_led=2; blockLedInd[9]=1; timer=millis(); CMn("Датчик в ванной запрещен");}
if(!firstStart) {timer=millis()-20001; firstStart=1;}
returnSec(1); if(!ignoreMoveBath || millis()-timer<5000) return; ignoreMoveBath=0; if(RemoteXY.Bath_RGB_led!=3) RemoteXY.Bath_RGB_led=0; blockLedInd[9]=0; CMn("Датчик в ванной разрешен");}

void CountDownBathRGB(boolean in=0) {if(!in) returnSec(1); static boolean stat; if((!save.autoMode || !stat) && !timerBathRGBEnable) {stat=0;
if(RemoteXY.Bath_RGB_led==3) RemoteXY.Bath_RGB_led=0; if(timerLedInd[9] && RemoteXY.Vent_led!=2) timerLedInd[9]=0; return;} static uint32_t timer;
if(!timerLedInd[9] && stat) timerLedInd[9]=1; if(stat && RemoteXY.Bath_RGB_led!=2) RemoteXY.Bath_RGB_led=3;
if(stat && (in || RemoteXY.Bath || RemoteXY.Bath_Mirror || !RemoteXY.Bath_RGB)) {stat=0; timerBathRGBEnable=0; if(RemoteXY.Vent_led!=2) timerLedInd[9]=0; CMn("Таймер отключения RGB ванны снят");}
if(!stat && timerBathRGBEnable) stat=1;
if(stat && !RemoteXY.Bath_RGB && !RemoteXY.Bath_Mirror && !RemoteXY.Bath) {timer=millis(); timerLedInd[9]=1; RemoteXY.Bath_RGB=1; CMn("Таймер отключения RGB ванны запущен");}
if(!stat || millis()-timer<save.Timer[1]*1000) return; RemoteXY.Bath_RGB=0; if(RemoteXY.Vent_led!=2) timerLedInd[9]=0; stat=0; timerBathRGBEnable=0; BathRGBautoOFF=1; CMn("RGB ванны отключена по таймеру");}

void CountDownVent() {returnSec(1); static boolean state=0; if(!save.autoMode || !RemoteXY.Vent) {if(RemoteXY.Vent_led) RemoteXY.Vent_led=0; if(!timerBathRGBEnable) timerLedInd[9]=0; return;} static uint32_t timer;
if(!timerLedInd[9] && RemoteXY.Vent_led==2) timerLedInd[9]=1;
if(RemoteXY.Vent_led==2 && (RemoteXY.Bath || RemoteXY.Bath_RGB || RemoteXY.Bath_Mirror)) {RemoteXY.Vent_led=0; if(RemoteXY.Bath_RGB_led!=3) timerLedInd[9]=0;}
if(!state && RemoteXY.Vent && (RemoteXY.Bath || RemoteXY.Bath_RGB || RemoteXY.Bath_Mirror)) state=1;
if(state && RemoteXY.Vent && !RemoteXY.Bath && !RemoteXY.Bath_RGB && !RemoteXY.Bath_Mirror) {state=0; timer=millis(); RemoteXY.Vent_led=2; timerLedInd[9]=1;}
if(RemoteXY.Vent_led!=2 || millis()-timer<save.Timer[2]*1000) return; RemoteXY.Vent=0; if(RemoteXY.Bath_RGB_led!=3) timerLedInd[9]=0; BathVentautoOFF=1;}

void CountDownNewYear() {static boolean state=0; if(!save.autoMode || !RemoteXY.TimerWindow_switch || !RemoteXY.NewYear) {
state=0; if(RemoteXY.level) RemoteXY.level=0; if(timerLedInd[6]) timerLedInd[6]=0;
if(RemoteXY.NewYear_led==2) RemoteXY.NewYear_led=0; if(RemoteXY.TimerWin_led) RemoteXY.TimerWin_led=0; return;} static uint32_t timer;
if(!state) {state=1; timer=millis(); RemoteXY.TimerWin_led=1; RemoteXY.NewYear_led=2; timerLedInd[6]=1; CMn("Запуск таймера розеток окон");}
returnSec(1); char outValue[4]; uint32_t offTime=(timer+save.Timer[3]*1000-millis())/1000;
RemoteXY.level=map(offTime,0,save.Timer[3],0,100); if(RemoteXY.NewYear_led==0) RemoteXY.NewYear_led=2;
 if(offTime<save.Timer[3]*1000) {CM("Отключение розеток окон после ");
if(offTime>60) {CM(offTime/60); CM(" мин "); if(offTime%60<10) CM("0"); CM(offTime%60); CMn(" сек");} else
{if(offTime<10) CM("0"); CM(offTime); CMn(" сек");}
strcpy(RemoteXY.CountDown_Window,"до авто отключения ");
if(offTime>60) {itoa(offTime/60,outValue,10); strcat(RemoteXY.CountDown_Window,outValue); strcat(RemoteXY.CountDown_Window," м. ");
if(offTime%60<10) strcat(RemoteXY.CountDown_Window,"0"); itoa(offTime%60,outValue,10); strcat(RemoteXY.CountDown_Window,outValue);} else
{if(offTime<10) strcat(RemoteXY.CountDown_Window,"0"); itoa(offTime,outValue,10); strcat(RemoteXY.CountDown_Window,outValue);} strcat(RemoteXY.CountDown_Window," c.");}
if(RemoteXY.NewYear_led!=2 || millis()-timer<save.Timer[3]*1000) return; RemoteXY.NewYear=0; RemoteXY.NewYear_led=0; RemoteXY.TimerWin_led=0; RemoteXY.level=0; 
timerLedInd[6]=0; syncLedInd[6]=1; CMn("Авто выкл розеток по таймеру");}

boolean LightOFFAllWithoutBathRoom(boolean in=0) {// 1-проверка, 0-выключить все кроме кухни
if(in) {if(!RemoteXY.Spot0 && !RemoteXY.Spot1 && !RemoteXY.Window_RGB && !RemoteXY.Torsher && !RemoteXY.Wall && !RemoteXY.Mirror_RGB && !RemoteXY.Kitchen_Work &&
!RemoteXY.Kitchen_RGB && !RemoteXY.Bar && !RemoteXY.Bar_RGB && !RemoteXY.Koridor && !RemoteXY.NewYear) return(1); else return(0);}
RemoteXY.Spot0=0; RemoteXY.Spot1=0; RemoteXY.Window_RGB=0; RemoteXY.Torsher=0; RemoteXY.Wall=0; RemoteXY.Mirror_RGB=0; RemoteXY.Kitchen_Work=0;
RemoteXY.Kitchen_RGB=0; RemoteXY.Bar=0; RemoteXY.Bar_RGB=0; RemoteXY.Koridor=0; RemoteXY.NewYear=0; return(1);}

boolean LightOFFAll(boolean in=0) {// 1-проверка, 0-выключить весь свет в квартире
if(in) {if(!RemoteXY.Spot0 && !RemoteXY.Spot1 && !RemoteXY.Window_RGB && !RemoteXY.Torsher && !RemoteXY.Wall && !RemoteXY.Mirror_RGB && !RemoteXY.Kitchen_Work &&
!RemoteXY.Kitchen_RGB && !RemoteXY.Bar && !RemoteXY.Bar_RGB && !RemoteXY.Koridor && !RemoteXY.NewYear && !RemoteXY.Bath && !RemoteXY.Bath_RGB && !RemoteXY.Bath_Mirror && !RemoteXY.Vent) return(1); else return(0);}
RemoteXY.Spot0=0; RemoteXY.Spot1=0; RemoteXY.Window_RGB=0; RemoteXY.Torsher=0; RemoteXY.Wall=0; RemoteXY.Mirror_RGB=0; RemoteXY.Kitchen_Work=0;
RemoteXY.Kitchen_RGB=0; RemoteXY.Bar=0; RemoteXY.Bar_RGB=0; RemoteXY.Koridor=0; RemoteXY.NewYear=0; RemoteXY.Bath=0; RemoteXY.Bath_RGB=0; RemoteXY.Bath_Mirror=0; RemoteXY.Vent=0; return(1);}

#if defined RelayBus
void rBus(uint16_t data=0) {static uint16_t send_Codes[10], lastReciveCode=0, lastDataCode, repeatCount; static uint8_t statusBus;
static sendStrukt CODE; static sendStrukt seviceCOD; static sendStrukt relayData;
static uint32_t sendTimer, confirmTimer, inTimer=millis()-500, verifyTimer=millis(); //static bCode bSt; if(bSt!=busState) {bSt=busState; CM("----------------------- Bus state "); CMn(bSt);}
static boolean AlarmState;
if(data) {if(data!=318) {CM("--> rData: "); CMn(data);} if(!lastDataCode) {inTimer=millis(); CMn("Check timer");}} if(lastDataCode && millis()-inTimer>400) lastDataCode=0;
relayBus.tick(); if(relayBus.statusChanged()) {statusBus=relayBus.getStatus(); if(statusBus==7 || statusBus==2) {if(!AlarmState) {AlarmState=1; CMn("rBus error");}} else if(AlarmState) {AlarmState=0; CMn("rBus Ok");}}
if(data && (data!=lastDataCode || millis()-inTimer>400)) {lastDataCode=data; CMn("Check timer reset"); for_i(0,10) {if(!send_Codes[i]) {send_Codes[i]=data; CM("In Data "); CM(data); CM(" -> cell "); CMn(i); break;}}}
if(send_Codes[0] && !relayData.val) {relayData.val=send_Codes[0]; CM("Send Data "); CM(relayData.val); CM(" cell <- "); CMn(0);}
if(!AlarmState && relayData.val && !statusBus && (relayData.val<1000 || millis()-sendTimer>400)) {CM("Timer: "); CMn(millis()-sendTimer); relayData.val>1000 ? repeatCount++ : repeatCount=0;
relayBus.sendData(2, relayData); CM("<==== rCode ");
CMn(relayData.val); if(relayData.val<1000) relayData.val+=2000; sendTimer=millis();}
if(!AlarmState && seviceCOD.val && !statusBus && millis()-confirmTimer>5) {CM("<= rservice code "); CMn(seviceCOD.val); relayBus.sendData(2, seviceCOD); seviceCOD.val=0; confirmTimer=millis();}
relayBus.tick(); if(relayBus.statusChanged()) {statusBus=relayBus.getStatus(); if(statusBus==7 || statusBus==2) {if(!AlarmState) {AlarmState=1; CMn("rBus error");}} else if(AlarmState) {AlarmState=0; CMn("rBus Ok");}}
if(relayBus.gotData()) {relayBus.readData(CODE); CM("====> rBus "); CMn(CODE.val); verifyTimer=millis();
if(CODE.val==2000) {CM("rCod = 2000  "); relayData.val=0; CMn("Repeat send code");}
else if(CODE.val>2000) {CM("rCod > 2000  "); if(lastReciveCode==CODE.val-2000) {seviceCOD.val=lastReciveCode+1000; CM("<= rConfirm again "); CMn(seviceCOD.val);} else {
seviceCOD.val=2000; CMn("<= rThis code not read");}}
else if(CODE.val>1000) {CM("rCod > 1000  "); for_i(0,10) {if(send_Codes[i]==CODE.val-1000) {CM("rCode "); CM(send_Codes[i]);
CMn(" >>>>>>> OK"); repeatCount=0; for_i(0,9) send_Codes[i]=send_Codes[i+1]; send_Codes[9]=0; relayData.val=0; break;}}}
else if(CODE.val<1000) {CM("rCod < 1000  "); seviceCOD.val=CODE.val+1000; lastReciveCode=CODE.val; CM("<= rConfirm "); CMn(seviceCOD.val);
if(CODE.val>=100 && CODE.val<=136) {CMn("Восстановили состояние реле");}
switch (CODE.val) {
case 101: CMn("PIN 1 On"); break;
case 102: CMn("PIN 2 On"); break;
case 103: CMn("PIN 3 On"); break;
case 104: CMn("Spot2 On"); if(!RemoteXY.Spot1) processRestoreRelay=1; RemoteXY.Spot1=1; break;
case 105: CMn("Wall On"); if(!RemoteXY.Wall) processRestoreRelay=1; RemoteXY.Wall=1; break;
case 106: CMn("Bar On"); if(!RemoteXY.Bar) processRestoreRelay=1; RemoteXY.Bar=1; break;
case 107: CMn("Vent On"); if(!RemoteXY.Vent) processRestoreRelay=1; RemoteXY.Vent=1; break;
case 108: CMn("Bath Room On"); if(!RemoteXY.Bath) processRestoreRelay=1; RemoteXY.Bath=1; break;
case 109: CMn("PIN 9 On"); break;
case 110: CMn("Kitchen Work On"); if(!RemoteXY.Kitchen_Work) processRestoreRelay=1; RemoteXY.Kitchen_Work=1; break;
case 111: CMn("Bath Mirror On"); if(!RemoteXY.Bath_Mirror) processRestoreRelay=1; RemoteXY.Bath_Mirror=1; break;
case 112: CMn("Koridor On"); if(!RemoteXY.Koridor) processRestoreRelay=1; RemoteXY.Koridor=1; break;
case 113: CMn("Spot1 On"); if(!RemoteXY.Spot0) processRestoreRelay=1; RemoteXY.Spot0=1; break;
case 114: CMn("Torsher On"); if(!RemoteXY.Torsher) processRestoreRelay=1; RemoteXY.Torsher=1; break;
case 115: CMn("NewYear On"); if(!RemoteXY.NewYear) processRestoreRelay=1; RemoteXY.NewYear=1; break;
case 116: CMn("PIN 16 On"); break;
case 121: CMn("PIN 1 Off"); break;
case 122: CMn("PIN 2 Off"); break;
case 123: CMn("PIN 3 Off"); break;
case 124: CMn("Spot2 Off"); if(RemoteXY.Spot1) processRestoreRelay=1; RemoteXY.Spot1=0; break;
case 125: CMn("Wall Off"); if(RemoteXY.Wall) processRestoreRelay=1; RemoteXY.Wall=0; break;
case 126: CMn("Bar Off"); if(RemoteXY.Bar) processRestoreRelay=1; RemoteXY.Bar=0; break;
case 127: CMn("Vent Off"); if(RemoteXY.Vent) processRestoreRelay=1; RemoteXY.Vent=0; break;
case 128: CMn("Bath Room Off"); if(RemoteXY.Bath) processRestoreRelay=1; RemoteXY.Bath=0; break;
case 129: CMn("PIN 9 Off"); break;
case 130: CMn("Kitchen Work Off"); if(RemoteXY.Kitchen_Work) processRestoreRelay=1; RemoteXY.Kitchen_Work=0; break;
case 131: CMn("Bath Mirror Off"); if(RemoteXY.Bath_Mirror) processRestoreRelay=1; RemoteXY.Bath_Mirror=0; break;
case 132: CMn("Koridor Off"); if(RemoteXY.Koridor) processRestoreRelay=1; RemoteXY.Koridor=0; break;
case 133: CMn("Spot1 Off"); if(RemoteXY.Spot0) processRestoreRelay=1; RemoteXY.Spot0=0; break;
case 134: CMn("Torsher Off"); if(RemoteXY.Torsher) processRestoreRelay=1; RemoteXY.Torsher=0; break;
case 135: CMn("NewYear Off"); if(RemoteXY.NewYear) processRestoreRelay=1; RemoteXY.NewYear=0; break;
case 136: CMn("PIN 16 On"); break;
case 218: CMn("Sync Relay"); break;
default: if(CODE.val) {CM("Неизвестный код: "); CMn(CODE.val);}} if(CODE.val && CODE.val!=2 && CODE.val<140) CountDownСnox(200); CODE.val=0;}}
if(millis()-verifyTimer>20000 || repeatCount>50) {CMn("Connection error. Reset"); delay(100); ESP.restart();}}
#endif

void sBus(uint16_t data=0) {static uint16_t send_Codes[10], lastReciveCode=0, lastDataCode, repeatCount; static uint8_t statusBus;
static sendStrukt CODE; static sendStrukt seviceCOD; static sendStrukt sendData;
static uint32_t sendTimer, confirmTimer, inTimer=millis()-500, checkTimer, verifyTimer=millis(); //static bCode bSt; if(bSt!=busState) {bSt=busState; CM("----------------------- Bus state "); CMn(bSt);}
if(millis()-checkTimer>5000) {data=318; checkTimer=millis();} static boolean AlarmState;
if(data) {CM("--> Data: "); CMn(data); if(!lastDataCode) {inTimer=millis(); CMn("Check timer");}} if(lastDataCode && millis()-inTimer>400) lastDataCode=0;
bus.tick(); if(bus.statusChanged()) {statusBus=bus.getStatus(); if(statusBus==7 || statusBus==2) {if(!AlarmState) {AlarmState=1; CMn("Bus error");}} else if(AlarmState) {AlarmState=0; CMn("Bus Ok");}}
if(data==318) {boolean alredy318=0; for_i(0,10) if(send_Codes[i]==318) {alredy318=1; CM("318 alredy in cell "); CMn(i); break;} if(!alredy318) for_i(0,10) {if(!send_Codes[i]) {send_Codes[i]=data; CM("In Data "); CM(data); CM(" -> cell "); CMn(i); break;}}} else
if(data && (data!=lastDataCode || millis()-inTimer>400)) {lastDataCode=data; CMn("Check timer reset"); for_i(0,10) {if(!send_Codes[i]) {send_Codes[i]=data; CM("In Data "); CM(data); CM(" -> cell "); CMn(i); break;}}}
if(send_Codes[0] && !sendData.val) {sendData.val=send_Codes[0]; CM("Send Data "); CM(sendData.val); CM(" cell <- "); CMn(0);}
if(!AlarmState && sendData.val && !statusBus && (sendData.val<1000 || millis()-sendTimer>400)) {CM("Timer: "); CMn(millis()-sendTimer); sendData.val>1000 ? repeatCount++ : repeatCount=0;
bus.sendData(5, sendData); CM("<==== Code "); CMn(sendData.val); if(sendData.val<1000) sendData.val+=2000; sendTimer=millis();}
if(!AlarmState && seviceCOD.val && !statusBus && millis()-confirmTimer>5) {bus.sendData(5, seviceCOD); CM("<= service code "); CMn(seviceCOD.val); seviceCOD.val=0; confirmTimer=millis();}
bus.tick(); if(bus.statusChanged()) {statusBus=bus.getStatus(); if(statusBus==7 || statusBus==2) {if(!AlarmState) {AlarmState=1; CMn("Bus error");}} else if(AlarmState) {AlarmState=0; CMn("Bus Ok");}}
if(bus.gotData()) {bus.readData(CODE); CM("====> Code "); CMn(CODE.val); verifyTimer=millis();
if(CODE.val==2000) {CM("Cod = 2000  "); sendData.val=0; CMn("Repeat send code");}
else if(CODE.val>2000) {CM("Cod > 2000  "); if(lastReciveCode==CODE.val-2000) {seviceCOD.val=lastReciveCode+1000; CM("<= Confirm again "); CMn(seviceCOD.val);} else {
seviceCOD.val=2000; CMn("<= This code not read");}}
else if(CODE.val>1000) {CM("Cod > 1000  "); for_i(0,10) {if(send_Codes[i]==CODE.val-1000) {CM("Code "); CM(send_Codes[i]);
CMn(" >>>>>>>>>>>> OK"); repeatCount=0; for_i(0,9) send_Codes[i]=send_Codes[i+1]; send_Codes[9]=0; sendData.val=0; break;}}}
else if(CODE.val<1000) {CM("Cod < 1000  "); seviceCOD.val=CODE.val+1000; lastReciveCode=CODE.val; CM("<= Confirm "); CMn(seviceCOD.val);

if(CODE.val>151 && CODE.val<217) {CM("Click кнопки: "); float Btnkf=(CODE.val-136)*5; Btnkf=Btnkf/100; CM(Btnkf); CM(" : ");
char value[8]; dtostrf(500*Btnkf,1,0,value); CM(value); CMn(" мсек."); BtnHOLD=500*Btnkf; sprintf(RemoteXY.Button_text,"Двойной клик: %d мс.",BtnHOLD); CODE.val=0; return;}

switch (CODE.val) {
case 1: if(!RemoteXY.Relax) {FIXLightWithoutBath(FixState,6); RemoteXY.Relax=1;} else if(RemoteXY.Relax && !FIXLightWithoutBath(RestoreState,6)) RemoteXY.Relax=0;  break;
case 2: if(RemoteXY.WaitMode && !RemoteXY.AutoMode_switch) RemoteXY.MaxLight=1; else if(!FIXLight(FixCheck,1)) CountDownСnox(1); else RemoteXY.MaxLight=1; break;
case 3: if(!FIXLightWithoutBath(RestoreState,1)) {if(!LightOFFAllWithoutBathRoom(1)) FIXLightWithoutBath(FixState,1); LightOFFAllWithoutBathRoom(0);} break;
case 4: RemoteXY.Mirror_RGB=!RemoteXY.Mirror_RGB; break;
case 5: if(!FIXLightWithoutBath(RestoreState,1)) {if(!LightOFFAllWithoutBathRoom(1)) FIXLightWithoutBath(FixState,1); LightOFFAllWithoutBathRoom(0);} break;
case 6: if(!FIXLightWithoutBath(RestoreState,1)) {if(!LightOFFAllWithoutBathRoom(1)) FIXLightWithoutBath(FixState,1); LightOFFAllWithoutBathRoom(0);} break;
case 7: if(!FIXLightWithoutBath(RestoreState,7)) {if(!RemoteXY.Koridor) {FIXLightWithoutBath(FixState,7); LightOFFAllWithoutBathRoom(); RemoteXY.Koridor=1;} else RemoteXY.Koridor=0;} break;
case 8: if(!FIXLightWithoutBath(RestoreState,8)) {if(!LightOFFAllWithoutBathRoom(1)) FIXLightWithoutBath(FixState,8); LightOFFAll(0);} break;
case 10: if(!RemoteXY.Koridor && !FIXLightWithoutBath(RestoreState,9)) {FIXLightWithoutBath(FixState,9); LightOFFAllWithoutBathRoom(); RemoteXY.Koridor=1;} break;
case 11: if(!FIXLightWithoutBath(RestoreState,8)) {if(!LightOFFAllWithoutBathRoom(1)) FIXLightWithoutBath(FixState,8); LightOFFAll(0);} break;
case 13: RemoteXY.AutoMode_switch=1; for_i(5,10) syncLedInd[i]=1; break;
case 14: RemoteXY.AutoMode_switch=0; for_i(5,10) syncLedInd[i]=1; break;
case 15: for_i(5,10) syncLedInd[i]=1; saveData1; CMn("Звук включен"); if(!save.Zumer) voice(1); save.Zumer=1; RemoteXY.Block_Sound=0; break;
case 16: for_i(5,10) syncLedInd[i]=1; saveData1; CMn("Звук отключен"); if(save.Zumer) voice(2); save.Zumer=0; RemoteXY.Block_Sound=1; break;
case 21: if(timerBathRGBEnable) {syncLedInd[9]=1; CountDownBathRGB(1);} else if (!FIXBath(RestoreState,0)) {if(!RemoteXY.Bath_RGB) {FIXBath(FixState,0); RemoteXY.Bath=0; RemoteXY.Bath_RGB=1; RemoteXY.Bath_Mirror=0;} else RemoteXY.Bath_RGB=0;} break;
case 23: if(!FIXLightWithoutBath(RestoreState,2)) {if(!LightOFFAllWithoutBathRoom(2)) FIXLightWithoutBath(FixState,2); LightOFFAllWithoutBathRoom(0);} break;
case 22: if(RemoteXY.Bath || RemoteXY.Bath_RGB || RemoteXY.Bath_Mirror) {RemoteXY.Bath=0; RemoteXY.Bath_RGB=0; RemoteXY.Bath_Mirror=0;} else {RemoteXY.Bath=1; RemoteXY.Bath_RGB=1; RemoteXY.Bath_Mirror=1;} break;
case 24: RemoteXY.Bath_Mirror=!RemoteXY.Bath_Mirror; break;
case 25: if(!FIXLightWithoutBath(RestoreState,2)) {if(!LightOFFAllWithoutBathRoom(2)) FIXLightWithoutBath(FixState,2); LightOFFAllWithoutBathRoom(0);} break;
case 26: if(!FIXLightWithoutBath(RestoreState,2)) {if(!LightOFFAllWithoutBathRoom(2)) FIXLightWithoutBath(FixState,2); LightOFFAllWithoutBathRoom(0);} break;
case 27: if (!FIXBath(RestoreState,1)) {if(!RemoteXY.Bath) {FIXBath(FixState,1); RemoteXY.Bath=1; RemoteXY.Bath_RGB=0;} else RemoteXY.Bath=0;} break;
case 28: RemoteXY.Vent=!RemoteXY.Vent; break;
case 30: if (!FIXBath(RestoreState,1)) {if(!RemoteXY.Bath) {FIXBath(FixState,1); RemoteXY.Bath=1; RemoteXY.Bath_RGB=0;} else RemoteXY.Bath=0;} break;
case 31: RemoteXY.Vent=!RemoteXY.Vent; break;
case 33: RemoteXY.AutoMode_switch=1; for_i(5,10) syncLedInd[i]=1; break;
case 34: RemoteXY.AutoMode_switch=0; for_i(5,10) syncLedInd[i]=1; break;
case 35: for_i(5,10) syncLedInd[i]=1; saveData1; CMn("Звук включен"); if(!save.Zumer) voice(1); save.Zumer=1; RemoteXY.Block_Sound=0; break;
case 36: for_i(5,10) syncLedInd[i]=1; saveData1; CMn("Звук отключен"); if(save.Zumer) voice(2); save.Zumer=0; RemoteXY.Block_Sound=1; break;
case 41: RemoteXY.Kitchen_Work=!RemoteXY.Kitchen_Work; break;
case 42: if(RemoteXY.Kitchen_Work || RemoteXY.Kitchen_RGB) {RemoteXY.Kitchen_Work=0; RemoteXY.Kitchen_RGB=0;} else {RemoteXY.Kitchen_Work=1; RemoteXY.Kitchen_RGB=1;} break;
case 43: if(!FIXLightWithoutBath(RestoreState,3)) {if(!LightOFFAllWithoutBathRoom(3)) FIXLightWithoutBath(FixState,3); LightOFFAllWithoutBathRoom(0);} break;
case 44: RemoteXY.Kitchen_Work=1; RemoteXY.Kitchen_RGB=1; break;
case 45: if(!FIXLightWithoutBath(RestoreState,3)) {if(!LightOFFAllWithoutBathRoom(3)) FIXLightWithoutBath(FixState,3); LightOFFAllWithoutBathRoom(0);} break;
case 46: if(!FIXLightWithoutBath(RestoreState,3)) {if(!LightOFFAllWithoutBathRoom(3)) FIXLightWithoutBath(FixState,3); LightOFFAllWithoutBathRoom(0);} break;
case 47: RemoteXY.Kitchen_RGB=!RemoteXY.Kitchen_RGB; break;
case 48: if(!FIXLightWithoutBath(RestoreState,12)) {if(!LightOFFAllWithoutBathRoom(1)) FIXLightWithoutBath(FixState,12); LightOFFAll(0);} break;
case 50: RemoteXY.Kitchen_RGB=!RemoteXY.Kitchen_RGB; break;
case 51: if(!FIXLightWithoutBath(RestoreState,12)) {if(!LightOFFAllWithoutBathRoom(1)) FIXLightWithoutBath(FixState,12); LightOFFAll(0);} break;
case 53: RemoteXY.AutoMode_switch=1; for_i(5,10) syncLedInd[i]=1; break;
case 54: RemoteXY.AutoMode_switch=0; for_i(5,10) syncLedInd[i]=1; break;
case 55: for_i(5,10) syncLedInd[i]=1; saveData1; CMn("Звук включен"); if(!save.Zumer) voice(1); save.Zumer=1; RemoteXY.Block_Sound=0; break;
case 56: for_i(5,10) syncLedInd[i]=1; saveData1; CMn("Звук отключен"); if(save.Zumer) voice(2); save.Zumer=0; RemoteXY.Block_Sound=1; break;
case 61: RemoteXY.Bar=!RemoteXY.Bar; break;
case 63: if(!FIXLightWithoutBath(RestoreState,4)) {if(!LightOFFAllWithoutBathRoom(4)) FIXLightWithoutBath(FixState,4); LightOFFAllWithoutBathRoom(0);} break;
case 62: if(RemoteXY.Bar || RemoteXY.Bar_RGB) {RemoteXY.Bar=0; RemoteXY.Bar_RGB=0;} else {RemoteXY.Bar=1; RemoteXY.Bar_RGB=1;} break;
case 64: RemoteXY.Bar=1; RemoteXY.Bar_RGB=1; break;
case 65: if(!FIXLightWithoutBath(RestoreState,4)) {if(!LightOFFAllWithoutBathRoom(4)) FIXLightWithoutBath(FixState,4); LightOFFAllWithoutBathRoom(0);} break;
case 66: if(!FIXLightWithoutBath(RestoreState,4)) {if(!LightOFFAllWithoutBathRoom(4)) FIXLightWithoutBath(FixState,4); LightOFFAllWithoutBathRoom(0);} break;
case 67: RemoteXY.Bar_RGB=!RemoteXY.Bar_RGB; break;
case 68: if(!FIXLightWithoutBath(RestoreState,10)) {if(!LightOFFAllWithoutBathRoom(1)) FIXLightWithoutBath(FixState,10); LightOFFAll(0);} break;
case 70: RemoteXY.Bar_RGB=!RemoteXY.Bar_RGB; break;
case 71: if(!FIXLightWithoutBath(RestoreState,10)) {if(!LightOFFAllWithoutBathRoom(1)) FIXLightWithoutBath(FixState,10); LightOFFAll(0);} break;
case 73: RemoteXY.AutoMode_switch=1; for_i(5,10) syncLedInd[i]=1; break;
case 74: RemoteXY.AutoMode_switch=0; for_i(5,10) syncLedInd[i]=1; break;
case 75: for_i(5,10) syncLedInd[i]=1; saveData1; CMn("Звук включен"); if(!save.Zumer) voice(1); save.Zumer=1; RemoteXY.Block_Sound=0; break;
case 76: for_i(5,10) syncLedInd[i]=1; saveData1; CMn("Звук отключен"); if(save.Zumer) voice(2); save.Zumer=0; RemoteXY.Block_Sound=1; break;
case 81: if(!RemoteXY.Relax) {FIXLightWithoutBath(FixState,11); RemoteXY.Relax=1;} else if(RemoteXY.Relax && !FIXLightWithoutBath(RestoreState,11)) RemoteXY.Relax=0;  break;
case 82: if(!nigtMode) {FIXLight(FixState,0); LightOFFAll(0); nigtModeSwitch=1; nigtMode=1;} else RemoteXY.Window_RGB=1; break;
case 83: if(!FIXLightWithoutBath(RestoreState,5)) {if(!LightOFFAllWithoutBathRoom(1)) FIXLightWithoutBath(FixState,5); LightOFFAllWithoutBathRoom(0);} break;
case 84: if(!RemoteXY.Spot0 && !RemoteXY.Spot1) {RemoteXY.Spot0=1; RemoteXY.Spot1=1; RemoteXY.Torsher=0; RemoteXY.Wall=0; RemoteXY.Window_RGB=0; RemoteXY.Mirror_RGB=0; RemoteXY.Kitchen_RGB=0; RemoteXY.Bar_RGB=0;} else 
if(RemoteXY.Spot0) {RemoteXY.Spot0=0; RemoteXY.Spot1=1;} else {RemoteXY.Spot0=1; RemoteXY.Spot1=0;} break;
case 85: if(!RemoteXY.Torsher) {FIXLightWithoutBath(FixState,0); RemoteXY.Koridor=0; RemoteXY.Spot0=0; RemoteXY.Spot1=0; RemoteXY.Bar=0; RemoteXY.Kitchen_Work=0; RemoteXY.Torsher=1; RemoteXY.Wall=1; RemoteXY.Window_RGB=1;} else if(!FIXLightWithoutBath(RestoreState,0)) RemoteXY.Torsher=0; break;
case 86: if(!FIXLightWithoutBath(RestoreState,5)) {if(!LightOFFAllWithoutBathRoom(1)) FIXLightWithoutBath(FixState,5); LightOFFAllWithoutBathRoom(0);} break;
case 87: if(!RemoteXY.Window_RGB) {FIXRoom(FixState,1); RemoteXY.Spot0=0; RemoteXY.Spot1=0; RemoteXY.Torsher=0; RemoteXY.Wall=0; RemoteXY.Window_RGB=1;} else if(!FIXRoom(RestoreState,1)) RemoteXY.Window_RGB=0; break;
case 88: RemoteXY.Wall=!RemoteXY.Wall; break;
case 90: RemoteXY.NewYear=!RemoteXY.NewYear; break;
case 91: if(!FIXLight(RestoreAll,0)) {FIXLight(FixState,0); LightOFFAll(0);} break;
case 93: RemoteXY.AutoMode_switch=1; for_i(5,10) syncLedInd[i]=1; break;
case 94: RemoteXY.AutoMode_switch=0; for_i(5,10) syncLedInd[i]=1; break;
case 95: for_i(5,10) syncLedInd[i]=1; saveData1; CMn("Звук включен"); if(!save.Zumer) voice(1); save.Zumer=1; RemoteXY.Block_Sound=0; break;
case 96: for_i(5,10) syncLedInd[i]=1; saveData1; CMn("Звук отключен"); if(save.Zumer) voice(2); save.Zumer=0; RemoteXY.Block_Sound=1; break;
case 100: CMn("Программирование клика кнопок"); syncLedInd[5]=1; voice(61); for_i(5,10) progLedInd[i]=1; break;
case 101: CMn("Тренировка времени клика кнопок"); syncLedInd[5]=1; voice(62); for_i(5,10) progLedInd[i]=1; break;
case 103: CMn("Программирование пульта"); syncLedInd[5]=1; voice(63); for_i(5,10) progLedInd[i]=1; break;
case 104: CMn("Нажмите кнопку"); syncLedInd[5]=1; voice(64); break;
case 105: CMn("Программирование успешно завершено"); syncLedInd[5]=1; voice(65); for_i(5,10) progLedInd[i]=0; break;
case 106: CMn("Программирование отменено"); syncLedInd[5]=1; voice(66); for_i(5,10) progLedInd[i]=0; break;
case 107: CMn("Пульт уже есть в памяти"); syncLedInd[5]=1; voice(59); break;
case 108: CMn("Первая кнопка пульта"); syncLedInd[5]=1; voice(96); break;
case 109: CMn("Вторая кнопка пульта"); syncLedInd[5]=1; voice(97); break;
case 110: CMn("Третья кнопка пульта"); syncLedInd[5]=1; voice(98); break;
case 111: CMn("Четвертая кнопка пульта"); syncLedInd[5]=1; voice(99); break;
case 112: CMn("Пятая кнопка пульта"); syncLedInd[5]=1; voice(100); break;
case 113: CMn("Шестая кнопка пульта"); syncLedInd[5]=1; voice(101); break;
case 114: CMn("Седьмая кнопка пульта"); syncLedInd[5]=1; voice(102); break;
case 115: CMn("Восьмая кнопка пульта"); syncLedInd[5]=1; voice(103); break;
case 116: CMn("Нажатие кнопки"); syncLedInd[5]=1; voice(115); break;
case 117: CMn("Двойное нажатие/удержание кнопки"); syncLedInd[5]=1; voice(116); break;
case 118: CMn("Кнопка пропущена"); syncLedInd[5]=1; voice(117); break;
case 119: CMn("Кнопка уже сохранена"); syncLedInd[5]=1; voice(120); break;
case 120: CMn("Все сохраненные пульты удалены"); syncLedInd[5]=1; voice(121); break;
case 121: if(!RemoteXY.Relax) {FIXLightWithoutBath(FixState,11); RemoteXY.Relax=1;} else if(RemoteXY.Relax && !FIXLightWithoutBath(RestoreState,11)) RemoteXY.Relax=0;  break;
case 122: RemoteXY.Window_RGB=!RemoteXY.Window_RGB; break;
case 123: RemoteXY.MaxLight=!RemoteXY.MaxLight; break;
case 124: RemoteXY.Spot0=!RemoteXY.Spot0; break;
case 125: RemoteXY.Koridor=!RemoteXY.Koridor; break;
case 126: RemoteXY.Mirror_RGB=!RemoteXY.Mirror_RGB; break;
case 127: if(!FIXLightWithoutBath(RestoreState,5)) {if(!LightOFFAllWithoutBathRoom(1)) FIXLightWithoutBath(FixState,5); LightOFFAllWithoutBathRoom(0);} break;
RemoteXY.Spot0=0; RemoteXY.Spot1=0; RemoteXY.Window_RGB=0; RemoteXY.Torsher=0; RemoteXY.Wall=0; RemoteXY.Mirror_RGB=0; RemoteXY.Kitchen_Work=0;
RemoteXY.Kitchen_RGB=0; RemoteXY.Bar=0; RemoteXY.Bar_RGB=0; RemoteXY.Koridor=0; RemoteXY.NewYear=0; break;
case 128: RemoteXY.Spot1=!RemoteXY.Spot1; break;
case 129: RemoteXY.Bar=!RemoteXY.Bar; break;
case 130: RemoteXY.Bar_RGB=!RemoteXY.Bar_RGB; break;
case 131: RemoteXY.Kitchen_Work=!RemoteXY.Kitchen_Work; break;
case 132: RemoteXY.Kitchen_RGB=!RemoteXY.Kitchen_RGB; break;
case 133: if(!RemoteXY.Torsher) {FIXLightWithoutBath(FixState,0); RemoteXY.Koridor=0; RemoteXY.Spot0=0; RemoteXY.Spot1=0; RemoteXY.Bar=0; RemoteXY.Kitchen_Work=0; RemoteXY.Torsher=1; RemoteXY.Wall=1; RemoteXY.Window_RGB=1;} else if(!FIXLightWithoutBath(RestoreState,0)) RemoteXY.Torsher=0; break;
case 134: RemoteXY.Wall=!RemoteXY.Wall; break;
case 135: if(!nigtMode) {FIXLight(FixState,0); LightOFFAll(0); nigtModeSwitch=1; nigtMode=1;} else RemoteXY.Window_RGB=1; break;
case 136: RemoteXY.NewYear=!RemoteXY.NewYear; break;
case 140: CMn("Входная дверь закрыта");  if(save.autoMode) {CountDownСnox(3); koridorSensor=1;} break;
case 141: CMn("Входная дверь открыта"); if(save.autoMode) {CountDownСnox(2); koridorSensor=0;} syncLedInd[5]=1; break;
case 142: if(!ignoreMoveBath && save.autoMode) {CMn("Сенсор в ванне ВКЛ"); if(save.autoMode && !RemoteXY.Bath_RGB && !RemoteXY.Bath_Mirror && !RemoteXY.Bath) timerBathRGBEnable=1;} break;
case 143: CMn("Сенсор в ванне ВЫКЛ"); break;
case 144: Alarm(1); CMn("Сенсор сигнализации ВКЛ"); break;
case 145: Alarm(0); CMn("Сенсор сигнализации ВЫКЛ"); break;
default: if(CODE.val) {CM("Неизвестный код: "); CMn(CODE.val);}} if(CODE.val && CODE.val!=2 && CODE.val<140) CountDownСnox(200); CODE.val=0;}}
if(millis()-verifyTimer>20000 || repeatCount>50) {CMn("Connection error. Reset"); delay(100); ESP.restart();}}

void buttonLed() {static uint32_t timer; if(millis()-timer<100) return; timer=millis(); static CRGB saveLed[5];
if(saveLed[0][0]!=ledsw[0].r) {saveLed[0][0]=ledsw[0].r; RemoteXY.But0_led_r=ledsw[0].r;}
if(saveLed[0][1]!=ledsw[0].g) {saveLed[0][1]=ledsw[0].g; RemoteXY.But0_led_g=ledsw[0].g;}
if(saveLed[0][2]!=ledsw[0].b) {saveLed[0][2]=ledsw[0].b; RemoteXY.But0_led_b=ledsw[0].b;}
if(saveLed[1][0]!=ledsw[1].r) {saveLed[1][0]=ledsw[1].r; RemoteXY.But1_led_r=ledsw[1].r;}
if(saveLed[1][1]!=ledsw[1].g) {saveLed[1][1]=ledsw[1].g; RemoteXY.But1_led_g=ledsw[1].g;}
if(saveLed[1][2]!=ledsw[1].b) {saveLed[1][2]=ledsw[1].b; RemoteXY.But1_led_b=ledsw[1].b;}
if(saveLed[2][0]!=ledsw[2].r) {saveLed[2][0]=ledsw[2].r; RemoteXY.But2_led_r=ledsw[2].r;}
if(saveLed[2][1]!=ledsw[2].g) {saveLed[2][1]=ledsw[2].g; RemoteXY.But2_led_g=ledsw[2].g;}
if(saveLed[2][2]!=ledsw[2].b) {saveLed[2][2]=ledsw[2].b; RemoteXY.But2_led_b=ledsw[2].b;}
if(saveLed[3][0]!=ledsw[3].r) {saveLed[3][0]=ledsw[3].r; RemoteXY.But3_led_r=ledsw[3].r;}
if(saveLed[3][1]!=ledsw[3].g) {saveLed[3][1]=ledsw[3].g; RemoteXY.But3_led_g=ledsw[3].g;}
if(saveLed[3][2]!=ledsw[3].b) {saveLed[3][2]=ledsw[3].b; RemoteXY.But3_led_b=ledsw[3].b;}
if(saveLed[4][0]!=ledsw[4].r) {saveLed[4][0]=ledsw[4].r; RemoteXY.But4_led_r=ledsw[4].r;}
if(saveLed[4][1]!=ledsw[4].g) {saveLed[4][1]=ledsw[4].g; RemoteXY.But4_led_g=ledsw[4].g;}
if(saveLed[4][2]!=ledsw[4].b) {saveLed[4][2]=ledsw[4].b; RemoteXY.But4_led_b=ledsw[4].b;}
returnMS(100); boolean LedOFF;
LedOFF=1; for_i(0,3) if(ledColorRX[0][i]>50) LedOFF=0; if(LedOFF) {RemoteXY.RGB0_led_r=72; RemoteXY.RGB0_led_g=72; RemoteXY.RGB0_led_b=72;} else {
if(RemoteXY.RGB0_led_r!=ledColorRX[0][0]) RemoteXY.RGB0_led_r=ledColorRX[0][0];
if(RemoteXY.RGB0_led_g!=ledColorRX[0][1]) RemoteXY.RGB0_led_g=ledColorRX[0][1];
if(RemoteXY.RGB0_led_b!=ledColorRX[0][2]) RemoteXY.RGB0_led_b=ledColorRX[0][2];}
LedOFF=1; for_i(0,3) if(ledColorRX[1][i]>50) LedOFF=0; if(LedOFF) {RemoteXY.RGB1_led_r=72; RemoteXY.RGB1_led_g=72; RemoteXY.RGB1_led_b=72;} else {
if(RemoteXY.RGB1_led_r!=ledColorRX[1][0]) RemoteXY.RGB1_led_r=ledColorRX[1][0];
if(RemoteXY.RGB1_led_g!=ledColorRX[1][1]) RemoteXY.RGB1_led_g=ledColorRX[1][1];
if(RemoteXY.RGB1_led_b!=ledColorRX[1][2]) RemoteXY.RGB1_led_b=ledColorRX[1][2];}
LedOFF=1; for_i(0,3) if(ledColorRX[2][i]>50) LedOFF=0; if(LedOFF) {RemoteXY.RGB2_led_r=72; RemoteXY.RGB2_led_g=72; RemoteXY.RGB2_led_b=72;} else {
if(RemoteXY.RGB2_led_r!=ledColorRX[2][0]) RemoteXY.RGB2_led_r=ledColorRX[2][0];
if(RemoteXY.RGB2_led_g!=ledColorRX[2][1]) RemoteXY.RGB2_led_g=ledColorRX[2][1];
if(RemoteXY.RGB2_led_b!=ledColorRX[2][2]) RemoteXY.RGB2_led_b=ledColorRX[2][2];}
LedOFF=1; for_i(0,3) if(ledColorRX[3][i]>50) LedOFF=0; if(LedOFF) {RemoteXY.RGB3_led_r=72; RemoteXY.RGB3_led_g=72; RemoteXY.RGB3_led_b=72;} else {
if(RemoteXY.RGB3_led_r!=ledColorRX[3][0]) RemoteXY.RGB3_led_r=ledColorRX[3][0];
if(RemoteXY.RGB3_led_g!=ledColorRX[3][1]) RemoteXY.RGB3_led_g=ledColorRX[3][1];
if(RemoteXY.RGB3_led_b!=ledColorRX[3][2]) RemoteXY.RGB3_led_b=ledColorRX[3][2];}
LedOFF=1; for_i(0,3) if(ledColorRX[4][i]>50) LedOFF=0; if(LedOFF) {RemoteXY.RGB4_led_r=72; RemoteXY.RGB4_led_g=72; RemoteXY.RGB4_led_b=72;} else {
if(RemoteXY.RGB4_led_r!=ledColorRX[4][0]) RemoteXY.RGB4_led_r=ledColorRX[4][0];
if(RemoteXY.RGB4_led_g!=ledColorRX[4][1]) RemoteXY.RGB4_led_g=ledColorRX[4][1];
if(RemoteXY.RGB4_led_b!=ledColorRX[4][2]) RemoteXY.RGB4_led_b=ledColorRX[4][2];}}

#if not defined AccessPointEnable
void GetIP() {static boolean WiFiOk; static boolean firstStart; 
if (!WiFi.isConnected()) {if(WiFiOk) {WiFiOk = 0; CMn("\nСоединение WiFi потеряно");
if(!firstStart) {firstStart=1; CMn("\nСоединение WiFi...");}}} else {if (!WiFiOk) {IPAddress ip; ip = WiFi.localIP(); WiFiOk=1; WiFi.hostname(ESPName);
CM("\nСоединение успешно:\nСеть WiFi: "); CMn(WiFi.SSID().c_str()); char IPadress[40]; sprintf(IPadress, "IP: %03d.%03d.%03d.%03d", ip[0], ip[1], ip[2], ip[3]);
CMn(IPadress); CMf("Имя хоста: %s\n\n", WiFi.hostname().c_str());
#if not defined AccessPointEnable
sprintf(RemoteXY.IP_text, "IP ADRESS: %03d.%03d.%03d.%03d", ip[0], ip[1], ip[2], ip[3]);
#endif
}} if(!firstStart && !WiFiOk) {returnSec(5); CMn("\nОжидаем cоединения WiFi");}}
#endif

void loopspeed() {static uint32_t loops; loops++; returnSec(1); CM("Loops in sec: "); CMn(loops); loops=0;}

//void chainaLedRun() {//static float hue = 0.0; static boolean up = true;
//for_i(0, NUM_LEDS) chainaleds.setColorRGB(i, 50, 2, 100);
//up ? hue+= 0.025 : hue-= 0.025; if (hue>=1.0 && up) up = 0; else if (hue<=0.0 && !up) up = 1;
//}

//  void reNewLED() {returnMS(5); FastLED.show();}

//void sendDataLoop() {returnMS(500); static uint8_t sdt; sdt++; rBus(sdt); CM("Send "); CMn(sdt);}

void setup() {
#if defined RelayBus
relaySerial.begin(57600);
#endif
#if defined DebagEnable
Serial.begin(115200); delay(10); CMn("\n");
mySerial.begin(57600);
#else
Serial.begin(57600);
#endif

char txtSize[10]; int32_t size = ESP.getFlashChipSize(); if(size < 1024) strcpy(txtSize," байт"); else if(size < 1048576) {size/=1024; strcpy(txtSize," Kбайт");}
else if(size < 1073741824) {size/=1024; size/=1024; strcpy(txtSize," Mбайт");}
CM("Память чипа: "); CM(size); CMn(txtSize); char str[10];
CM("Прошивка занимает: "); float Size=ESP.getSketchSize(); if(Size<1024) {CM(Size); CMn(" байт");} else if(Size<1024*1024) {dtostrf(Size/1024, 0, 0, str); CM(str); CMn(" Kбайт");} else {dtostrf(Size/1024/1024, 0, 1, str); CM(str); CMn(" Мбайт");}
CM("Firmware version: "); CMn(FirmwareVersion);
CM("Место для OTA прошивки: "); Size=ESP.getFreeSketchSpace();
if(Size<1024) {CM(Size); CMn(" байт");} else if(Size<1024*1024) {dtostrf(Size/1024, 0, 0, str); CM(str); CMn(" Kбайт");} else {dtostrf(Size/1024/1024, 0, 1, str); CM(str); CMn(" Мбайт");}
CM("Прошивка по воздуху: "); if(ESP.getFreeSketchSpace()>(ESP.getSketchSize()*1.1)) {OTApossible=1;
CMn("Доступна");} else CMn("Невозможна");
#if defined clearMemory
CMn("Запись EEPROM по умолчинию"); saveChange=1;
#endif
pinMode(2, OUTPUT);
pinMode(0, INPUT_PULLUP); digitalWrite(2,1);
FastLED.addLeds<P9813, DATA_PIN, CLOCK_PIN , RGB>(leds, NUM_LEDS);
for_t(0,NUM_LEDS) {leds[t] = CRGB::Black;} FastLED.show();
FastLED.addLeds<P9813, DATA_PIN1, CLOCK_PIN1 , RGB>(ledsw, NUM_LEDS);
#if not defined AccessPointEnable
if(OTApossible) {
ArduinoOTA.setHostname(ESPName);
ArduinoOTA.onStart([]() {CMn("\nStart OTA");});
ArduinoOTA.onEnd([]() {CMn("\nEnd OTA");});
ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {CMf("Progress: %u%%\r", (progress / (total / 100)));});
ArduinoOTA.onError([](ota_error_t error) {CM("Error: "); switch(error) {
case OTA_AUTH_ERROR: CMn("Auth Failed"); break;
case OTA_BEGIN_ERROR: CMn("Begin Failed"); break;
case OTA_CONNECT_ERROR: CMn("Connect Failed"); break;
case OTA_RECEIVE_ERROR: CMn("Receive Failed"); break;
case OTA_END_ERROR: CMn("End Failed"); break;}});
ArduinoOTA.begin(); CMn("OTA Ready");}
#else
boolean result = WiFi.softAPConfig(local_IP, gateway, subnet);
CM("Soft-AP configuration ... "); CMn(result ? "Success" : "Failed!");
result = WiFi.softAP(ESPName);
CM("Setting soft-AP ... "); CMn(result ? "Ready" : "Failed!");
CM("Soft-AP IP address = "); CMn(WiFi.softAPIP());
#endif
server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {request->send(200, "text/plain", "Hi! Add to http adress /update.");});
AsyncElegantOTA.begin(&server); server.begin(); CMn("ElegantOTA update сервер запущен");
RemoteXY_Init();
dataToEEPROM.ReadData();
//Initialization=1; CMn("Инициализация завершена");
sBus(218); // запрос времени клика
#if defined RelayBus
rBus(50); CMn("Запрос состояния реле");
sensKitchen ? sBus(301) : sBus(300); CMn("Установка чтения датчиков");
sensAlarm ? sBus(303) : sBus(302);
sensBath ? sBus(305) : sBus(304);
sensDoor ? sBus(307) : sBus(306);
#endif
CMn("Setup completed");
}

void loop() { //sendDataLoop(); //voice(); loopspeed();
#if not defined AccessPointEnable
ArduinoOTA.handle();
GetIP();
#endif
sBus();
rBus();
Event_Remote();
FIXLight();
FIXLightWithoutBath();
FIXBath();
FIXRoom();
CountDownLed();
CountDownVent();
CountDownBathSensor();
CountDownBathRGB();
CountDownNewYear();
SetRGBLed();
SaveData();
CountDownСnox();
Alarm();
buttonLed(); // повторяет на смарте RGB цвет подсветки кнопок и RGB модулей
autoOnLed();
autoOffLed();
cnoxLed();
waitLed();
timerLed();
blockLed();
programLed();
autoOnNightLed();
autoOffNightLed();
syncIndikatorLed();
selectInd();
voice(0);
CountDownESPLed(0);
//reNewLED();
}