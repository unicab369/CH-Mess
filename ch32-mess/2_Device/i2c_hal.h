#include <stdio.h>
#include <stdint.h>
#include "ch32fun.h"

//
// 32-bit unsigned divide
// takes less flash space than the 2K used by the RISC-V math library
//
uint32_t udiv32(uint32_t num, uint32_t den)
{
uint32_t place = 1;
uint32_t ret = 0;
   while ((num >> 1) >= den) {
      place<<=1;
      den<<=1;
   }
   for ( ; place>0; place>>=1, den>>=1) {
      if (num>=den) {
         num-=den;
         ret+=place;
      }
   }
   return ret;
} /* udiv32() */

uint32_t udivmod32(uint32_t num, uint32_t den, uint32_t *pRemainder)
{
uint32_t place = 1;
uint32_t ret = 0;
   while ((num >> 1) >= den) {
      place<<=1;
      den<<=1;
   }
   for ( ; place>0; place>>=1, den>>=1) {
      if (num>=den) {
         num-=den;
         ret+=place;
      }
   }
   if (pRemainder) *pRemainder = num;
   return ret;
} /* udivmod32() */

// 
// 32-bit unsigned modulus
// takes less flash space than the 2K used by the RISC-V math library
// 
uint32_t umod32(uint32_t num, uint32_t den)
{
uint32_t place = 1;
//uint32_t ret = 0;
   while ((num >> 1) >= den) {
      place<<=1;
      den<<=1;
   }
   for ( ; place>0; place>>=1, den>>=1) {
      if (num>=den) {
         num-=den;
//         ret+=place;
      }
   }
   return num;
} /* umod32() */

uint32_t SystemCoreClock = 48000000;
uint32_t u32TickMicros = 48000000 / 8000;

void I2CInit(uint8_t iSDA, uint8_t iSCL, int iSpeed)
{
   uint16_t tmpreg = 0, freqrange = 0;
   uint16_t result = 0x04;
   uint32_t pclk1 = 8000000;

   (void)iSDA; (void)iSCL; // Use C1/C2

      // Enable GPIOC and I2C
      RCC->APB2PCENR |= RCC_APB2Periph_GPIOC;
      RCC->APB1PCENR |= RCC_APB1Periph_I2C1;

      // PC1 is SDA, 10MHz Output, alt func
      GPIOC->CFGLR &= ~(0xf<<(4*1));
      GPIOC->CFGLR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_OD_AF)<<(4*1);

      // PC2 is SCL, 10MHz Output, alt func
      GPIOC->CFGLR &= ~(0xf<<(4*2));
      GPIOC->CFGLR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_OD_AF)<<(4*2);

// reset I2C registers
      RCC->APB1PRSTR |= RCC_APB1Periph_I2C1;
      RCC->APB1PRSTR &= ~RCC_APB1Periph_I2C1;
   I2C1->CTLR1 |= I2C_CTLR1_SWRST;
   I2C1->CTLR1 &= ~I2C_CTLR1_SWRST;

   tmpreg = I2C1->CTLR2;
   tmpreg &= CTLR2_FREQ_Reset;
//    RCC_GetClocksFreq(&rcc_clocks);
   pclk1 = SystemCoreClock; //rcc_clocks.PCLK1_Frequency;
   freqrange = (uint16_t)udiv32(pclk1 , 1000000);
   tmpreg |= freqrange;
   I2C1->CTLR2 = tmpreg;

   I2C1->CTLR1 &= CTLR1_PE_Reset;
   tmpreg = 0;

   if(iSpeed <= 100000)
   {
      result = (uint16_t)udiv32(pclk1, (iSpeed << 1));
      if(result < 0x04)
      {
            result = 0x04;
      }

      tmpreg |= result;
   }
   else
   {
//        if(I2C_InitStruct->I2C_DutyCycle == I2C_DutyCycle_2)
//        {
//            result = (uint16_t)(pclk1 / (I2C_InitStruct->I2C_ClockSpeed * 3));
//        }
//        else
//        {
            result = (uint16_t)udiv32(pclk1, iSpeed * 25);
            result |= I2C_DutyCycle_16_9;
//        }

      if((result & CKCFGR_CCR_Set) == 0)
      {
            result |= (uint16_t)0x0001;
      }

      tmpreg |= (uint16_t)(result | CKCFGR_FS_Set);
   }

   I2C1->CKCFGR = tmpreg;

   tmpreg = I2C1->CTLR1;
   tmpreg &= I2C_CTLR1_CLEAR_Mask;
   tmpreg |= (uint16_t)(uint32_t)(I2C_Mode_I2C | I2C_Ack_Enable);
   I2C1->CTLR1 = tmpreg;

   I2C1->OADDR1 = 0x2 | I2C_AcknowledgedAddress_7bit;
   I2C1->CTLR1 |= I2C_CTLR1_PE; // enable I2C

} /* I2CInit() */

#define  I2C_EVENT_MASTER_MODE_SELECT ((uint32_t)0x00030001)  /* BUSY, MSL and SB flag */
#define  I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED ((uint32_t)0x00070082)  /* BUSY, MSL, ADDR, TXE and TRA flags */
#define  I2C_EVENT_MASTER_BYTE_TRANSMITTED ((uint32_t)0x00070084)  /* TRA, BUSY, MSL, TXE and BTF flags */
// #define I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED              ((uint32_t)0x00030002) /* BUSY, MSL and ADDR flags */
/* I2C FLAG mask */
#define I2C_FLAG_Mask                ((uint32_t)0x00FFFFFF)
#define CTLR1_ACK_Reset          ((uint16_t)0xFBFF)
#define TIMEOUT 100000

uint8_t I2C_CheckEvent(uint32_t event_mask)
{
uint32_t status = I2C1->STAR1 | (I2C1->STAR2 <<16);
return (status & event_mask) == event_mask;
}
uint8_t I2C_GetFlagStatus(uint32_t I2C_FLAG)
{
   uint8_t bitstatus = 0;
   uint32_t i2creg = 0, i2cxbase = 0;

   i2cxbase = (uint32_t)I2C1;
   i2creg = I2C_FLAG >> 28;
   I2C_FLAG &= I2C_FLAG_Mask;

   if(i2creg != 0)
   {
      i2cxbase += 0x14;
   }
   else
   {
      I2C_FLAG = (uint32_t)(I2C_FLAG >> 16);
      i2cxbase += 0x18;
   }

   if(((*(uint32_t *)i2cxbase) & I2C_FLAG) != 0)
   {
      bitstatus = 1;
   }
   else
   {
      bitstatus = 0;
   }

   return bitstatus;
}

void I2C_ClearFlag(uint32_t I2C_FLAG)
{
   uint32_t flagpos = 0;

   flagpos = I2C_FLAG & I2C_FLAG_Mask;
   I2C1->STAR1 = (uint16_t)~flagpos;
}

//
// Returns 0 for timeout error
// returns 1 for success
//
int I2CRead(uint8_t u8Addr, uint8_t *pData, int iLen)
{
int iTimeout = 0;
uint16_t u16;

//    while( I2C_GetFlagStatus( I2C_FLAG_BUSY ) != 0) {};
   I2C1->CTLR1 |= I2C_CTLR1_START;
   // while( !I2C_CheckEvent( I2C_EVENT_MASTER_MODE_SELECT ) );

   I2C1->DATAR = (u8Addr << 1) | 1; // send 7-bit address, read flag = 1

   // while(/*iTimeout < TIMEOUT &&*/ !I2C_CheckEvent( I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED ) ) {
   //    iTimeout++;
   // }
   //if (iTimeout >= TIMEOUT) return 0; // error
   u16 = I2C1->STAR2; // clear the status register
   I2C1->CTLR1 |= CTLR1_ACK_Set; // enable acknowledge
   iTimeout = 0;
   while(iLen && iTimeout < TIMEOUT)
   {
//        if( I2C_GetFlagStatus( I2C_FLAG_RXNE ) !=  0 )
      if (I2C1->STAR1 & I2C_STAR1_RXNE)
//	if (I2C_GetFlagStatus( I2C_FLAG_RXNE) == 0) {
//		I2C1->CTLR1 &= CTLR1_ACK_Reset;
//		iTimeout++;
//	} else {
   {
            iTimeout = 0;
            *pData++ = (uint8_t)I2C1->DATAR;
            iLen--;
            if (iLen == 1) { // last byte
               I2C1->CTLR1 &= CTLR1_ACK_Reset; // disable acknowledge
               I2C1->CTLR1 |= I2C_CTLR1_STOP; // send stop signal
            }
      } else {
      iTimeout++;
   }
   } // while

//    I2C1->CTLR1 |= I2C_CTLR1_STOP;
   return (iLen == 0);
} /* I2CRead() */

void I2CWrite(uint8_t u8Addr, uint8_t *pData, int iLen)
{
   I2C1->CTLR1 |= I2C_CTLR1_START;
   while( !I2C_CheckEvent( I2C_EVENT_MASTER_MODE_SELECT ) );

   I2C1->DATAR = (u8Addr << 1) | 0; // write flag = 0

   while( !I2C_CheckEvent( I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED ) );

   for (int i=0; i<iLen; i++) {
      I2C1->DATAR = pData[i];
   }

   while(iLen)
   {
      if( I2C_GetFlagStatus( I2C_FLAG_TXE ) !=  0 )
      {
         I2C1->DATAR = pData[0];
         pData++;
         iLen--;
      }
   }

   // while( !I2C_CheckEvent( I2C_EVENT_MASTER_BYTE_TRANSMITTED ) );
   I2C1->CTLR1 |= I2C_CTLR1_STOP;

} /* I2CWrite() */

int I2CTest(uint8_t u8Addr)
{
   int iTimeout = 0;

   I2C_ClearFlag(I2C_FLAG_AF);
   I2C1->CTLR1 |= I2C_CTLR1_START;
   while(iTimeout < TIMEOUT && !I2C_CheckEvent( I2C_EVENT_MASTER_MODE_SELECT ) ) {
      iTimeout++;
   }
   if (iTimeout >= TIMEOUT) return 0; // no pull-ups, open bus

   I2C1->DATAR = (u8Addr << 1) | 0; // transmit direction

   while(iTimeout < TIMEOUT && !I2C_CheckEvent( I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED ) ) {
      iTimeout++;
   }
   if (iTimeout >= TIMEOUT) return 0; // no device at that address; the MTMS flag will never get set

   I2C1->CTLR1 |= I2C_CTLR1_STOP;
   // check ACK failure flag
   return (I2C_GetFlagStatus(I2C_FLAG_AF) == 0); // 0 = fail, 1 = succeed

} /* I2CTest() */

//
// Read N bytes starting at a specific I2C internal register
// returns 1 for success, 0 for error
//
void I2CReadRegister(uint8_t iAddr, uint8_t u8Register, uint8_t *pData, int iLen)
{
   I2CWrite(iAddr, &u8Register, 1);
   I2CRead(iAddr, pData, iLen);
} /* I2CReadRegister() */


// 
// 64-bit unsigned divide
// takes less flash space than the 2K used by the RISC-V math library
// 
uint64_t udiv64(uint64_t num, uint64_t den)
{
uint64_t place = 1;
uint64_t ret = 0;
   while ((num >> 1) >= den) {
      place<<=1;
      den<<=1;
   }
   for ( ; place>0; place>>=1, den>>=1) {
      if (num>=den) {
         num-=den;
         ret+=place;
      }
   }
   return ret;
} /* udiv64() */
