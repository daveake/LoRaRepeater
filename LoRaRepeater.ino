// Intended for use with an Adafruit Feather with LoRa and serial USB comms to an Android phone

#include <string.h>
#include <ctype.h>
#include <SPI.h>
#include <EEPROM.h>

/*-----------------------------------------*\
|                                           |
|               D4  - Reset                 |
|               D7  - DIO0                  |
|               D8  - NSS                   |
|               D14 - RFM MISO              |
|               D15 - RFM CLK               |
|               D16 - RFM MOSI              |
|                                           |
\*-----------------------------------------*/

// RFM98
int RST = 4;
int NSS = 8;
int DIO0 = 7;
byte currentMode = 0x81;
unsigned long UpdateClientAt=0;
int Sending=0;

struct TRadio
{
  double Frequency;

  int ImplicitOrExplicit;
  int ErrorCoding;
  int Bandwidth;
  int SpreadingFactor;
  int LowDataRateOptimize;
};

struct TSettings
{
  // Common
  double PPM;
  
  // Rx Settings
  struct TRadio Rx;

  // TxSettings
  struct TRadio Tx;

  char EnableRepeater;
} Settings;

#define REG_FIFO                    0x00
#define REG_FIFO_ADDR_PTR           0x0D 
#define REG_FIFO_TX_BASE_AD         0x0E
#define REG_FIFO_RX_BASE_AD         0x0F
#define REG_RX_NB_BYTES             0x13
#define REG_OPMODE                  0x01
#define REG_FIFO_RX_CURRENT_ADDR    0x10
#define REG_IRQ_FLAGS               0x12
#define REG_PACKET_SNR              0x19
#define REG_PACKET_RSSI             0x1A
#define REG_RSSI_CURRENT            0x1B
#define REG_DIO_MAPPING_1           0x40
#define REG_DIO_MAPPING_2           0x41
#define REG_MODEM_CONFIG            0x1D
#define REG_MODEM_CONFIG2           0x1E
#define REG_MODEM_CONFIG3           0x26
#define REG_PAYLOAD_LENGTH          0x22
#define REG_IRQ_FLAGS_MASK          0x11
#define REG_HOP_PERIOD              0x24
#define REG_FREQ_ERROR              0x28
#define REG_DETECT_OPT              0x31
#define REG_DETECTION_THRESHOLD     0x37

// MODES
// MODES
#define RF96_MODE_RX_CONTINUOUS     0x85
#define RF96_MODE_TX                0x83
#define RF96_MODE_SLEEP             0x80
#define RF96_MODE_STANDBY           0x81

#define PAYLOAD_LENGTH              80


// Modem Config 2

#define SPREADING_6                 0x60
#define SPREADING_7                 0x70
#define SPREADING_8                 0x80
#define SPREADING_9                 0x90
#define SPREADING_10                0xA0
#define SPREADING_11                0xB0
#define SPREADING_12                0xC0

#define CRC_OFF                     0x00
#define CRC_ON                      0x04


// POWER AMPLIFIER CONFIG
#define REG_PA_CONFIG               0x09
#define PA_MAX_BOOST                0x8F
#define PA_LOW_BOOST                0x81
#define PA_MED_BOOST                0x8A
#define PA_MAX_UK                   0x88
#define PA_OFF_BOOST                0x00
#define RFO_MIN                     0x00

// LOW NOISE AMPLIFIER
#define REG_LNA                     0x0C
#define LNA_MAX_GAIN                0x23  // 0010 0011
#define LNA_OFF_GAIN                0x00


char Hex[] = "0123456789ABCDEF";

char *Bandwidths[] = {"7K8", "10K4", "15K6", "20K8", "31K25", "41K7", "62K5", "125K", "250K", "500K"};  // Registr value index here << 4

// initialize the library with the numbers of the interface pins

void setup()
{
  Serial.begin(57600);

  Serial.println("");
  Serial.println("LoRa USB Repeater");
  Serial.println("Version=1.0.1");

  if ((EEPROM.read(0) == 'D') && (EEPROM.read(1) == 'A'))
  {
    // Load settings from EEPROM
    LoadSettings();
    Serial.println("Settings loaded from EEPROM");
  }
  else
  {
    LoadDefaults();
    Serial.println("Loaded default settings");
    StoreSettings();
  }

  ShowSettings();
  
  // SetParametersFromLoRaMode(Settings.RxMode);

  SetupRFM98();
}

void ShowSettings(void)
{
  // Common
  Serial.print("PPM="); Serial.println(Settings.PPM);

  // Rx
  Serial.print("RxFrequency="); Serial.println(Settings.Rx.Frequency, 4);
  Serial.print("RxImplicit="); Serial.println(Settings.Rx.ImplicitOrExplicit);
  Serial.print("RxErrorCoding="); Serial.println(Settings.Rx.ErrorCoding);
  if ((Settings.Rx.Bandwidth >= 0) && (Settings.Rx.Bandwidth <= 9)) Serial.print("RxBandwidth="); Serial.println(Bandwidths[Settings.Rx.Bandwidth]);
  Serial.print("RxSpreadingFactor="); Serial.println(Settings.Rx.SpreadingFactor);
  Serial.print("RxLowDataRateOptimize="); Serial.println(Settings.Rx.LowDataRateOptimize);

  // Tx
  Serial.print("TxFrequency="); Serial.println(Settings.Tx.Frequency, 4);
  Serial.print("TxImplicit="); Serial.println(Settings.Tx.ImplicitOrExplicit);
  Serial.print("TxErrorCoding="); Serial.println(Settings.Tx.ErrorCoding);
  if ((Settings.Tx.Bandwidth >= 0) && (Settings.Tx.Bandwidth <= 9)) Serial.print("TxBandwidth="); Serial.println(Bandwidths[Settings.Tx.Bandwidth]);
  Serial.print("TxSpreadingFactor="); Serial.println(Settings.Tx.SpreadingFactor);
  Serial.print("TxLowDataRateOptimize="); Serial.println(Settings.Tx.LowDataRateOptimize);

  // Repeating
  Serial.print("EnableRepeater="); Serial.println(Settings.EnableRepeater ? 1 : 0);
}

void LoadSettings(void)
{
  int i;
  unsigned char *ptr;

  ptr = (unsigned char *)(&Settings);
  for (i=0; i<sizeof(Settings); i++, ptr++)
  {
    *ptr = EEPROM.read(i+2);
  }
}

void StoreSettings(void)
{
  int i;
  unsigned char *ptr;
  
  // Signature
  EEPROM.write(0, 'D');
  EEPROM.write(1, 'A');

  // Settings
  ptr = (unsigned char *)(&Settings);
  for (i=0; i<sizeof(Settings); i++, ptr++)
  {
    EEPROM.write(i+2, *ptr);
  }
}


void LoadDefaults()
{
  Settings.PPM = 0.0;
  
  Settings.Rx.Frequency = 434.450;
  Settings.Rx.ImplicitOrExplicit = 1;
  Settings.Rx.ErrorCoding = 5;
  Settings.Rx.Bandwidth = 3;

  Settings.Rx.SpreadingFactor = 6;

  Settings.Tx.Frequency = 434.450;
  Settings.Tx.ImplicitOrExplicit = 1;
  Settings.Tx.ErrorCoding = 5;
  Settings.Tx.Bandwidth = 3;
  Settings.Tx.SpreadingFactor = 6;

  Settings.EnableRepeater = 0;
}

void loop()
{
  CheckPC();
  
  CheckLoRa();
  
  UpdateClient();
}

void UpdateClient(void)
{
  if (millis() >= UpdateClientAt)
  {
    int CurrentRSSI;

    if (Settings.Rx.Frequency > 525)
    {
      CurrentRSSI = readRegister(REG_RSSI_CURRENT) - 157;
    }
    else
    {
      CurrentRSSI = readRegister(REG_RSSI_CURRENT) - 164;
    }
    
    Serial.print("CurrentRSSI=");
    Serial.println(CurrentRSSI);
    
    UpdateClientAt = millis() + 1000;
  }
}

double FrequencyReference(void)
{
  const double Bandwidths[] = {7.8, 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125, 250, 500};

  return (Bandwidths[Settings.Rx.Bandwidth]);
}


double FrequencyError(void)
{
  int32_t Temp;
  double T;
  
  Temp = (int32_t)readRegister(REG_FREQ_ERROR) & 7;
  Temp <<= 8L;
  Temp += (int32_t)readRegister(REG_FREQ_ERROR+1);
  Temp <<= 8L;
  Temp += (int32_t)readRegister(REG_FREQ_ERROR+2);
  
  if (readRegister(REG_FREQ_ERROR) & 8)
  {
    Temp = Temp - 524288;
  }

  T = (double)Temp;
  T *=  (16777216.0 / 32000000.0);
  T *= (FrequencyReference() / 500000.0);

  return -T;
} 

int ReceiveMessage(unsigned char *message)
{
  int i, Bytes, currentAddr;

  int x = readRegister(REG_IRQ_FLAGS);
  // printf("Message status = %02Xh\n", x);
  
  // clear the rxDone flag
  // writeRegister(REG_IRQ_FLAGS, 0x40); 
  writeRegister(REG_IRQ_FLAGS, 0xFF); 
   
  // check for payload crc issues (0x20 is the bit we are looking for
  if((x & 0x20) == 0x20)
  {
    // reset the crc flags
    writeRegister(REG_IRQ_FLAGS, 0x20); 
    Bytes = 0;
    Serial.println("CRC error");
  }
  else
  {
    currentAddr = readRegister(REG_FIFO_RX_CURRENT_ADDR);
    Bytes = readRegister(REG_RX_NB_BYTES);
	
    writeRegister(REG_FIFO_ADDR_PTR, currentAddr);   
    for(i = 0; i < Bytes; i++)
    {
      message[i] = (unsigned char)readRegister(REG_FIFO);
    }
    message[Bytes] = '\0';
  } 
  
  return Bytes;
}

void ReplyOK(void)
{
  Serial.println('*');
}

void ReplyBad(void)
{
  Serial.println('?');
}

void SetFrequency(int Tx, char *Line)
{
  double Freq;

  Freq = atof(Line);

  if (Freq > 0)
  {
    ReplyOK();

    if (Tx)
    {
      Settings.Tx.Frequency = Freq;
    }
    else
    {
      Settings.Rx.Frequency = Freq;
      StartReceiving();
    }
    
    // Serial.print("Frequency=");
    // Serial.println(Frequency);

  }
  else
  {
    ReplyBad();
  }
}

void SetBandwidth(int Tx, char *Line)
{
  int Bandwidth, i;

  Bandwidth = -1;
  
  for (i=0; i<=9; i++)
  {
    if (strcmp(Line, Bandwidths[i]) == 0)
    {
        Bandwidth = i;
    }
  }

  if (Bandwidth >= 0)
  {
    if (Tx)
    {
      Settings.Tx.Bandwidth = Bandwidth;
    }
    else
    {
      Settings.Rx.Bandwidth = Bandwidth;
      StartReceiving();
    }
    ReplyOK();
  }
  else
  {
    ReplyBad();
  }
}

void SetErrorCoding(int Tx, char *Line)
{
  int Coding;

  Coding = atoi(Line);

  if ((Coding >= 5) && (Coding <= 8))
  {
    if (Tx)
    {
      Settings.Tx.ErrorCoding = Coding;
    }
    else
    {
      Settings.Rx.ErrorCoding = Coding;
      StartReceiving();
    }
    ReplyOK();
  }
  else
  {
    ReplyBad();
  }
}

void SetSpreadingFactor(int Tx, char *Line)
{
  int Spread;

  Spread = atoi(Line);

  if ((Spread >= 6) && (Spread <= 12))
  {
    if (Tx)
    {
      Settings.Tx.SpreadingFactor = Spread;
    }
    else
    {
      Settings.Rx.SpreadingFactor = Spread;
      StartReceiving();
    }
    ReplyOK();
  }
  else
  {
    ReplyBad();
  }
}

void SetImplicit(int Tx, char *Line)
{
  int Implicit;

  Implicit = atoi(Line);

  if (Tx)
  {
    Settings.Tx.ImplicitOrExplicit = Implicit;
  }
  else
  {
    Settings.Rx.ImplicitOrExplicit = Implicit;
    StartReceiving();
  }
  ReplyOK();
}

void SetLowOpt(int Tx, char *Line)
{
  int LowOpt;

  LowOpt = atoi(Line);

  if (Tx)
  {
    Settings.Tx.LowDataRateOptimize = LowOpt;
  }
  else
  {
    Settings.Rx.LowDataRateOptimize = LowOpt;
    StartReceiving();
  }
  
  ReplyOK();
}

void SetPPM(char *Line)
{
  Settings.PPM = atof(Line);

  ReplyOK();
  
  StartReceiving();
}

void SetRepeating(char *Line)
{
  Settings.EnableRepeater  = atoi(Line);

  ReplyOK();
  
  StartReceiving();
}

void ProcessCommand(char *Line)
{
  char Command;
  int Tx;

  Command = toupper(Line[1]);
  Tx = islower(Line[1]);
  
  Line += 2;
       
  if (Command == 'F')
  {
    SetFrequency(Tx, Line);
  }
  else if (Command == 'B')
  {
    SetBandwidth(Tx, Line);
  }
  else if (Command == 'E')
  {
    SetErrorCoding(Tx, Line);
  }
  else if (Command == 'S')
  {
    SetSpreadingFactor(Tx, Line);
  }
  else if (Command == 'I')
  {
    SetImplicit(Tx, Line);
  }
  else if (Command == 'L')
  {
    SetLowOpt(Tx, Line);
  }
  else if (Command == 'P')
  {
    SetPPM(Line);
  }
  else if (Command == 'R')
  {
    SetRepeating(Line);
  }
  else if (Command == '!')
  {
    StoreSettings();
    Serial.println("Saved settings");
  }
  else if (Command == '?')
  {
    ShowSettings();
  }
  else if (Command == '^')
  {
    LoadSettings();
    ShowSettings();
  }
  else if (Command == '*')
  {
    LoadDefaults();
    Serial.println("Loaded default settings");
    StoreSettings();
    ShowSettings();
  }
  else
  {
    ReplyBad();
  }
}

void CheckPC()
{
  static char Line[32];
  static int Length=0;
  char Character;

  while (Serial.available())
  { 
    Character = Serial.read();
    
    if (Character == '~')
    {
      Line[0] = Character;
      Length = 1;
    }
    else if (Length >= sizeof(Line))
    {
      Length = 0;
    }
    else if (Length > 0)
    {
      if (Character == '\r')
      {
        Line[Length] = '\0';
        ProcessCommand(Line);
        Length = 0;
      }
      else
      {
        Line[Length++] = Character;
      }
    }
  }
}

void SendPacket(unsigned char *buffer, int Length)
{
  int i;
  
  Serial.print("Sending "); Serial.print(Length);Serial.println(" bytes");
  
  SetLoRaFrequency(Settings.Tx.Frequency);
  
  SetLoRaParameters(Settings.Tx);

  writeRegister(REG_DIO_MAPPING_1,0x40);
  writeRegister(REG_DIO_MAPPING_2,0x00);

  writeRegister(REG_FIFO_TX_BASE_AD, 0x00);  // Update the address ptr to the current tx base address
  writeRegister(REG_FIFO_ADDR_PTR, 0x00); 
  
  if (Settings.Tx.ImplicitOrExplicit == 0)
  {
    writeRegister(REG_PAYLOAD_LENGTH, Length);
  }
  
  select();
  SPI.transfer(REG_FIFO | 0x80);

  for (i = 0; i < Length; i++)
  {
    SPI.transfer(buffer[i]);
  }
  unselect();

  // go into transmit mode
  setMode(RF96_MODE_TX);

  Sending = 1;
}

void CheckLoRa()
{
  if (digitalRead(DIO0))
  {
    if (Sending)
    {
      Serial.println("DONE SENDING");
      writeRegister( REG_IRQ_FLAGS, 0x08); 
      StartReceiving();
    }
    else
    {
      unsigned char Message[256];
      int Bytes, SNR, RSSI, i;
      long Altitude;
      
      Bytes = ReceiveMessage(Message);
  
      if (Bytes > 0)
      {
        Serial.print("FreqErr="); Serial.println(FrequencyError()/1000.0);
    
        SNR = readRegister(REG_PACKET_SNR);
        SNR /= 4;
        
        if (Settings.Rx.Frequency > 525)
        {
          RSSI = readRegister(REG_PACKET_RSSI) - 157;
        }
        else
        {
          RSSI = readRegister(REG_PACKET_RSSI) - 164;
        }
        
        if (SNR < 0)
        {
          RSSI += SNR;
        }
        
        Serial.print("PacketRSSI="); Serial.println(RSSI);
        Serial.print("PacketSNR="); Serial.println(SNR);
        
        if (Message[0] == '$')
        {
          Serial.print("Message=");
          Serial.println((char *)Message);
        }
        else if (Message[0] == '%')
        {
          char *ptr, *ptr2;
    
          Message[0] = '$';
          
          ptr = (char *)Message;
          do
          {
            if ((ptr2 = strchr(ptr, '\n')) != NULL)
            {
              *ptr2 = '\0';
              Serial.print("Message=");
              Serial.println(ptr);
              ptr = ptr2 + 1;
            }
          } while (ptr2 != NULL);
        }
        else
        {
          Serial.print("Hex=");
          for (i=0; i<Bytes; i++)
          {
            if (Message[i] < 0x10)
            {
              Serial.print("0");
            } 
            Serial.print(Message[i], HEX);
          }
          Serial.println();
        }
        
        if (Settings.EnableRepeater && (Message[0] == '$'))
        {
          // Repeat packet
          SendPacket(Message, Bytes);
        }
      }
    }
  }
}


/////////////////////////////////////
//    Method:   Change the mode
//////////////////////////////////////
void setMode(byte newMode)
{
  if(newMode == currentMode)
    return;  
  
  switch (newMode) 
  {
    case RF96_MODE_TX:
      writeRegister(REG_LNA, LNA_OFF_GAIN);  // TURN LNA OFF FOR TRANSMITT
      writeRegister(REG_PA_CONFIG, PA_MAX_UK);
      writeRegister(REG_OPMODE, newMode);
      currentMode = newMode; 
      
      break;
    case RF96_MODE_RX_CONTINUOUS:
      writeRegister(REG_PA_CONFIG, PA_OFF_BOOST);  // TURN PA OFF FOR RECIEVE??
      writeRegister(REG_LNA, LNA_MAX_GAIN);  // LNA_MAX_GAIN);  // MAX GAIN FOR RECIEVE
      writeRegister(REG_OPMODE, newMode);
      currentMode = newMode; 
      break;
      
      break;
    case RF96_MODE_SLEEP:
      writeRegister(REG_OPMODE, newMode);
      currentMode = newMode; 
      break;
    case RF96_MODE_STANDBY:
      writeRegister(REG_OPMODE, newMode);
      currentMode = newMode; 
      break;
    default: return;
  } 
  
  if(newMode != RF96_MODE_SLEEP)
  {
    delay(10);
  }
   
  return;
}


/////////////////////////////////////
//    Method:   Read Register
//////////////////////////////////////

byte readRegister(byte addr)
{
  select();
  SPI.transfer(addr & 0x7F);
  byte regval = SPI.transfer(0);
  unselect();
  return regval;
}

/////////////////////////////////////
//    Method:   Write Register
//////////////////////////////////////

void writeRegister(byte addr, byte value)
{
  select();
  SPI.transfer(addr | 0x80); // OR address with 10000000 to indicate write enable;
  SPI.transfer(value);
  unselect();
}

/////////////////////////////////////
//    Method:   Select Transceiver
//////////////////////////////////////
void select() 
{
  digitalWrite(NSS, LOW);
}

/////////////////////////////////////
//    Method:   UNSelect Transceiver
//////////////////////////////////////
void unselect() 
{
  digitalWrite(NSS, HIGH);
}

void SetLoRaFrequency(double Frequency)
{
  unsigned long FrequencyValue;
  double Temp;

  Frequency = Frequency * (1.0 + Settings.PPM / 1000000);
  Temp = Frequency * 7110656 / 434;
  FrequencyValue = (unsigned long)(Temp);

  writeRegister(0x06, (FrequencyValue >> 16) & 0xFF);    // Set frequency
  writeRegister(0x07, (FrequencyValue >> 8) & 0xFF);
  writeRegister(0x08, FrequencyValue & 0xFF);
}

void SetLoRaParameters(struct TRadio Radio)
{ 
  writeRegister(REG_MODEM_CONFIG, (Radio.ImplicitOrExplicit ? 1 : 0) | ((Radio.ErrorCoding - 4) << 1) | (Radio.Bandwidth << 4));
  writeRegister(REG_MODEM_CONFIG2, (Radio.SpreadingFactor << 4) | CRC_ON);
  writeRegister(REG_MODEM_CONFIG3, 0x04 | (Radio.LowDataRateOptimize ? 0x08 : 0));                  // 0x04: AGC sets LNA gain
  writeRegister(REG_DETECT_OPT, (readRegister(REG_DETECT_OPT) & 0xF8) | ((Radio.SpreadingFactor == 6) ? 0x05 : 0x03));  // 0x05 For SF6; 0x03 otherwise
  writeRegister(REG_DETECTION_THRESHOLD, (Radio.SpreadingFactor == 6) ? 0x0C : 0x0A);    // 0x0C for SF6, 0x0A otherwise
}

void StartReceiving()
{
  Sending = 0;
  
  setMode(RF96_MODE_SLEEP);
  writeRegister(REG_OPMODE,0x80);  
  setMode(RF96_MODE_SLEEP);

  SetLoRaFrequency(Settings.Rx.Frequency);
  
  SetLoRaParameters(Settings.Rx);
  
  writeRegister(REG_DIO_MAPPING_1, 0x00);    // 00 00 00 00 maps DIO0 to RxDone

  writeRegister(REG_PAYLOAD_LENGTH, 255);
  writeRegister(REG_RX_NB_BYTES, 255);
  
  writeRegister(REG_FIFO_RX_BASE_AD, 0);
  writeRegister(REG_FIFO_ADDR_PTR, 0);
  
  // Setup Receive Continous Mode
  setMode(RF96_MODE_RX_CONTINUOUS);
}

void SetupRFM98(void)
{
  // initialize the pins
  pinMode(RST, OUTPUT);
  digitalWrite(RST, HIGH);
  delay(10);          // Module needs this before it's ready  
  
  pinMode(NSS, OUTPUT);
  pinMode(DIO0, INPUT);

  SPI.begin();
  
  StartReceiving();
}


