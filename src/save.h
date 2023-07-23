#ifndef XIIIMSave_h
#define XIIIMSave_h

#pragma once
#include <Arduino.h>
#include <EEPROM.h>

class XIIIMSave {
public:

void ReadData();
void SaveData();

uint16_t startAdress;         // стартовый адрес хранения данных в EEPROM
boolean readFirst=0;
private:

};

void XIIIMSave::SaveData() {
EEPROM.begin (4096);
delay(10);
uint16_t addr = 4095;
#if defined ledIndikator_Enable
Led.SaveInd=1;
#endif
EEPROM.put (addr, 'x');
addr = 0;
EEPROM.get (addr, startAdress);

if(save.saveCount >= 1000 || startAdress + sizeof(save) > 4095) {startAdress += sizeof(save); save.saveCount = 0; // меняем адрес каждые 1000 сохранений
if ((startAdress + sizeof(save)) > 4095) {startAdress = sizeof(startAdress); CM("startAdress = "); CMn(startAdress);} EEPROM.put (0, startAdress);} save.saveCount++;

addr = startAdress;
EEPROM.put (addr, save);
addr = addr+(sizeof(save));
EEPROM.put (addr,save.saveCount);
EEPROM.end();
CM("Записали "); CM(sizeof(save)); CM(" байт "); CM("по адресу: "); CM(startAdress); CM(" | "); 
CM(save.saveCount); CMn(" раз(а).");}

void XIIIMSave::ReadData() {uint8_t initread=0; int16_t addr=4095;
EEPROM.begin(4096);
delay(10);
EEPROM.get (addr, initread); if (char(initread) != 'x') {CMn("Первый запуск. Инициализация EEPROM."); 
EEPROM.end(); delay(10); 
saveChange=1;} else {
addr = 0;
EEPROM.get (addr, startAdress);
addr = startAdress;
EEPROM.get (addr, save);
EEPROM.end();
}
CM("Читаем данные по адресу: "); CM(startAdress); CM(", "); CM(sizeof(save)); CMn(" байт");
CM("Авто режим: "); CMn(save.autoMode ? "Включен" : "Отключен");
CM("Время StandBy режима: "); CM(save.standByTime); CMn(" сек");
if(save.pressSpeed < 0.8) save.pressSpeed=0.8; if(save.pressSpeed > 4) save.pressSpeed=4; char value[7]; dtostrf(save.pressSpeed,1,2,value);
CM("Задержка кнопки: "); CM(value); CM(" : "); dtostrf(500*save.pressSpeed,1,0,value); CM(value); CMn("мсек");
String RGBLight[5] = {"RGB подсветка зеркала на входе: ","RGB подсветка окон: ","RGB подсветка кухни: ","RGB подсветка бара: ","RGB подсветка ванной: "};
for_t(0,5) {CM(RGBLight[t]); CM(save.RGB_r[t]); CM(" "); CM(save.RGB_g[t]); CM(" "); CMn(save.RGB_b[t]);}
CM("Состояние RGB при старте: "); if (save.StartMode == 0) CMn("Восстановить");
else if (save.StartMode == 1) CMn("Выключить");
else if (save.StartMode == 2) CMn("Включить");
CM("Задержка перетекания цвета: "); CM(save.FadeDelay); CMn(" мс");
CM("Гамма коррекция: "); CMn(save.GammaCorrection ? "Да" : "Нет");
for_i(0,3) {if(save.Timer[i]<0) save.Timer[i]=0; if(save.Timer[i]>3660) save.Timer[i]=3660;}
CM("Овтоотключение розеток на окнах: "); CMn(save.PowerWindowOFF ? "Да" : "Нет");
CM("Время авто выкл света на входе: "); CM(save.Timer[0]); CMn(" сек");
CM("Время авто выкл RGB в ванной: "); CM(save.Timer[1]); CMn(" сек");
CM("Время авто выкл вытяжки в ванне: "); CM(save.Timer[2]); CMn(" сек");
CM("Время авто выкл розеток на окнах: "); CM(save.Timer[3]); CMn(" сек");
CM("Звуковое сопровождение: "); CMn(save.Zumer ? "Да" : "Нет");
CM("Голосовой помощник: "); CMn(save.Sound ? "Да" : "Нет");
for_i(5,10) {save.autoMode ? modeInd[i]=autoIndOn : modeInd[i]=autoIndOff;}
if((save.StartMode==0 && save.RGB_ModulePower[0]) || save.StartMode==2) {RGB_PowerOn(0); RemoteXY.Mirror_RGB=1;}
if((save.StartMode==0 && save.RGB_ModulePower[1]) || save.StartMode==2) {RGB_PowerOn(1); RemoteXY.Window_RGB=1;}
if((save.StartMode==0 && save.RGB_ModulePower[2]) || save.StartMode==2) {RGB_PowerOn(2); RemoteXY.Kitchen_RGB=1;}
if((save.StartMode==0 && save.RGB_ModulePower[3]) || save.StartMode==2) {RGB_PowerOn(3); RemoteXY.Bar_RGB=1;}
if((save.StartMode==0 && save.RGB_ModulePower[4]) || save.StartMode==2) {RGB_PowerOn(4); RemoteXY.Bath_RGB=1;}
if(save.StartMode==1) {RemoteXY.Bath_RGB=0; RemoteXY.Mirror_RGB=0; RemoteXY.Window_RGB=0; RemoteXY.Kitchen_RGB=0; RemoteXY.Bar_RGB=0;} readFirst=1; CMn("Данные восстановлены");}

#endif // XIIIMSave_h