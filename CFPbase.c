/* -----------------------------------------------------------------------------
 * Project:			Módulo central
 * File:			CFPbase.c
 * Module:			
 * Author:			Diogo Tavares  e José Nicolau Varela
 * Version:			1.0
 * Last edition:	17/12/2017
 * ---------------------------------------------------------------------------*/

// -----------------------------------------------------------------------------
// System definitions ----------------------------------------------------------

#define F_CPU 16000000UL
#define DRV_MMC 
// -----------------------------------------------------------------------------
// Header files ----------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>

#include "globalDefines.h"
#include "atmega328.h"
#include "diskio.h"
#include "ff.h"
#include "twimaster.h"
#include "ds1307.h"
#include "lcd4d.h"
#include <string.h>
// -----------------------------------------------------------------------------
// Project definitions ---------------------------------------------------------
   #define US_DDR   DDRD
   #define tst_bit(y,bit)   (y&(1<<bit)) 
    
// -----------------------------------------------------------------------------
// New data types --------------------------------------------------------------
typedef union systemflags_t{
	struct
	{
		uint8 adcChannel		: 2;
		uint8 dataReady			: 1;
		uint8 leftOverBits		: 5;
	};
	uint8 allFlags;
	
} systemflags_t;
// -----------------------------------------------------------------------------
// Function declaration --------------------------------------------------------

// -----------------------------------------------------------------------------
// Global variables ------------------------------------------------------------
volatile systemflags_t systemFlags;
volatile uint16 rawData[3] = {0, 0 ,0};
volatile uint16 adjust1 = 0;
volatile uint16 adjust2 = 0;

volatile unsigned char flagUS1;
volatile unsigned char flagUS2;

attachLcd(display);
 uint8 year, month, day, weekday, hours, minutes, seconds;
volatile uint8  tempo;
volatile uint8 BufferLeitura[256] = {0};
// -----------------------------------------------------------------------------
// Main function ---------------------------------------------------------------

int main(void)
{
	// Variable declaration
	
	FRESULT res;
	FATFS card;
	FIL file;
	char string[50];
	uint16 bytesWritten;
	uint8 aux8 = 0;
	uint8 bufferIn;
	uint8 pacDados[3] ;
	uint8 i = 0;
	flagUS1 = 0;
	flagUS2 = 0;
	uint8 d = 0;
	
char* daysWeek[7] = {"SEGUNDA_F","TERCA_F", "QUARTA_F","QUINTA_F","SEXTA_F","SABADO","DOMINGO"};
	
	                     // seta triger como entrada
	 setBit (DDRD,PD6);           
 //USART configuration
	usartConfig(USART_MODE_ASYNCHRONOUS, USART_BAUD_9600, USART_DATA_BITS_8, USART_PARITY_NONE, USART_STOP_BIT_SINGLE);
	usartEnableReceiver();
	usartEnableTransmitter();
	usartStdio();
		 
	// twi config
	twiMasterInit(10000);
	// Enable Global Interrupts
	sei();
	aux8 = ds1307GetRamData(18);
	// RTC config
	if (aux8!=2){
		ds1307SetControl(DS1307_COUNTING_RESUME, DS1307_CLOCK_1HZ, DS1307_FORMAT_24_HOURS);
		ds1307SetTime(1, 22, 33, DS1307_24);
		ds1307SetDate(11, 12, 13, 4);
    ds1307SetRamData(2,18);
	}

    _delay_ms(1000);
	
	// Mounting SD card
	res = f_mount(0, &card);
	if(res != FR_OK){

		printf("->card not mounted => error\r");
		}else{
		printf("->SD card successfully mounted\r");
	}
		
	ds1307GetDate(&year, &month, &day, &weekday);
	ds1307GetTime(&hours, &minutes, &seconds, &aux8);
		

	// Creating a file
	res = f_open(&file,"CFP.txt", FA_WRITE | FA_CREATE_ALWAYS);
	if(res != FR_OK){
		printf("->File not created => error = %d\r", res);
		printf("->File not created => error\r");
	}
	else{
		strcpy(string,"->File created successfully\r");
		printf("->File created successfully\r");
	}


	while(1){
		    usartAddDataToReceiverBuffer( usartReceive());
		    while(!usartIsReceiverBufferEmpty())
		    { 
			    bufferIn = usartGetDataFromReceiverBuffer();
			    if(bufferIn == 0x7e){
				        
					for (i=0;i<3;i++)
					{
						usartAddDataToReceiverBuffer( usartReceive());
						pacDados[i]= usartGetDataFromReceiverBuffer();        
					}
					        
					if ( (pacDados[0] + pacDados[1]) == (pacDados[2]))
					{          
						ds1307GetDate(&year, &month, &day, &weekday);
						ds1307GetTime(&hours, &minutes, &seconds, &aux8);
						sprintf(string,"IN = %d   DATE :  %02d/%02d/20%d %d   TIME: %02d:%02d:%02d\n\r",pacDados[1], day, month,year, adjust2, hours, minutes, seconds);
						f_write(&file, string, strlen(string), &bytesWritten);
						printf("%s",string);
						        
					}
								
					if   ((seconds ==10) || (seconds == 40) || (seconds == 0))
					{   
							sprintf(string,"%s\n\r", daysWeek[d]);
							printf("DIA DA SEMANA : %s\n\r", daysWeek[d]);
									
							f_sync(&file);
							f_close(&file);
								
							res = f_open(&file,"CFP.txt",FA_READ|FA_WRITE);
							res = f_lseek(&file, f_size(&file));
									
							f_write(&file,string, strlen(string), &bytesWritten);
									
							d++;
							
							if (d == 8){
								d=0;
							}
									  
					//	break;
					}
				}	
			}

	}
	return 0;
}

// -----------------------------------------------------------------------------
// Interruption handlers -------------------------------------------------------

USART_RECEIVER_BUFFER_FUNCTION_HANDLER

// -----------------------------------------------------------------------------
// Function definitions --------------------------------------------------------

