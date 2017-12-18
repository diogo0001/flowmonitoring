/* -----------------------------------------------------------------------------
 * Project:			Sensor Remoto
 * File:			remoteSensor.c
 * Module:			
 * Author:			Diogo Tavares  e José Nicolau Varela
 * Version:			1.0
 * Last edition:	17/12/2017
 * ---------------------------------------------------------------------------*/

#include "ATmega328.h"

#define ECHO1  PC0
#define TRG1   PC1
#define ECHO2  PC2
#define TRG2   PC3

uint8 sevenSeg(uint8 num);
void serialConv(uint8 byte);
void disp(int16 value, uint8 *i);

typedef union {
	struct {		
		uint8 muxDisp   : 1;		// multiplexação do display
		uint8 tx     	: 1;		// habilita transmissão
		uint8 muxTrig	: 1;		// multiplaxação das aquisições dos ultrassons
		uint8 setTrig	: 1;		// 
		uint8 calibrate	: 1;		// para calibração do valor floor (base)
		uint8 active	: 1;		// quando ambos sensores estão em 1
		uint8 side		: 2;		// sentido do movimento dir ou esq
		};
		uint8 allFlags;
}flags_t;

typedef union {
	struct {		
		uint8 ckp1		:1;		// checkpoint1 - quando sensor 1 estiver em 1
		uint8 ckp2		:1;		// checkpoint2 - quando sensor 2 estiver em 1
		uint8 unused	:6;
		};
		uint8 machine;			// maquina de estados setada pelos bits

}machine_t;

volatile machine_t control;
volatile flags_t bit;
volatile int8 counter;
volatile uint8 last_PINC;
volatile int8 dist1, dist2;				

uint8 escapeData_checkSum(uint8 *buffer, uint8 *frameData, uint8 *frameIndex);

int main(void)
{
	//definições dos pinos
	DDRB = 0b00011111;
	DDRD = 0b00111100;
	DDRC = 0b00001010;
	setBit(PCICR,PCIE1);			//habilita interrupções pelo PORTC
	PCMSK1 = 0b00000101;			//define quais os pinos do PORTC serão interrupção
	PORTC  = 0b00010000;				
	PORTB  = 0b00000111;	
	
	//seta modo CTC Timer0 para multiplexação do display, utiliza a flag: bit.muxdisp
	TCCR0A = 0x02;					
	OCR0A  = 249;					// Topo da contagem de TCNT0 para 1 ms
	TIMSK0 = 1<<OCIE0A;	
	TCCR0B = 0x03;					// Prescaler 64 para 16 MHz
	TCNT0  = 0;

	//seta modo CTC Timer1 para o contador
	TCCR1B = (1 << WGM12)|(1 << CS11);	// prescaler  8
	TIMSK1 = 1<<OCIE1A;	
	OCR1A  = 65000;					
	TCNT1  = 0;
		
	usartConfig(USART_MODE_ASYNCHRONOUS,USART_BAUD_9600,USART_DATA_BITS_8,USART_PARITY_NONE,USART_STOP_BIT_SINGLE);
	usartEnableTransmitter();
	usartStdio();
	//printf("Teste!");
		
	sei();
	
	bit.allFlags = 0;
	control.machine = 0;
	bit.muxTrig = 1;
	bit.setTrig = 1;
	last_PINC = 0xff;
	counter = 0;
	
	uint8 buffer_tx[4];
	//uint8 buffer_aux[7];  // para a implementação da função para escape da trasmissão
	uint8 i = 0;
	uint8 j = 0;
	int8 passCount = 0;		
	uint8 floor = 0;
	uint8 aux = 0;
	
    while(1)
    {	
		//-------------------  Calibração  -----------------------
    	if(bit.calibrate){
			floor = dist1;
			bit.calibrate = 0;
			
			//while(counter<50)		
				disp(floor, &i);		
				
			counter = 0;
    	}
		
		//floor = 90;
		
    	// -----------------  ultrassom  -------------------------


		if(counter>45){	// garante que a nova aquisição será maior que 
    	
			if(bit.muxTrig){				// multiplexa as aquisições  
		    	setBit(PORTC,TRG1);
		    	_delay_us(10);
				clrBit(PORTC,TRG1);
			}
			else{
				setBit(PORTC,TRG2);
		    	_delay_us(10);
				clrBit(PORTC,TRG2);
			}
			counter=0;	
			//bit.setTrig = 0;
			//bit.tx=1;
		}
		
		
		// ------------------  controle  --------------------------

		if(dist1<(floor*0.80))		// atualiza a control.machine
			control.ckp1 = 1;
		else
			control.ckp1 = 0;
		
		if(dist2<(floor*0.80))
			control.ckp2 = 1;
		else
			control.ckp2 = 0;
			
		aux = control.machine;
		
		if(control.machine==3)
			bit.active = 1;
			 
		if(aux!=3){        
			if(bit.side == 0)   
				bit.side = aux;
		}
		
		if(dist1<(floor*0.80))		// atualiza a control.machine
		control.ckp1 = 1;
		else
		control.ckp1 = 0;
			
		if(dist2<(floor*0.80))
		control.ckp2 = 1;
		else
		control.ckp2 = 0;
		
		if(control.machine == 0 && bit.active==1){ 
		
			bit.active = 0;
			
			if(bit.side == 1){
				passCount++;   
				aux = 0;
				bit.side = 0;
				bit.tx = 1;
			
			}
			else if(bit.side == 2){
				passCount--; 
				aux = 0;
				bit.side = 0;
				bit.tx = 1;
			
			}
		}
		
		//passCount = control.machine;
	
		// ----------------------  mostra e transmite  -------------------------
		
		disp(passCount, &i);
	
		if(bit.tx){

			buffer_tx[0] = 0x7E;        //  conta até 126´sem precisar do escape data 
			buffer_tx[1] = 1;			// id
			buffer_tx[2] = passCount;
			buffer_tx[3] = 1 + passCount;

			// inserir a função escape

			for(j=0;j<4;j++){
				usartTransmit(buffer_tx[j]);
			}
			bit.tx = 0;
		}
		
	} 
	  return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Interrupções
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ISR(PCINT1_vect)					 // Interrupção de maior grau de prioridade
{
	uint8  sreg = SREG;
	clrBit(PCICR,PCIE1);			// desabilita, por garantia, a interrupção do PC0
	sei();							 // habilita interrupções para não perder o estouro dos Timers 
				 
	uint8 changed_bits;
	changed_bits = PINC^last_PINC;  //seta os bits dos pinos da última mudança de estado
	last_PINC = PINC;

    if(changed_bits & (1 << PINC0)){ //detecta qual pino foi pressionado
		if(PINC & (1 << PINC0)){     //efetua a operação por borda de subida, iniciou-se um período do echo1
			TCNT1 = 0;
		}
		else{						// encerrou um periodo do echo1
			dist1 = TCNT1/116;
			//bit.setTrig = 1;
			bit.muxTrig = 0;		// habilita o proximo sensor
			//counter = 0;
		}
	}
	
    if(changed_bits & (1 << PINC2)){
		if(PINC & (1 << PINC2)){ 
			TCNT1 = 0;
		}
		else{						// encerrou um periodo do echo2
			dist2 = TCNT1/116;
			//bit.setTrig = 1;
			bit.muxTrig = 1;
			//counter = 0;		
		}
	}

	if(changed_bits & (1 << PINC4)){		// botão de calibração
		_delay_ms(10);
		if(PINC & (1 << PINC4)) 
			bit.calibrate = 1;
			//bit.setTrig = 1;
			bit.muxTrig = 0;		
			counter = 0;
	}
				
	setBit(PCICR,PCIE1);	
	SREG = sreg;
}

//--------------------------------  Timers  -------------------------------------------------
ISR(TIMER1_COMPA_vect){	
}


ISR(TIMER0_COMPA_vect){		//  Última interrupção a ser atendida, porém é a mais recorrente 
	bit.muxDisp = 1;
	counter++;				// contador de 1 ms				
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Função: disp() mostra os valores no display 7seg
/////////////////////////////////////////////////////////////////////////////////////////////

void disp(int16 value, uint8 *i){
	
	uint8 seg[4];
	
	//separa cada grandeza do número em uma posição
	seg[3] = sevenSeg(value%10);
	seg[2] = sevenSeg((value%100)/10);
	seg[1] = sevenSeg((value%1000)/100);
	seg[0] = sevenSeg((value)/1000);
	
	//multiplexação para o display
	if(bit.muxDisp){
		PORTD = 0b00111100;
		serialConv(seg[(*i)]);
		clrBit(PORTD,PD2+(*i));
		(*i)++;
		bit.muxDisp = 0;
	}
	(*i) = (*i)==4 ?0 :(*i);
	
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Função: sevenSeg()  converte o valor para bits 7seg.. específico para esta aplicação em hardware
////////////////////////////////////////////////////////////////////////////////////////////////////

uint8 sevenSeg(uint8 num)
{
	uint8 data = 0;
	switch(num){ 

		case 0x00:	data = 0b01111011;	break;
		case 0x01:	data = 0b00011000;	break;
		case 0x02:	data = 0b01010111;	break;
		case 0x03:	data = 0b01011101;	break;
		case 0x04:	data = 0b00111100;	break;
		case 0x05:	data = 0b01101101;	break;
		case 0x06:	data = 0b01101111;	break;
		case 0x07:	data = 0b01011000;	break;
		case 0x08:	data = 0b01111111;	break;
		case 0x09:	data = 0b01111101;	break; 
	}
	return data; 
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Função: serialConv()  manda os bits em série para o 74595
/////////////////////////////////////////////////////////////////////////////////////////////// 

void serialConv(uint8 byte){

	uint8 i = 0;

	setBit(PORTB,PB0);
	clrBit(PORTB,PB0);

	for(i=0;i<8;i++){
		// mascara para ver o nivel do bit
		if(byte & 0b10000000)
			setBit(PORTB,PB1);
		else
			clrBit(PORTB,PB1);
		// pulso de clock para deslocamento
		setBit(PORTB,PB0);
		clrBit(PORTB,PB0);
		byte = byte<<1;
	}
	// strobe
	setBit(PORTB,PB2);
	clrBit(PORTB,PB2);

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//  Função : escapedata
//////////////////////////////////////////////////////////////////////////////////////////////////////////

uint8 escapeData_checkSum(uint8 *buffer, uint8 *frameData, uint8 *frameIndex){

	uint8 chekSum = 0xff;
	uint8 aux8 = 0;

	for(aux8=1; aux8<8; aux8++){									// aux8 varre o buffer, frameIndex incrementa para os indices do array frameData[] para escape		
		if(frameData[aux8-1] == 0x7E || frameData[aux8-1] == 0x02){	// caracteres de escape
			buffer[*frameIndex] = 0x02;								// coloca o identificador de escape 0x02
			buffer[++(*frameIndex)] = frameData[aux8-1]^0x1D;		// byte escapado
			chekSum -= frameData[aux8+1]^0x1D;						// checksum com dado escapado
		}
		else{
			buffer[*frameIndex] = frameData[aux8-1];
			chekSum -= frameData[aux8-1];
		}
		(*frameIndex)++;
	} 
	buffer[*frameIndex] = chekSum;

	return chekSum;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////
/*

	while(1){
		
		if(dist1 == floor && control.active_f){
			control.state = 1;
		}
		
		else if(dist2 == floor && control.active_f){
			control.state = 2;
		}
		
		switch(control.state){
			
			case 0:
			break;
			case 1:
			if(control.active_f){
				passCount++;
				control.state = 0;
				control.active_f = 0;
				control.last_s = 0;
				bit.tx = 1;
			}

			break;
			case 2:
			if(control.active_f){
				passCount--;
				control.state = 0;
				control.active_f = 0;
				control.last_s = 0;
				bit.tx = 1;
			}
			
			break;
			case 3:
			control.active_f = 1;
			control.state = 0;
			control.last_s = 0;
			break;
		}
		
		if(bit.tx)
		break;
	}*/

//////////////////////////////////

/*
	while(control.state == 3){
		
		if(dist1>(floor*0.7)){
			control.state = 0;
			control.last_s = 1;
			control.active_f = 1;
			break;
		}
		if(dist2>(floor*0.7)){
			control.state = 0;
			control.last_s = 2;
			control.active_f = 1;
			break;
		}
	}
	
	if(control.active_f){
		if(control.last_s==1){
			passCount++;
			control.state = 0;
			control.last_s = 0;
			bit.tx = 1;
		}
		else if(control.last_s==2){
			passCount--;
			control.state = 0;
			control.last_s = 0;
			bit.tx = 1;
		}
		control.active_f = 0;
	}*/

////////////////////////////////////////////////////////////////////////////

/*
		//while(control.machine !=3){
		
		if(side==0 && side!=3)
		side = control.machine;
		
		if(dist1<(floor*0.80))
		control.ckp1 = 1;
		else
		control.ckp1 = 0;
		
		if(dist2<(floor*0.80))
		control.ckp2 = 1;
		else
		control.ckp2 = 0;
		
		passCount = control.machine;
		//disp(passCount, &i);
		//	}
		
		//control.machine = 0;
		
		if(side == 1){
			passCount++;
			control.machine = 0;
			side = 0;
			bit.tx = 1;
		}
		if(side == 2){
			passCount--;
			
			control.machine = 0;
			side = 0;
			bit.tx = 1;
		}
		
		*/


///////////////////////////////////////////////////////////


/*
		
		//  le sensores
		
		if(dist1<(floor*0.80))
		control.ckp1 = 1;
		else
		control.ckp1 = 0;
		
		if(dist2<(floor*0.80))
		control.ckp2 = 1;
		else
		control.ckp2 = 0;
		
		while(control.machine == 0){
			
			// le sensores esperando por movimento
			
			if(dist1<(floor*0.80))
			control.ckp1 = 1;
			else
			control.ckp1 = 0;
			
			if(dist2<(floor*0.80))
			control.ckp2 = 1;
			else
			control.ckp2 = 0;
			
		}
		
		while(control.machine==1){
			
			if(dist1<(floor*0.80))
			control.ckp1 = 1;
			else
			control.ckp1 = 0;
			
			if(dist2<(floor*0.80))
			control.ckp2 = 1;
			else
			control.ckp2 = 0;
			
			side = 1;
		}
		
		while(control.machine == 3){
			
			if(dist1<(floor*0.80))
			control.ckp1 = 1;
			else
			control.ckp1 = 0;
			
			if(dist2<(floor*0.80))
			control.ckp2 = 1;
			else
			control.ckp2 = 0;
			
			side = 3;
		}
		
		while(control.machine==2){
			
			if(dist1<(floor*0.80))
			control.ckp1 = 1;
			else
			control.ckp1 = 0;
			
			if(dist2<(floor*0.80))
			control.ckp2 = 1;
			else
			control.ckp2 = 0;
			
			side = 2;
		}
		
		
		if(control.machine==0 && side==1){
			
			
		}
		else if(control.machine==0 && side==){
			
		}
		

		
		passCount = control.machine;

		*/