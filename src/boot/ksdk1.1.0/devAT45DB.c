/*
  Authored 2021. Phillip Stanley-Marbell.

  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  *	Redistributions of source code must retain the above
    copyright notice, this list of conditions and the following
    disclaimer.

  *	Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following
    disclaimer in the documentation and/or other materials
    provided with the distribution.

  *	Neither the name of the author nor the names of its
    contributors may be used to endorse or promote products
    derived from this software without specific prior written
    permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.
*/
#include <stdlib.h>

/*
 *	config.h needs to come first
 */
#include "config.h"

#include "fsl_misc_utilities.h"
#include "fsl_device_registers.h"
#include "fsl_i2c_master_driver.h"
#include "fsl_spi_master_driver.h"
#include "fsl_rtc_driver.h"
#include "fsl_clock_manager.h"
#include "fsl_power_manager.h"
#include "fsl_mcglite_hal.h"
#include "fsl_port_hal.h"

#include "errstrs.h"
#include "gpio_pins.h"
#include "SEGGER_RTT.h"
#include "warp.h"
#include "devAT45DB.h"

extern volatile WarpSPIDeviceState	deviceAT45DBState;
extern volatile uint32_t			gWarpSpiTimeoutMicroseconds;
extern uint8_t						gWarpSpiCommonSourceBuffer[];
extern uint8_t						gWarpSpiCommonSinkBuffer[];
extern uint8_t						gWarpWriteToFlash;

/* Read commands */
#define AT45DB_RDMN          0xd2 /* Main Memory Page Read */
#define AT45DB_RDARRY        0xe8 /* Continuous Array Read (Legacy Command) */
#define AT45DB_RDARRAYLF     0x03 /* Continuous Array Read (Low Frequency) */
#define AT45DB_RDARRAYHF     0x0b /* Continuous Array Read (High Frequency) */
#define AT45DB_RDBF1LF       0xd1 /* Buffer 1 Read (Low Frequency) */
#define AT45DB_RDBF2LF       0xd3 /* Buffer 2 Read (Low Frequency) */
#define AT45DB_RDBF1         0xd4 /* Buffer 1 Read */
#define AT45DB_RDBF2         0xd6 /* Buffer 2 Read */

/* Program and Erase Commands */
#define AT45DB_WRBF1         0x84 /* Buffer 1 Write */
#define AT45DB_WRBF2         0x87 /* Buffer 2 Write */
#define AT45DB_BF1TOMNE      0x83 /* Buffer 1 to Main Memory Page Program with Built-in Erase */
#define AT45DB_BF2TOMNE      0x86 /* Buffer 2 to Main Memory Page Program with Built-in Erase */
#define AT45DB_BF1TOMN       0x88 /* Buffer 1 to Main Memory Page Program without Built-in Erase */
#define AT45DB_BF2TOMN       0x89 /* Buffer 2 to Main Memory Page Program without Built-in Erase  */
#define AT45DB_PGERASE       0x81 /* Page Erase */
#define AT45DB_BLKERASE      0x50 /* Block Erase */
#define AT45DB_SECTERASE     0x7c /* Sector Erase */
#define AT45DB_CHIPERASE1    0xc7 /* Chip Erase - byte 1 */
#define AT45DB_CHIPERASE2  0x94 /* Chip Erase - byte 2 */
#define AT45DB_CHIPERASE3  0x80 /* Chip Erase - byte 3 */
#define AT45DB_CHIPERASE4  0x9a /* Chip Erase - byte 4 */
#define AT45DB_MNTHRUBF1     0x82 /* Main Memory Page Program Through Buffer 1 */
#define AT45DB_MNTHRUBF2     0x85 /* Main Memory Page Program Through Buffer 2 */

/* Protection and Security Commands */
#define AT45DB_ENABPROT1     0x3d /* Enable Sector Protection - byte 1 */
#define AT45DB_ENABPROT2   0x2a /* Enable Sector Protection - byte 2 */
#define AT45DB_ENABPROT3   0x7f /* Enable Sector Protection - byte 3 */
#define AT45DB_ENABPROT4   0xa9 /* Enable Sector Protection - byte 4 */
#define AT45DB_DISABPROT1    0x3d /* Disable Sector Protection - byte 1 */
#define AT45DB_DISABPROT2  0x2a /* Disable Sector Protection - byte 2 */
#define AT45DB_DISABPROT3  0x7f /* Disable Sector Protection - byte 3 */
#define AT45DB_DISABPROT4  0x9a /* Disable Sector Protection - byte 4 */
#define AT45DB_ERASEPROT1    0x3d /* Erase Sector Protection Register - byte 1 */
#define AT45DB_ERASEPROT2  0x2a /* Erase Sector Protection Register - byte 2 */
#define AT45DB_ERASEPROT3  0x7f /* Erase Sector Protection Register - byte 3 */
#define AT45DB_ERASEPROT4  0xcf /* Erase Sector Protection Register - byte 4 */
#define AT45DB_PROGPROT1     0x3d /* Program Sector Protection Register - byte 1 */
#define AT45DB_PROGPROT2   0x2a /* Program Sector Protection Register - byte 2 */
#define AT45DB_PROGPROT3   0x7f /* Program Sector Protection Register - byte 3 */
#define AT45DB_PROGPROT4   0xfc /* Program Sector Protection Register - byte 4 */
#define AT45DB_RDPROT        0x32 /* Read Sector Protection Register */
#define AT45DB_LOCKDOWN1     0x3d /* Sector Lockdown - byte 1 */
#define AT45DB_LOCKDOWN2   0x2a /* Sector Lockdown - byte 2 */
#define AT45DB_LOCKDOWN3   0x7f /* Sector Lockdown - byte 3 */
#define AT45DB_LOCKDOWN4   0x30 /* Sector Lockdown - byte 4 */
#define AT45DB_RDLOCKDOWN    0x35 /* Read Sector Lockdown Register  */
#define AT45DB_PROGSEC1      0x9b /* Program Security Register - byte 1 */
#define AT45DB_PROGSEC2    0x00 /* Program Security Register - byte 2 */
#define AT45DB_PROGSEC3    0x00 /* Program Security Register - byte 3 */
#define AT45DB_PROGSEC4    0x00 /* Program Security Register - byte 4 */
#define AT45DB_RDSEC         0x77 /* Read Security Register */

/* Additional commands */
#define AT45DB_MNTOBF1XFR    0x53 /* Main Memory Page to Buffer 1 Transfer */
#define AT45DB_MNTOBF2XFR    0x55 /* Main Memory Page to Buffer 2 Transfer */
#define AT45DB_MNBF1CMP      0x60 /* Main Memory Page to Buffer 1 Compare  */
#define AT45DB_MNBF2CMP      0x61 /* Main Memory Page to Buffer 2 Compare */
#define AT45DB_AUTOWRBF1     0x58 /* Auto Page Rewrite through Buffer 1 */
#define AT45DB_AUTOWRBF2     0x59 /* Auto Page Rewrite through Buffer 2 */
#define AT45DB_PWRDOWN       0xb9 /* Deep Power-down */
#define AT45DB_RESUME        0xab /* Resume from Deep Power-down */
#define AT45DB_RDSR          0xd7 /* Status Register Read */
#define AT45DB_RDDEVID       0x9f /* Manufacturer and Device ID Read */

#define AT45DB_MANUFACTURER  0x1f /* Manufacturer ID: Atmel */
#define AT45DB_DEVID1_CAPMSK 0x1f /* Bits 0-4: Capacity */
#define AT45DB_DEVID1_1MBIT  0x02 /* xxx0 0010 = 1Mbit AT45DB011 */
#define AT45DB_DEVID1_2MBIT  0x03 /* xxx0 0012 = 2Mbit AT45DB021 */
#define AT45DB_DEVID1_4MBIT  0x04 /* xxx0 0100 = 4Mbit AT45DB041 */
#define AT45DB_DEVID1_8MBIT  0x05 /* xxx0 0101 = 8Mbit AT45DB081 */
#define AT45DB_DEVID1_16MBIT 0x06 /* xxx0 0110 = 16Mbit AT45DB161 */
#define AT45DB_DEVID1_32MBIT 0x07 /* xxx0 0111 = 32Mbit AT45DB321 */
#define AT45DB_DEVID1_64MBIT 0x08 /* xxx0 1000 = 32Mbit AT45DB641 */
#define AT45DB_DEVID1_FAMMSK 0xe0 /* Bits 5-7: Family */
#define AT45DB_DEVID1_DFLASH 0x20 /* 001x xxxx = Dataflash */
#define AT45DB_DEVID1_AT26DF 0x40 /* 010x xxxx = AT26DFxxx series (Not supported) */
#define AT45DB_DEVID2_VERMSK 0x1f /* Bits 0-4: MLC mask */
#define AT45DB_DEVID2_MLCMSK 0xe0 /* Bits 5-7: MLC mask */

/* Status register bit definitions */
#define AT45DB_SR_RDY       (1 << 7) /* Bit 7: RDY/ Not BUSY */
#define AT45DB_SR_COMP      (1 << 6) /* Bit 6: COMP */
#define AT45DB_SR_PROTECT   (1 << 1) /* Bit 1: PROTECT */
#define AT45DB_SR_PGSIZE    (1 << 0) /* Bit 0: PAGE_SIZE */

WarpStatus
initAT45DB(int chipSelectIoPinID, uint16_t operatingVoltageMillivolts)
{
  deviceAT45DBState.chipSelectIoPinID				= chipSelectIoPinID;
  deviceAT45DBState.spiSourceBuffer				= gWarpSpiCommonSourceBuffer;
  deviceAT45DBState.spiSinkBuffer					= gWarpSpiCommonSinkBuffer;
  deviceAT45DBState.spiBufferLength				= kWarpMemoryCommonSpiBufferBytes;
  deviceAT45DBState.operatingVoltageMillivolts	= operatingVoltageMillivolts;
}

WarpStatus
spiTransactionAT45DB(WarpSPIDeviceState volatile *  deviceStatePointer, uint8_t ops[], size_t opCount)
{
  spi_status_t	status;

  if (opCount > deviceAT45DBState.spiBufferLength)
  {
    return kWarpStatusBadDeviceCommand;
  }

  warpScaleSupplyVoltage(deviceAT45DBState.operatingVoltageMillivolts);

  /*
   *	First, configure chip select pins of the various SPI slave devices
   *	as GPIO and drive all of them high.
   */
  warpDeasserAllSPIchipSelects();

  for (int i = 0; (i < opCount) && (i < deviceAT45DBState.spiBufferLength); i++)
  {
    deviceAT45DBState.spiSourceBuffer[i] = ops[i];
    deviceAT45DBState.spiSinkBuffer[i] = 0xFF;
  }

  /*
   *	First, pulse the /CS to wake up device in case it is in Ultra-Deep Power-Down mode.
   *	(See Section 10.2.1 of manual). The 50 milliseconds we use here is conservative:
   *	the manual says the minimum time needed is 20ns. However, the datasheet also says:
   *
   *		"After the CS pin has been deasserted, the device will exit from the
   *		Ultra-Deep Power-Down mode and return to the standby mode within a
   *		maximum time of tXUDPD."
   *
   *	Since txudpd is 100us, we wait another millisecond before proceeding to assert /CS
   *	again below.
   */
  GPIO_DRV_ClearPinOutput(deviceAT45DBState.chipSelectIoPinID);
  OSA_TimeDelay(1);
  GPIO_DRV_SetPinOutput(deviceAT45DBState.chipSelectIoPinID);
  OSA_TimeDelay(1);

  /*
   *	Next, create a falling edge on chip-select.
   */
  GPIO_DRV_ClearPinOutput(deviceAT45DBState.chipSelectIoPinID);

  /*
   *	The result of the SPI transaction will be stored in deviceADXL362State.spiSinkBuffer.
   *	Providing a spi_master_user_config_t is optional since it is already provided when we did
   *	SPI_DRV_MasterConfigureBus(), so we pass in NULL. The "master instance" is always 0 for
   *	the KL03 since there is only one SPI peripheral.
   */
  warpEnableSPIpins();
  status = SPI_DRV_MasterTransferBlocking(0							/*	master instance			*/,
          NULL								/*	spi_master_user_config_t	*/,
          (const uint8_t * restrict)deviceAT45DBState.spiSourceBuffer	/*	source buffer			*/,
          (uint8_t * restrict)deviceAT45DBState.spiSinkBuffer		/*	receive buffer			*/,
          opCount								/*	transfer size			*/,
          gWarpSpiTimeoutMicroseconds);
  warpDisableSPIpins();

  /*
   *	Deassert the AT45DB
   */
  GPIO_DRV_SetPinOutput(deviceAT45DBState.chipSelectIoPinID);

  if (status != kStatus_SPI_Success)
  {
    return kWarpStatusDeviceCommunicationFailed;
  }

  return kWarpStatusOK;
}

void
enableAT45DBWrite() {
  WarpStatus status;
  uint8_t ops[] = {
      0x06, /* WREN */
  };
  status = spiTransactionAT45DB(&deviceAT45DBState, ops, 1);
  if (status != kWarpStatusOK) {
    warpPrint("\r\n\tCommunication failed: %d", status);
  }
}

WarpStatus
SaveToAT45DBFromEnd(size_t nbyte, uint8_t* buf) {
  int writeToFlash = gWarpWriteToFlash;
  gWarpWriteToFlash = kWarpWriteToFlash;
  // assume that nbyte < 32
  WarpStatus status;
  uint8_t pageOffsetBuf[3];
  status = readMemoryAT45DB(0, 3, pageOffsetBuf);
  if (status != kWarpStatusOK) {
  return status;
  }

  uint8_t pageOffset = pageOffsetBuf[2];
  uint16_t pageNumber = pageOffsetBuf[1] | pageOffsetBuf[0] << 8;

  size_t nExtraPagesRequired = (nbyte - pageOffset + (kWarpSizeAT45DBPageSizeBytes - 1)) / kWarpSizeAT45DBPageSizeBytes;
  if (kWarpSizeAT45DBNPages - nExtraPagesRequired < pageNumber) {
    return kWarpStatusBadDeviceCommand;
  }

  bool nextPageRequired = nbyte + pageOffset > kWarpSizeAT45DBPageSizeBytes;

  if (!nextPageRequired) {
    uint8_t fullBufSize = nbyte + pageOffset;
    uint8_t fullBuf[fullBufSize];

    if (pageOffset > 0) {
      status = readMemoryAT45DB(pageNumber, pageOffset, fullBuf);
      if (status != kWarpStatusOK) {
        return status;
      }
    }

    for (int i = 0; i < nbyte; i++) {
      fullBuf[pageOffset + i] = buf[i];
    }

    status = PageProgramAT45DB(pageNumber, fullBufSize, fullBuf);
	if (status != kWarpStatusOK) {
	  return status;
	}

    status = setAT45DBStartOffset(pageNumber, fullBufSize);
	if (status != kWarpStatusOK) {
	  return status;
	}
  }
  else {
	// Write to remaining space in current page
    uint8_t firstBufSize = kWarpSizeAT45DBPageSizeBytes - pageOffset;
    uint8_t firstBuf[kWarpSizeAT45DBPageSizeBytes];

    if (pageOffset > 0) {
      status = readMemoryAT45DB(pageNumber, pageOffset, firstBuf);
      if (status != kWarpStatusOK) {
        return status;
      }
    }

    for (int i = 0; i < firstBufSize; i++) {
      firstBuf[pageOffset + i] = buf[i];
    }

    status = PageProgramAT45DB(pageNumber, kWarpSizeAT45DBPageSizeBytes, firstBuf);
    if (status != kWarpStatusOK) {
      return status;
    }

	// Write to full pages as needed
    uint8_t middleBufSize = nbyte - firstBufSize;
    size_t nIterations = middleBufSize / kWarpSizeAT45DBPageSizeBytes;

    size_t excess = middleBufSize % kWarpSizeAT45DBPageSizeBytes;
    uint8_t* middleBuf = buf + firstBufSize;

    for (int i = 0; i < nIterations; i++) {
      status = PageProgramAT45DB((pageNumber+=1), kWarpSizeAT45DBPageSizeBytes, middleBuf+(i*kWarpSizeAT45DBPageSizeBytes));
      if (status != kWarpStatusOK) {
        return status;
      }
    }

	// Write to remaining bytes
    status = PageProgramAT45DB((pageNumber+=1), excess, middleBuf+(nIterations*kWarpSizeAT45DBPageSizeBytes));
      if (status != kWarpStatusOK) {
        return status;
      }

    if (excess == 32) {
      excess = 0;
      pageNumber += 1;
    }

    status = setAT45DBStartOffset(pageNumber, excess);
    if (status != kWarpStatusOK) {
      return status;
    }
  }
  gWarpWriteToFlash = writeToFlash;
  return kWarpStatusOK;
}

WarpStatus
setAT45DBStartOffset(uint16_t pageNumber, uint8_t pageOffset)
{
  enableAT45DBWrite();

  WarpStatus status;

  uint8_t initialNANDStartPosition[3];
  initialNANDStartPosition[1] = (uint8_t)pageNumber;
  initialNANDStartPosition[0] = (uint8_t)(pageNumber>>=8);
  initialNANDStartPosition[2] = pageOffset;

  status = PageProgramAT45DB(0, 3, initialNANDStartPosition);
  if (status != kWarpStatusOK) {
    return status;
  }
}

WarpStatus
PageProgramAT45DB(uint16_t pageNumber, size_t nbyte, uint8_t *  buf)
{

  WarpStatus	status;

  if (nbyte > kWarpMemoryCommonSpiBufferBytes - 4)
  {
    return kWarpStatusBadDeviceCommand;
  }
  uint8_t	ops[kWarpMemoryCommonSpiBufferBytes] = {0};
  ops[0] = 0x82;	/* PP */
  ops[2] = (uint8_t)(pageNumber <<= 1);
  ops[1] = (uint8_t)(pageNumber >>= 8);
  ops[3] = 0x00;

  for (size_t i = 0; i < nbyte; i++)
  {
    ops[i+4] = buf[i];
  }

  return  spiTransactionAT45DB(&deviceAT45DBState, ops, nbyte + 4);
}

WarpStatus
readMemoryAT45DB(uint16_t pageNumber, size_t nbyte, void *  buf)
{
  WarpStatus	status;
  if (nbyte > kWarpMemoryCommonSpiBufferBytes - 4)
  {
    return kWarpStatusBadDeviceCommand;
  }

  uint8_t	ops[kWarpMemoryCommonSpiBufferBytes] = {0};
  ops[0] = 0xD2;	/* NORD */
  ops[2] = (uint8_t)(pageNumber <<= 1);
  ops[1] = (uint8_t)(pageNumber >>= 8);
  ops[3] = 0x00;
  ops[4] = 0x00;
  ops[5] = 0x00;
  ops[6] = 0x00;
  ops[7] = 0x00;

  status = spiTransactionAT45DB(&deviceAT45DBState, ops, nbyte + 8);
  if (status != kWarpStatusOK)
  {
    return status;
  }

  for (size_t i = 0; i < nbyte; i++)
  {
    ((uint8_t*)buf)[i] = deviceAT45DBState.spiSinkBuffer[i+8];

  }

  return kWarpStatusOK;
}

WarpStatus
eraseSectorAT45DB(uint32_t address)
{
  uint8_t	ops[4] = {0};

  ops[0] = 0x7C;	/* SER (SPI Mode) */
  ops[1] = (uint8_t)((address & 0x0F00) >> 2);
  ops[2] = (uint8_t)((address & 0x00F0) >> 1);
  ops[3] = (uint8_t)((address & 0x000F));

  return spiTransactionAT45DB(&deviceAT45DBState, ops, 4);
}

WarpStatus
erase32kBlockAT45DB(uint32_t address)
{
  uint8_t	ops[4] = {0};

  ops[0] = 0x12;	/* BER32K (SPI Mode) */
  ops[1] = (uint8_t)((address & 0x0F00) >> 2);
  ops[2] = (uint8_t)((address & 0x00F0) >> 1);
  ops[3] = (uint8_t)((address & 0x000F));

  return spiTransactionAT45DB(&deviceAT45DBState, ops, 4);
}
WarpStatus
chipEraseAT45DB()
{
  WarpStatus	status;
  uint8_t	ops[4] = {0};

  ops[0] = 0xC7;	/* CER (SPI Mode) */
  ops[1] = 0x94;
  ops[2] = 0x80;
  ops[3] = 0x9A;

  status =  spiTransactionAT45DB(&deviceAT45DBState, ops, 4);
  return status;
}
WarpStatus
disablesectorprotection()
{
  WarpStatus	status;
  uint8_t	ops[4] = {0};

  ops[0] = 0x3D;	/* CER (SPI Mode) */
  ops[1] = 0x2A;
  ops[2] = 0x7F;
  ops[3] = 0x9A;

  status =  spiTransactionAT45DB(&deviceAT45DBState, ops, 4);
  return status;
}
