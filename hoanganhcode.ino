#include <OneWire.h>
#include <DallasTemperature.h>

#define N 4
#define TEMP_LIMIT 30.0


const byte mosPin[N] =
{
  3, 5, 6, 9
};


const byte dsPin[N] =
{
  A0, A1, A2, A3
};

OneWire ow0(A0);
OneWire ow1(A1);
OneWire ow2(A2);
OneWire ow3(A3);

DallasTemperature ds0(&ow0);
DallasTemperature ds1(&ow1);
DallasTemperature ds2(&ow2);
DallasTemperature ds3(&ow3);

DallasTemperature* sensor[N] =
{
  &ds0,
  &ds1,
  &ds2,
  &ds3
};


float temp[N];

int pwmValue[N] =
{
  0, 0, 0, 0
};

String rxBuffer = "";

unsigned long timerSend = 0;


void ReadAllTemp();
void ControlAllMosfet();
void ReceiveUART();
void SendTemperature();



void setup()
{
  Serial.begin(115200);

  for (byte i = 0; i < N; i++)
  {
    sensor[i]->begin();

    pinMode(mosPin[i], OUTPUT);

    analogWrite(mosPin[i], 0);
  }

  Serial.println("Nano Ready");
}


void loop()
{
  ReceiveUART();

  ReadAllTemp();

  ControlAllMosfet();

  if (millis() - timerSend > 500)
  {
    timerSend = millis();

    SendTemperature();
  }
}


void ReceiveUART()
{
  while (Serial.available())
  {
    char c = Serial.read();

    if (c == '\n')
    {
      rxBuffer.trim();

      if (rxBuffer.startsWith("P:"))
      {
        int p1, p2, p3, p4;

        sscanf(
          rxBuffer.c_str(),
          "P:%d,%d,%d,%d",
          &p1,
          &p2,
          &p3,
          &p4
        );

        pwmValue[0] = constrain(p1, 0, 255);
        pwmValue[1] = constrain(p2, 0, 255);
        pwmValue[2] = constrain(p3, 0, 255);
        pwmValue[3] = constrain(p4, 0, 255);
      }

      rxBuffer = "";
    }
    else
    {
      rxBuffer += c;
    }
  }
}


void ReadAllTemp()
{
  for (byte i = 0; i < N; i++)
  {
    sensor[i]->requestTemperatures();

    temp[i] =
      sensor[i]->getTempCByIndex(0);
  }
}


void ControlAllMosfet()
{
  for (byte i = 0; i < N; i++)
  {
    if (temp[i] >= TEMP_LIMIT)
    {
      analogWrite(
        mosPin[i],
        0
      );
    }
    else
    {
      analogWrite(
        mosPin[i],
        pwmValue[i]
      );
    }
  }
}


void SendTemperature()
{
  Serial.print("T:");

  for (byte i = 0; i < N; i++)
  {
    Serial.print(temp[i], 1);

    if (i < N - 1)
    {
      Serial.print(",");
    }
  }

  Serial.println();
}