// Service 01 PIDs (more detail: https://en.wikipedia.org/wiki/OBD-II_PIDs)
#define CAN_ID_PID 0x7DF //OBD-II CAN frame ID
#define PID_ENGINE_RPM  0x0C
#define PID_COOLANT_TEMP 0x05
#define PID_AMBIENT_TEMP 0x46

// Подключаем библиотеки
#include <SPI.h>
#include <mcp_can.h>
#include <SD.h>
#include <TimerOne.h>

// MCP2515
#define CAN0_INT 2                                              // Set INT to pin 2  <--------- CHANGE if using different pin number
MCP_CAN CAN0(10);                                               // Set CS to pin 10 <--------- CHANGE if using different pin number
  
// Считанную температуру будем хранить в массиве, сразу ининиализируем его на 80 градусов 
int CoolantTempArray[9] = {80,80,80,80,80,80,80,80,80};
int CoolantTemp;

// Прототипы функций
void CanInit();                                   // Инициализация CAN интерфейса
unsigned char * receivePID(unsigned char __pid);  // Получение значений посрдеством OBD2 
int ReadTemp();                                   // Чтение температуры ОЖ
void setRelayOut(char pwr);                       // Устанавливаем выходную мощность
void setPower(char pwr);

//Первичная конфигурация платы и периферии 
void setup()
{
  CanInit();
  Timer1.initialize(500);
  Timer1.attachInterrupt(ReadTemp);
  Serial.begin(115200);
  
}

void loop()
{
  unsigned char * getData;
  getData = receivePID(PID_ENGINE_RPM);
  uint16_t rpm = (256*(*(getData + 3)) + *(getData + 4))/4;
  Serial.print("   RPM ");
  Serial.println(rpm);
  getData = receivePID(PID_AMBIENT_TEMP);
  uint8_t atemp = *(getData + 3) - 40;
  Serial.print("   Ambient  ");
  Serial.println(atemp);
  
  if ((rpm >=700)&(atemp<=10)) {

  } //if (rpm >=700)&(atemp<=10)
}

unsigned long set_delay = millis();
char current_set = 0;
void setRelayOut(char pwr){
  //Pump+Heater = ppwr 
  //1001b - 09 - 25%  = 8 + 1
  //1011b - 11 - 50%  = 8 + 3
  //1101b - 13 - 75%  = 8 + 5
  //1111b - 15 - 100% = 8 + 7
  switch (pwr){
    case   0: setPower(0); break;
    case  25: setPower(1); break;
    case  50: setPower(3); break;
    case  75: setPower(5); break;
    case 100: setPower(7); break;
  }
}

//Functions
//Чтение температуры
int ReadTemp(){
  for (char i=0;i<8;++i){
    CoolantTempArray[i] = CoolantTempArray[i+1]; // Смещаем элементы массива на один порядок вправо
  } //for

  unsigned char * getData;
  getData = receivePID(PID_COOLANT_TEMP);     // Получаем новое значение температуры ОЖ 
  CoolantTempArray[8] = *(getData + 3) - 40;  // и заносим в конец массива

  int TempArray[9]; //Создаем временный массив из элементов массива CoolantTempArray
  for(char i=0;i<9;++i){
    TempArray[i] = CoolantTempArray[i];
  }
  qsort(TempArray,10,sizeof(int),cmpfunc);    // Сортируем временный массив, в начале и в конце будут ошибочные данные, если конечно будут
  CoolantTemp = TempArray[4];                 // в середине будет верное значение температуры
  return CoolantTemp;
} //readTemp

// Вспомогательнся функция сравнения для функции qsort
int cmpfunc (const void * a, const void * b) {
   return ( *(int*)a - *(int*)b );
} //cmpfunc

void CanInit(){
  if (CAN0.begin(MCP_STDEXT, CAN_500KBPS, MCP_8MHZ) == CAN_OK)  // Initialize MCP2515 running at 16MHz with a baudrate of 500kb/s and the masks and filters disabled.
    { 
      Serial.println("MCP2515 Initialized Successfully!");
    }
  else 
    {
      Serial.println("Error Initializing MCP2515...");
      while (1);
    }

  //initialise mask and filter to allow only receipt of 0x7xx CAN IDs
  CAN0.init_Mask(0, 0, 0x07000000);             // Init first mask...
  CAN0.init_Mask(1, 0, 0x07000000);             // Init second mask...
  
  for (uint8_t i = 0; i < 6; ++i) {
    CAN0.init_Filt(i, 0, 0x07000000);           // Init filters
  }
  
  CAN0.setMode(MCP_NORMAL);                     // Set operation mode to normal so the MCP2515 sends acks to received data.

  pinMode(CAN0_INT, INPUT);                     // Configuring pin for /INT input

  Serial.println("Sending and Receiving OBD-II_PIDs");
} //CanInit()


unsigned char * receivePID(unsigned char __pid)
{
  static unsigned char rxBuf[8];
  long unsigned int rxId;
  unsigned char len = 0;
  char msgString[128];                                          // Массив для хранения отладочных сообщений
  
  unsigned char tmp[8] = {0x02, 0x01, __pid, 0, 0, 0, 0, 0};    // Формируем кадр для отправки
  byte sndStat = CAN0.sendMsgBuf(CAN_ID_PID, 0, 8, tmp);        // Отправляем кадр
  
  if (sndStat == CAN_OK)                                        // Проверяем статус отправки
    {                                      
      Serial.print("PID sent: 0x");
      Serial.println(__pid, HEX);
    }
  else 
    {
      Serial.println("Error Sending Message...");               // Сообщаем о проблеме отправки
    }

  delay(200);                                                   // Задержка перед получением данных

  if (!digitalRead(CAN0_INT))                                   // If CAN0_INT pin is low, read receive buffer 
    {                                                   
      CAN0.readMsgBuf(&rxId, &len, rxBuf);                      // Read data: len = data length, buf = data byte(s)
      sprintf(msgString, "PID: 0x%.3lX, DLC: %1d, Data: ", rxId, len);      Serial.print(msgString);

      for (byte i = 0; i < len; i++) 
        {
          sprintf(msgString, " 0x%.2X", rxBuf[i]);
          Serial.print(msgString);
        }
      Serial.println("");
   } // endif
   
   return rxBuf;
} //end receivePID

char prev = 0;
 void setPower(char pwr){
   if (prev != pwr)
   if (prev < pwr)
   for (char i=prev;i<=pwr;++i){
      if (i%2 != 0) {
	    PORTB = i + 8;
	    delay(500);
	 } //if i%2
	 }
   else 
   for (char i=prev;i>=pwr;--i)
      if (i%2 != 0 || i == 0) {
	    PORTB = i + 8;
	    delay(500);
	 } //if i%2
	 prev = pwr;
   delay(2000);
   if (pwr == 0) PORTB = 0;
 }