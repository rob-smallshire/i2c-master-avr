/*
  I2C.cpp - I2C library
  Copyright (c) 2011-2012 Wayne Truchsess.  All right reserved.
  Rev 5.0 - January 24th, 2012
          - Removed the use of interrupts completely from the library
            so TWI state changes are now polled. 
          - Added calls to lockup() function in most functions 
            to combat arbitration problems 
          - Fixed scan() procedure which left timeouts enabled 
            and set to 80msec after exiting procedure
          - Changed scan() address range back to 0 - 0x7F
          - Removed all Wire legacy functions from library
          - A big thanks to Richard Baldwin for all the testing
            and feedback with debugging bus lockups!
  Rev 4.0 - January 14th, 2012
          - Updated to make compatible with 8MHz clock frequency
  Rev 3.0 - January 9th, 2012
          - Modified library to be compatible with Arduino 1.0
          - Changed argument type from boolean to uint8_t in pullUp(), 
            setSpeed() and receiveByte() functions for 1.0 compatability
          - Modified return values for timeout feature to report
            back where in the transmission the timeout occured.
          - added function scan() to perform a bus scan to find devices
            attached to the I2C bus.  Similar to work done by Todbot
            and Nick Gammon
  Rev 2.0 - September 19th, 2011
          - Added support for timeout function to prevent 
            and recover from bus lockup (thanks to PaulS
            and CrossRoads on the Arduino forum)
          - Changed return type for stop() from void to
            uint8_t to handle timeOut function 
  Rev 1.0 - August 8th, 2011
  
  This is a modified version of the Arduino Wire/TWI 
  library.  Functions were rewritten to provide more functionality
  and also the use of Repeated Start.  Some I2C devices will not
  function correctly without the use of a Repeated Start.  The 
  initial version of this library only supports the Master.


  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <inttypes.h>
#include <string.h>

#include <avr/io.h>

extern "C" {
#include "elapsed.h"
}

#include "i2c.h"

#define START           0x08
#define REPEATED_START  0x10
#define MT_SLA_ACK	0x18
#define MT_SLA_NACK	0x20
#define MT_DATA_ACK     0x28
#define MT_DATA_NACK    0x30
#define MR_SLA_ACK	0x40
#define MR_SLA_NACK	0x48
#define MR_DATA_ACK     0x50
#define MR_DATA_NACK    0x58
#define LOST_ARBTRTN    0x38
#define TWI_STATUS      (TWSR & 0xF8)
#define SLA_W(address)  (address << 1)
#define SLA_R(address)  ((address << 1) + 0x01)
#define cbi(sfr, bit)   (_SFR_BYTE(sfr) &= ~_BV(bit))
#define sbi(sfr, bit)   (_SFR_BYTE(sfr) |= _BV(bit))

#define MAX_BUFFER_SIZE 32


#define CHECKED(expr, original_status, translated_status) {          \
    	uint8_t status = expr;                                       \
        if (status) {                                                \
        	if (status == original_status) return translated_status; \
        	return status;                                           \
        }                                                            \
    }


namespace {

// State variables
uint8_t nack = 0;
uint8_t data[MAX_BUFFER_SIZE];
uint8_t bytesAvailable = 0;
uint8_t bufferIndex = 0;
uint8_t totalBytes = 0;
uint16_t timeOutDelay = 0;

void lockUp()
{
  TWCR = 0; // releases SDA and SCL lines to high impedance
  TWCR = _BV(TWEN) | _BV(TWEA); // reinitialize TWI 
}

uint8_t receiveByte(uint8_t ack)
{
  unsigned long startingTime = millis();
  if(ack)
  {
    TWCR = (1<<TWINT) | (1<<TWEN) | (1<<TWEA);

  }
  else
  {
    TWCR = (1<<TWINT) | (1<<TWEN);
  }
  while (!(TWCR & (1<<TWINT)))
  {
    if(!timeOutDelay){continue;}
    if((millis() - startingTime) >= timeOutDelay)
    {
      lockUp();
      return 1;
    }
  }
  if (TWI_STATUS == LOST_ARBTRTN)
  {
    uint8_t bufferedStatus = TWI_STATUS;
    lockUp();
    return bufferedStatus;
  }
  return(TWI_STATUS); 
}

uint8_t readBytes(uint8_t numberBytes, uint8_t *dataBuffer)
{
	for(uint8_t i = 0; i < numberBytes; i++)
	{
	    if(i == nack)
	    {
	      uint8_t returnStatus = receiveByte(0);
	      if (returnStatus == 1) return 6;
	      if (returnStatus != MR_DATA_NACK) return returnStatus;
	    }
	    else
	    {
	      uint8_t returnStatus = receiveByte(1);
	      if (returnStatus == 1) return 6;
	      if (returnStatus != MR_DATA_ACK) return returnStatus;
	    }
	    dataBuffer[i] = TWDR;
	    bytesAvailable = i+1;
	    totalBytes = i+1;
	}
	return 0;
}

uint8_t start()
{
  unsigned long startingTime = millis();
  TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);
  while (!(TWCR & (1<<TWINT)))
  {
    if(!timeOutDelay){continue;}
    if((millis() - startingTime) >= timeOutDelay)
    {
      lockUp();
      return 1;
    }
       
  }
  if ((TWI_STATUS == START) || (TWI_STATUS == REPEATED_START))
  {
    return 0;
  }
  if (TWI_STATUS == LOST_ARBTRTN)
  {
    uint8_t bufferedStatus = TWI_STATUS;
    lockUp();
    return bufferedStatus;
  }
  return TWI_STATUS;
}

uint8_t stop()
{
  unsigned long startingTime = millis();
  TWCR = (1<<TWINT)|(1<<TWEN)| (1<<TWSTO);
  while ((TWCR & (1<<TWSTO)))
  {
    if(!timeOutDelay){continue;}
    if((millis() - startingTime) >= timeOutDelay)
    {
      lockUp();
      return 1;
    }
       
  }
  return(0);
}    

uint8_t sendAddress(uint8_t i2cAddress)
{
  TWDR = i2cAddress;
  unsigned long startingTime = millis();
  TWCR = (1<<TWINT) | (1<<TWEN);
  while (!(TWCR & (1<<TWINT)))
  {
    if(!timeOutDelay){continue;}
    if((millis() - startingTime) >= timeOutDelay)
    {
      lockUp();
      return 1;
    }
       
  }
  if ((TWI_STATUS == MT_SLA_ACK) || (TWI_STATUS == MR_SLA_ACK))
  {
    return 0;
  }
  uint8_t bufferedStatus = TWI_STATUS;
  if ((TWI_STATUS == MT_SLA_NACK) || (TWI_STATUS == MR_SLA_NACK))
  {
    stop();
    return bufferedStatus;
  }
  else
  {
    lockUp();
    return bufferedStatus;
  } 
}

uint8_t sendByte(uint8_t i2cData)
{
  TWDR = i2cData;
  unsigned long startingTime = millis();
  TWCR = (1<<TWINT) | (1<<TWEN);
  while (!(TWCR & (1<<TWINT)))
  {
    if(!timeOutDelay){continue;}
    if((millis() - startingTime) >= timeOutDelay)
    {
      lockUp();
      return 1;
    }
       
  }
  if (TWI_STATUS == MT_DATA_ACK)
  {
    return 0;
  }
  uint8_t bufferedStatus = TWI_STATUS;
  if (TWI_STATUS == MT_DATA_NACK)
  {
    stop();
    return bufferedStatus;
  }
  else
  {
    lockUp();
    return bufferedStatus;
  } 
}

} // end unnamed namespace

namespace I2C {

void begin()
{
  pullup(0);
  // initialize TWI prescaler and bit rate
  cbi(TWSR, TWPS0);
  cbi(TWSR, TWPS1);
  TWBR = ((F_CPU / 100000) - 16) / 2;
  // enable TWI module and acks
  TWCR = _BV(TWEN) | _BV(TWEA); 
}

void end()
{
  TWCR = 0;
}

void timeOut(uint16_t _timeOut)
{
  timeOutDelay = _timeOut;
}

void setSpeed(uint8_t _fast)
{
  if(!_fast)
  {
    TWBR = ((F_CPU / 100000) - 16) / 2;
  }
  else
  {
    TWBR = ((F_CPU / 400000) - 16) / 2;
  }
}
  
void pullup(uint8_t activate)
{
  if(activate)
  {
    #if defined(__AVR_ATmega168__) || defined(__AVR_ATmega8__) || defined(__AVR_ATmega328P__)
      // activate internal pull-ups for TWI
      // as per note from ATmega8 manual pg167
      sbi(PORTC, 4);
      sbi(PORTC, 5);
    #else
      // activate internal pull-ups for TWI
      // as per note from ATmega128 manual pg204
      sbi(PORTD, 0);
      sbi(PORTD, 1);
    #endif
  }
  else
  {
    #if defined(__AVR_ATmega168__) || defined(__AVR_ATmega8__) || defined(__AVR_ATmega328P__)
      // deactivate internal pull-ups for twi
      // as per note from ATmega8 manual pg167
      cbi(PORTC, 4);
      cbi(PORTC, 5);
    #else
      // deactivate internal pull-ups for twi
      // as per note from ATmega128 manual pg204
      cbi(PORTD, 0);
      cbi(PORTD, 1);
    #endif
  }
}

/**
 *  Scan for a device at an address.
 *
 *  Return values:
 *    -2 : Bus error
 *    -1 : Address out of range
 *     0 : No device at address
 *     1 : Device found at address
 */
int scan(uint8_t address)
{
    timeOut(80);
    if (address > 0x7f)
    {
        return -1;
    }
    uint8_t returnStatus = start();
    if (!returnStatus)
    { 
      returnStatus = sendAddress(SLA_W(address));
    }
    if (returnStatus)
    {
        if(returnStatus == 1)
        {
             return -2;
        }
    }
    else
    {
        stop();
 	return 1;
    }
    stop();
    return 0;
}

uint8_t available()
{
  return bytesAvailable;
}

uint8_t receive()
{
  bufferIndex = totalBytes - bytesAvailable;
  if (!bytesAvailable)
  {
    bufferIndex = 0;
    return 0;
  }
  bytesAvailable--;
  return data[bufferIndex];
}

  
/*return values for new functions that use the timeOut feature 
  will now return at what point in the transmission the timeout
  occurred. Looking at a full communication sequence between a 
  master and slave (transmit data and then readback data) there
  a total of 7 points in the sequence where a timeout can occur.
  These are listed below and correspond to the returned value:
  1 - Waiting for successful completion of a Start bit
  2 - Waiting for ACK/NACK while addressing slave in transmit mode (MT)
  3 - Waiting for ACK/NACK while sending data to the slave
  4 - Waiting for successful completion of a Repeated Start
  5 - Waiting for ACK/NACK while addressing slave in receiver mode (MR)
  6 - Waiting for ACK/NACK while receiving data from the slave
  7 - Waiting for successful completion of the Stop bit

  All possible return values:
  0           Function executed with no errors
  1 - 7       Timeout occurred, see above list
  8 - 0xFF    See datasheet for exact meaning */ 


/////////////////////////////////////////////////////

uint8_t write(uint8_t address, uint8_t registerAddress)
{
  CHECKED(start(), 0, 0);
  CHECKED(sendAddress(SLA_W(address)), 1, 2);
  CHECKED(sendByte(registerAddress), 1, 3);
  CHECKED(stop(), 1, 7);
  return 0;
}

uint8_t write(int address, int registerAddress)
{
  return write((uint8_t) address, (uint8_t) registerAddress);
}

uint8_t write(uint8_t address, uint8_t registerAddress, uint8_t data)
{
  CHECKED(start(), 0, 0);
  CHECKED(sendAddress(SLA_W(address)), 1, 2);
  CHECKED(sendByte(registerAddress), 1, 3);
  CHECKED(sendByte(data), 1, 3);
  CHECKED(stop(), 1, 7);
  return 0;
}

uint8_t write(int address, int registerAddress, int data)
{
  return write((uint8_t) address, (uint8_t) registerAddress, (uint8_t) data);
}

uint8_t write(uint8_t address, uint8_t registerAddress, char *data)
{
  uint8_t bufferLength = strlen(data);
  uint8_t returnStatus = write(address, registerAddress, (uint8_t*)data, bufferLength);
  return returnStatus;
}

uint8_t write(uint8_t address, uint8_t registerAddress, uint8_t *data, uint8_t numberBytes)
{
  CHECKED(start(), 0, 0);
  CHECKED(sendAddress(SLA_W(address)), 1, 2);
  CHECKED(sendByte(registerAddress), 1, 3);

  for (uint8_t i = 0; i < numberBytes; i++)
  {
	CHECKED(sendByte(data[i]), 1, 3);
  }

  CHECKED(stop(), 1, 7);
  return 0;
}

uint8_t writeBytes(uint8_t address, uint8_t registerAddress, uint8_t numBytes, ...) {
	CHECKED(start(), 0, 0);
	CHECKED(sendAddress(SLA_W(address)), 1, 2);
	CHECKED(sendByte(registerAddress), 1, 3);

	va_list args;
	va_start(args, numBytes);
	for (uint8_t i = 0; i < numBytes; ++i) {
		uint8_t b = (uint8_t) va_arg(args, int);
		CHECKED(sendByte(b), 1, 3);
	}

	CHECKED(stop(), 1, 7);
	return 0;
}

uint8_t read(int address, int numberBytes)
{
  return(read((uint8_t) address, (uint8_t) numberBytes));
}

uint8_t read(uint8_t address, uint8_t numberBytes)
{
  bytesAvailable = 0;
  bufferIndex = 0;
  if(numberBytes == 0){numberBytes++;}
  nack = numberBytes - 1;

  CHECKED(start(), 0, 0);
  CHECKED(sendAddress(SLA_R(address)), 1, 5);
  CHECKED(readBytes(numberBytes, data), 0, 0);
  CHECKED(stop(), 1, 7);
  return 0;
}

uint8_t read(int address, int registerAddress, int numberBytes)
{
  return read((uint8_t) address, (uint8_t) registerAddress, (uint8_t) numberBytes);
}

uint8_t read(uint8_t address, uint8_t registerAddress, uint8_t numberBytes)
{
  bytesAvailable = 0;
  bufferIndex = 0;
  if(numberBytes == 0){numberBytes++;}
  nack = numberBytes - 1;

  CHECKED(start(), 0, 0);
  CHECKED(sendAddress(SLA_W(address)), 1, 2);
  CHECKED(sendByte(registerAddress), 1, 3);
  CHECKED(start(), 1, 4);
  CHECKED(sendAddress(SLA_R(address)), 1, 5);
  CHECKED(readBytes(numberBytes, data), 0, 0);
  CHECKED(stop(), 1, 7);
  return 0;
}

uint8_t read(uint8_t address, uint8_t numberBytes, uint8_t *dataBuffer)
{
  bytesAvailable = 0;
  bufferIndex = 0;
  if(numberBytes == 0){numberBytes++;}
  nack = numberBytes - 1;

  CHECKED(start(), 0, 0);
  CHECKED(sendAddress(SLA_R(address)), 1, 5);
  CHECKED(readBytes(numberBytes, dataBuffer), 0, 0);
  CHECKED(stop(), 1, 7);
  return 0;
}

uint8_t read(uint8_t address, uint8_t registerAddress, uint8_t numberBytes, uint8_t *dataBuffer)
{
  bytesAvailable = 0;
  bufferIndex = 0;
  if(numberBytes == 0){numberBytes++;}
  nack = numberBytes - 1;

  CHECKED(start(), 0, 0);
  CHECKED(sendAddress(SLA_W(address)), 1, 2);
  CHECKED(sendByte(registerAddress), 1, 3);
  CHECKED(start(), 1, 4);
  CHECKED(sendAddress(SLA_R(address)), 1, 5);
  CHECKED(readBytes(numberBytes, dataBuffer), 0, 0);
  CHECKED(stop(), 1, 7);
  return 0;
}

} // end namespace I2C
