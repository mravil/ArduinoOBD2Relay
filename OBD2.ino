// Service 01 PIDs (more detail: https://en.wikipedia.org/wiki/OBD-II_PIDs)
#define CAN_ID_PID 0x7DF //OBD-II CAN frame ID
#define PID_ENGINE_RPM  0x0C
#define PID_COOLANT_TEMP 0x05

#include <mcp_can.h>
#include <SPI.h>

#define CAN0_INT 2                              // Set INT to pin 2  <--------- CHANGE if using different pin number
MCP_CAN CAN0(10);                               // Set CS to pin 10 <--------- CHANGE if using different pin number

long unsigned int rxId;
unsigned char len = 0;
unsigned char rxBuf[8];
char msgString[128];                        // Array to store serial string

void sendPID(unsigned char __pid)
{
  unsigned char tmp[8] = {0x02, 0x01, __pid, 0, 0, 0, 0, 0};

  byte sndStat = CAN0.sendMsgBuf(CAN_ID_PID, 0, 8, tmp);

//  if (sndStat == CAN_OK) {
//    Serial.print("PID sent: 0x");
//    Serial.println(__pid, HEX);
//  }
//  else {
//    Serial.println("Error Sending Message...");
//  }
}

void receivePID(unsigned char __pid)
{
    if (!digitalRead(CAN0_INT)) {                      // If CAN0_INT pin is low, read receive buffer
    CAN0.readMsgBuf(&rxId, &len, rxBuf);               // Read data: len = data length, buf = data byte(s)

 //   sprintf(msgString, "Standard ID: 0x%.3lX, DLC: %1d, Data: ", rxId, len);
 //   Serial.print(msgString);

//    for (byte i = 0; i < len; i++) {
//      sprintf(msgString, " 0x%.2X", rxBuf[i]);
//      Serial.print(msgString);
//    }
//    Serial.println("");


    switch (__pid) {
      case PID_COOLANT_TEMP:
        if(rxBuf[2] == PID_COOLANT_TEMP){
          uint8_t temp;
          temp = rxBuf[3] - 40;
          Serial.print("Engine Coolant Temp (degC): ");
          Serial.println(temp, DEC);
        }
      break;

      case PID_ENGINE_RPM:
        if(rxBuf[2] == PID_ENGINE_RPM){
          uint16_t rpm;
          rpm = ((256 * rxBuf[3]) + rxBuf[4]) / 4;
          Serial.print("Engine Speed (rpm): ");
          Serial.println(rpm, DEC);
        }
      break;
    }
  }
}

void setup()
{
  Serial.begin(115200);

  // Initialize MCP2515 running at 16MHz with a baudrate of 500kb/s and the masks and filters disabled.
  if (CAN0.begin(MCP_STDEXT, CAN_500KBPS, MCP_8MHZ) == CAN_OK) { //< -------- - CHANGE if using different board
    Serial.println("MCP2515 Initialized Successfully!");
  }
  else {
    Serial.println("Error Initializing MCP2515...");
    while (1);
  }

  //initialise mask and filter to allow only receipt of 0x7xx CAN IDs
  CAN0.init_Mask(0, 0, 0x07000000);              // Init first mask...
  CAN0.init_Mask(1, 0, 0x07000000);              // Init second mask...

  
  for (uint8_t i = 0; i < 6; ++i) {
    CAN0.init_Filt(i, 0, 0x07000000);           //Init filters
  }
  
  CAN0.setMode(MCP_NORMAL);                     // Set operation mode to normal so the MCP2515 sends acks to received data.

  pinMode(CAN0_INT, INPUT);                    // Configuring pin for /INT input

  Serial.println("Sending and Receiving OBD-II_PIDs Example...");
}

void loop()
{
  //request coolant temp
  sendPID(PID_COOLANT_TEMP);

  delay(200); //to allow time for ECU to reply

  receivePID(PID_COOLANT_TEMP);

//  delay();
}
