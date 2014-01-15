/******************************************************************
** Código creado en Electrónica 5Hz                              **
** www.5hz-electronica.com                                       **
** Por: Rafael Almaguer                                          **
**                                                               **
** Descripción del código:                                       **
**                                                               **
** Ejemplo para el sensor de presión y temperatura BMP085        **
**                                                               **
*******************************************************************
Conexiones:
UNO   BMP085

3.3V  VCC
GND   GND
SDA    SDA
SCL    SCL
*/

// Libreria para comunicarse por medio del protocolo I2C
#include <Wire.h>

#define BMP085_ADDRESS 0x77  // dirección I2C del BMP085

const unsigned char OSS = 0;  // Configuración de Oversampling

// Valores de calibración
int ac1;
int ac2; 
int ac3; 
unsigned int ac4;
unsigned int ac5;
unsigned int ac6;
int b1; 
int b2;
int mb;
int mc;
int md;

//b5 es calculado en bmp085GetTemperature(...), esta variable tambien es usada en bmp085GetPressure(...)
//asi que la funcion de temperatura debe ser llamada antes que la de presión
// b5 is calculated in bmp085GetTemperature(...), this variable is also used in bmp085GetPressure(...)
// so ...Temperature(...) must be called before ...Pressure(...).
long b5; 

float temperature;
long pressure;

// Usar estos como conversiones de altitud
const float p0 = 101325;     // Pressure at sea level (Pa)
float altitude;

void setup()
{
  Serial.begin(9600);
  Wire.begin();
  bmp085Calibration();
}

void loop()
{
  temperature = bmp085GetTemperature(bmp085ReadUT());
  pressure = bmp085GetPressure(bmp085ReadUP());
  Serial.print("Temperatura: ");
  Serial.print(temperature, 1);
  Serial.println(" grados C");
  Serial.print("Presion: ");
  Serial.print(pressure);
  Serial.println(" Pa");
  Serial.println();
  
  delay(1000);
}

//***************FUNCIONES I2C***********************************
// Lee 1 byte de el BMP085 en 'address'
char bmp085Read(unsigned char address)
{ 
  Wire.beginTransmission(BMP085_ADDRESS);
  Wire.write(address);
  Wire.endTransmission();
  
  Wire.requestFrom(BMP085_ADDRESS, 1);
  wait4data(1);
    
  return Wire.read();
}

// Lee 2 bytes del BMP085
// Primer byte será desde 'address'
//Segundo byte será desde 'address' + 1
int bmp085ReadInt(unsigned char address)
{
  unsigned char msb, lsb;
  
  Wire.beginTransmission(BMP085_ADDRESS);
  Wire.write(address);
  Wire.endTransmission();
  
  Wire.requestFrom(BMP085_ADDRESS, 2);
  wait4data(2);
  msb = Wire.read();
  lsb = Wire.read();
  
  return (int) msb<<8 | lsb;
}

//Función para hacer más entendible el código, 
//espera hasta recibir un cierto número de Bytes por I2C
void wait4data(byte howManyBytes)
{
  while(Wire.available()<howManyBytes);
}

//*****************************************************************

//**************FUNCIONES PARA VALORES DE CALIBRACIÓN y NO COMPENSADOS****************
//Guarda todos los valores de calibración del sensor en variables globales
//Los valores de calibración son requeridos para calcular la presión y temperatura.
//Esta función debe ser llamada al inicio del programa.
void bmp085Calibration()
{
  ac1 = bmp085ReadInt(0xAA);
  ac2 = bmp085ReadInt(0xAC);
  ac3 = bmp085ReadInt(0xAE);
  ac4 = bmp085ReadInt(0xB0);
  ac5 = bmp085ReadInt(0xB2);
  ac6 = bmp085ReadInt(0xB4);
  b1 = bmp085ReadInt(0xB6);
  b2 = bmp085ReadInt(0xB8);
  mb = bmp085ReadInt(0xBA);
  mc = bmp085ReadInt(0xBC);
  md = bmp085ReadInt(0xBE);
}

// Lee el valor de temperatura sin compensar
unsigned int bmp085ReadUT()
{
  unsigned int ut;
  
  // escribe 0x2E al registro 0xF4
  // Esto solicita la lectura de temperatura
  Wire.beginTransmission(BMP085_ADDRESS);
  Wire.write(0xF4);
  Wire.write(0x2E);
  Wire.endTransmission();
  
  // Espera al menos 4.5ms
  delay(5);
  
  // Lee 2 bytes de los registros 0xF6 y 0xF7
  ut = bmp085ReadInt(0xF6);
  return ut;
}

// Lee el valor de presión sin compensar
unsigned long bmp085ReadUP()
{
  unsigned char msb, lsb, xlsb;
  unsigned long up = 0;
  
  // Escribe 0x34+(OSS<<6) en el registro 0xF4
  // Solicita una lectura de presión con la configuración de Oversampling
  Wire.beginTransmission(BMP085_ADDRESS);
  Wire.write(0xF4);
  Wire.write(0x34 + (OSS<<6));
  Wire.endTransmission();
  
  // Espera por la conversión, el tiempo de espera es dependiente de OSS
  delay(2 + (3<<OSS));
  
  // Lee el registro 0xF6 (MSB), 0xF7 (LSB), y 0xF8 (XLSB)
  Wire.beginTransmission(BMP085_ADDRESS);
  Wire.write(0xF6);
  Wire.endTransmission();
  Wire.requestFrom(BMP085_ADDRESS, 3);
  
  // Espera los datos a estar disponibles
  wait4data(3);
  msb = Wire.read();
  lsb = Wire.read();
  xlsb = Wire.read();
  
  up = (((unsigned long) msb << 16) | ((unsigned long) lsb << 8) | (unsigned long) xlsb) >> (8-OSS);
  
  return up;
}

//****************************************************************************************

//**************************TEMPERATURA************************************************
// Calcula la temperatura para un ut dado
// El valor regresado será en décimas de grado centígrado (*0.1 °C)
float bmp085GetTemperature(unsigned int ut)
{
  long x1, x2;
  
  x1 = (((long)ut - (long)ac6)*(long)ac5) >> 15;
  x2 = ((long)mc << 11)/(x1 + md);
  b5 = x1 + x2;

  short decimasGrados = (b5 + 8)>>4;
  return decimasGrados/10.0;
}

//*************************************************************************************

//*************************PRESIÓN****************************************************
//Calcula presión dada
//Deben conocerse los valores de calibración
//b5 tambien es requerida asi que bmp085GetTemperature(...) debe ser llamada primero
//El valor regresado es en unidades de Pa.
long bmp085GetPressure(unsigned long up)
{
  long x1, x2, x3, b3, b6, p;
  unsigned long b4, b7;
  
  b6 = b5 - 4000;
  // Calcula B3
  x1 = (b2 * (b6 * b6)>>12)>>11;
  x2 = (ac2 * b6)>>11;
  x3 = x1 + x2;
  b3 = (((((long)ac1)*4 + x3)<<OSS) + 2)>>2;
  
  // Calcula B4
  x1 = (ac3 * b6)>>13;
  x2 = (b1 * ((b6 * b6)>>12))>>16;
  x3 = ((x1 + x2) + 2)>>2;
  b4 = (ac4 * (unsigned long)(x3 + 32768))>>15;
  
  b7 = ((unsigned long)(up - b3) * (50000>>OSS));
  if (b7 < 0x80000000)
    p = (b7<<1)/b4;
  else
    p = (b7/b4)<<1;
    
  x1 = (p>>8) * (p>>8);
  x1 = (x1 * 3038)>>16;
  x2 = (-7357 * p)>>16;
  p += (x1 + x2 + 3791)>>4;
  
  return p;
}
//*************************************************************************************
