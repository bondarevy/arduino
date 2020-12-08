/*
   Газовая плита
   v0.1:
*/


////////////////////////// начало настроек которые можно менять //////////////////////
char SketchVersion[16] = "stove v0.1";

////////////////////////// конец настроек которые можно менять //////////////////////
#include <avr/wdt.h>
#include <Bounce2.h>
#include <EEPROM.h>

//доп переменные
boolean EmergencyExitCode = false;
boolean FireRunning[] = { false, false, false, false, false };

//цифровые выходы (прерывания только на 2 и 3 пине)
const int ButtonFirePWMUP = 2; //прибавить огня (увеличить шим/pwm на выбраном соленойде)
const int ButtonFirePWMDOWN = 3;  //убавить огня (уменьшить шим на выбранном соленойде)

const int BurnerPWMSolenoid[] = {11, 10, 9, 6, 5 };
int PWMAmmount[] =  { 127, 127, 127, 127, 127 }; //мощность по умолчаниию 127 = 50%


const int BurnerMAINSolenoid = 7; //главный соленойд
const int BurnerIgnitionCoil = 8; //катушка искры

//аналоговые входы
const int BurnerButton[] = { A0, A1, A2, A3, A4 }; //Кнопка включения горелки 1

//номера ячеек памяти
const int SolenoidPWMPOWEREepromAddress[] = {1, 2, 3, 4, 5};


int DrebezgTime = 150; //время дребезга кнопок
int PushWaitTime = 3000; //2 секунды ждать при нажатой кнопке
//текстовые переменные
char* SystemMessage; //текстовая запись при аварийном выключении

//инициируем объект дебаунсера

Bounce DebouncedBurnerButton[] = { Bounce(), Bounce(), Bounce(), Bounce(), Bounce() };


Bounce DebouncedButtonFirePWMUP = Bounce();
Bounce DebouncedButtonFirePWMDOWN = Bounce();

void Reset() {
  //чистим еепром для ресета в заводские настройки
  for (int i = 0 ; i < EEPROM.length() ; i++) {
    EEPROM.write(i, 0);
  }
}

void BurnerInit()
{

  //При включении инициализируется дисплей и датчики температуры
  //и выводятся на дисплей текущая температура масла и воды.

  //SERIAL PORT INIT
  Serial.begin(9600);

  //назначение вход-выход
  for (int i = 0; i <= 4; i++) {
    //output interfaces (switches/solenoids)
    pinMode(BurnerPWMSolenoid[i], OUTPUT);
    digitalWrite(BurnerPWMSolenoid[i], LOW); //и сразу отключаем 
    //input interfaces (sensors):
    pinMode(BurnerButton[i], INPUT_PULLUP);
  }

  pinMode(ButtonFirePWMUP, INPUT_PULLUP);
  pinMode(ButtonFirePWMDOWN, INPUT_PULLUP);
  
  pinMode(BurnerMAINSolenoid, OUTPUT);
  pinMode(BurnerIgnitionCoil, OUTPUT);
  
  //и сразу ОТКЛЮЧАЕМ ОСНОВНОЙ КЛАПАН И ИСКРУ при старте по умолчанию
  digitalWrite(BurnerMAINSolenoid, LOW);
  digitalWrite(BurnerIgnitionCoil, LOW);
      
  for (int i = 0; i <= 4; i++) {
    DebouncedBurnerButton[i].attach(BurnerButton[i]);
    DebouncedBurnerButton[i].interval(DrebezgTime);
  }

  //цепляемся раздребезгом к кнопке +
  DebouncedButtonFirePWMUP.attach(ButtonFirePWMUP);
  DebouncedButtonFirePWMUP.interval(DrebezgTime);

  //цепляемся раздребезгом к кнопке -
  DebouncedButtonFirePWMDOWN.attach(ButtonFirePWMDOWN);
  DebouncedButtonFirePWMDOWN.interval(DrebezgTime);
}

void PrintMessage(char SystemMessage[16]) {
  Serial.println(SystemMessage);
}

void FireOn(int ID) {
  wdt_reset(); //сбрасываем вочдог
  if (EmergencyExitCode != 1 ) {
    //проверяем нажата ли кнопка энкодера
    DebouncedBurnerButton[ID].update();
    if ( DebouncedBurnerButton[ID].read() == LOW && FireRunning[ID] == false ) {
      Serial.print("Stove #");
      Serial.print(ID);
      Serial.println(" startup ... ");
      wdt_reset(); //сбрасываем вочдог
      unsigned long PushTime = millis();
      while ( millis() < PushTime + PushWaitTime && EmergencyExitCode != 1 ) {
        wdt_reset(); //сбрасываем вочдог
        DebouncedBurnerButton[ID].update();
        if (DebouncedBurnerButton[ID].read() == HIGH) {

          Serial.print("CANCELLED ");
          Serial.print(ID);
          Serial.println(".");

          goto press_cancelled;
        }
      }
      //Запустить огонь на горелке
      //PrintMessage("Fire ON "+ID);
      Serial.print("Fire RUNNING #");
      Serial.print(ID);
      Serial.println(".");

      ///тут включаем клапан основной
      digitalWrite(BurnerMAINSolenoid, HIGH);
      delay(300); //подождали
      ///тут включаем искру
      digitalWrite(BurnerIgnitionCoil, HIGH);
      //тут включаем горелку
      analogWrite(BurnerPWMSolenoid[ID], PWMAmmount[ID]); //50% (0-255)
      
      //тут выключаем искру
      delay(3000); //ждем 3000 миллисекунд = 3сек
      digitalWrite(BurnerIgnitionCoil, LOW); 

           
      FireRunning[ID] = 1;
      //      FireRunning[ID] = 1;
    }   else if ( DebouncedBurnerButton[ID].read() == LOW && FireRunning[ID] == true ) {
     
    DebouncedButtonFirePWMUP.update();
    DebouncedButtonFirePWMDOWN.update();      
      if (DebouncedButtonFirePWMUP.fell()) {
        Serial.print("+");

        //тут увеличиваем газ
        PWMAmmount[ID]=(PWMAmmount[ID]+25);
        if (PWMAmmount[ID] >= 255) {
          PWMAmmount[ID]=255;
          digitalWrite(BurnerPWMSolenoid[ID], HIGH);
        } else {
          analogWrite(BurnerPWMSolenoid[ID], PWMAmmount[ID]);
        }
        
        Serial.print(PWMAmmount[ID]);
        //тут завершаем увеличение газа
        
      }
      if (DebouncedButtonFirePWMDOWN.fell()) {
        Serial.print("-");

        //тут уменьшаем подачу газа
        PWMAmmount[ID]=(PWMAmmount[ID]-25);
        if (PWMAmmount[ID] <= 0) {
          PWMAmmount[ID]=0;
          digitalWrite(BurnerPWMSolenoid[ID], LOW);
          FireRunning[ID] = false; //выходим из цикла так как ставим переменную что не горим
          PWMAmmount[ID]=127; // ставим в исходное положение 50% 
        } else {
          analogWrite(BurnerPWMSolenoid[ID], PWMAmmount[ID]);
        }
        Serial.print(PWMAmmount[ID]);
        //тут завершаем уменьшать подачу газа
        
      }
    }
press_cancelled:;

  }
}



void setup() {
  Serial.begin(9600);
  wdt_enable(WDTO_8S); //устанавливаем вочдог на 8 секунд
  BurnerInit();
  PrintMessage("Stove 0.1 OK");
  while (EmergencyExitCode != 1) {
    for (int t = 0; t <= 4; t++) {
      FireOn(t);
    }
  }
}

void loop() {

}


void SoftReset() {
  wdt_reset();
  asm volatile ("  jmp 0");
}
