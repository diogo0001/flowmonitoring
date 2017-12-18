/* -----------------------------------------------------------------------------
 * Project:			GPDSE AVR8 Library
 * File:			lcd4d.c
 * Module:			Character LCD controller
 * Author:			Leandro Schwarz
 * Version:			1.1
 * Last edition:	2016-01-15
 * Purpose:			Character LCD controller with 4 bits interface without
 *					support to busy flag (delay-driven)
 * -------------------------------------------------------------------------- */

// -----------------------------------------------------------------------------
// Header files ----------------------------------------------------------------

#include "lcd4d.h"
#if __LCD4D_H != 11
	#error Error 101 - Version mismatch on header and source code files (lcd4d).
#endif

// -----------------------------------------------------------------------------
// Global variables ------------------------------------------------------------

FILE lcdStream = FDEV_SETUP_STREAM(lcdWriteStd, NULL, _FDEV_SETUP_WRITE);
volatile lcdConfiguration_t * defaultDisplay = NULL;

// -----------------------------------------------------------------------------
// Static function declarations ------------------------------------------------


// -----------------------------------------------------------------------------
// Public function definitions -------------------------------------------------

/* -----------------------------------------------------------------------------
 * Function:	lcdResetConfiguration
 * Purpose:		Resets the LCD configuration struct
 * Arguments:	lcd			Pointer to the LCD struct
 * Returns:		LCD_OK
 * -------------------------------------------------------------------------- */

lcdResult_t lcdResetConfiguration(volatile lcdConfiguration_t * lcd)
{
	lcd->dataDDR = NULL;
	lcd->dataPORT = NULL;
	lcd->dataFirst = 0;
	lcd->controlDDR = NULL;
	lcd->controlPORT = NULL;
	lcd->controlE = 0;
	lcd->controlRS = 0;
	lcd->displayOn = FALSE;
	lcd->cursorOn = FALSE;
	lcd->cursorBlink = FALSE;
	lcd->dataPortConfigured = FALSE;
	lcd->controlPortConfigured = FALSE;
	lcd->initialized = FALSE;
	lcd->size = LCD_SIZE_UNDEFINED;
	lcd->cursorLine = 0;
	lcd->cursorColumn = 0;
	lcd->functionFontSize = LCD_FONT_5X8;
	lcd->entryIncDec = LCD_INCREMENT;
	lcd->entryShiftDisplay = LCD_DISPLAY_OVERWRITE;

	return LCD_OK;
}

/* -----------------------------------------------------------------------------
 * Function:	lcdSetDataPort
 * Purpose:		Configures the LCD I/O ports for the data bus
 * Arguments:	lcd			Pointer to the LCD struct
 *				dataDDR		Pointer to I/O Data Direction Register
 *				dataPORT	Pointer to I/O Port Output Register
 *				dataFirst	Position of the first bit of the LCD data nibble
 * Returns:		LCD_OK
 * -------------------------------------------------------------------------- */

lcdResult_t lcdSetDataPort(volatile lcdConfiguration_t * lcd, volatile uint8 * dataDDR, volatile uint8 * dataPORT, uint8 dataFirst)
{
	lcd->dataDDR = dataDDR;
	lcd->dataPORT = dataPORT;
	lcd->dataFirst = dataFirst;
	lcd->dataPortConfigured = TRUE;

	return LCD_OK;
}

/* -----------------------------------------------------------------------------
 * Function:	lcdSetControlPort
 * Purpose:		Configures the LCD I/O ports for the control bus
 * Arguments:	lcd			Pointer to the LCD struct
 *				controlDDR	Pointer to I/O Data Direction Register
 *				controlPORT	Pointer to I/O Port Output Register
 *				controlE	Position of pin E at the control register
 *				controlRS	Position of pin RS at the control register
 * Returns:		LCD_OK
 * -------------------------------------------------------------------------- */

lcdResult_t lcdSetControlPort(volatile lcdConfiguration_t * lcd, volatile uint8 * controlDDR, volatile uint8 * controlPORT, uint8 controlE, uint8 controlRS)
{
	lcd->controlDDR = controlDDR;
	lcd->controlPORT = controlPORT;
	lcd->controlE = controlE;
	lcd->controlRS = controlRS;
	lcd->controlPortConfigured = TRUE;

	return LCD_OK;
}

/* -----------------------------------------------------------------------------
 * Function:	lcdInit
 * Purpose:		Performs the LCD initialization routine
 * Arguments:	lcd			Pointer to the LCD struct
 *				size		Size of the display (use enumerations at lcdSize_t)
 *				font		Font size (use enumerations at lcdFont_t)
 * Returns:		LCD_UNSUPPORTED_FONT_SIZE
 *				LCD_UNSUPPORTED_DISPLAY_SIZE
 *				LCD_DATA_PORT_NOT_CONFIGURED
 *				LCD_CONTROL_PORT_NOT_CONFIGURED
 *				LCD_OK
 * -------------------------------------------------------------------------- */

lcdResult_t lcdInit(volatile lcdConfiguration_t * lcd, lcdSize_t size, lcdFont_t font)
{
	uint8 command = 0;
	uint8 lines = 0;

	// LCD font
	switch(font){
		case LCD_FONT_5X8:	lcd->functionFontSize = LCD_FONT_5X8;	break;
		case LCD_FONT_5X10:	lcd->functionFontSize = LCD_FONT_5X10;	break;
		default:			return LCD_UNSUPPORTED_FONT_SIZE;
	}

	// LCD size
	switch(size){
	case LCD_8X1:
	case LCD_16X1b:
	case LCD_20X1:
	case LCD_24X1:
	case LCD_40X1:
		lines = 1;
		lcd->size = size;
		break;
	case LCD_8X2:
	case LCD_10x2:
	case LCD_12X2:
	case LCD_16X1:
	case LCD_16X2:
	case LCD_20X2:
	case LCD_24X2:
	case LCD_30X2:
	case LCD_32X2:
	case LCD_40X2:
		lines = 2;
		lcd->size = size;
		break;
	case LCD_12X4:
	case LCD_16X4:
	case LCD_20X4:
		lines = 4;
		lcd->size = size;
		break;
	default:
		return LCD_UNSUPPORTED_DISPLAY_SIZE;
	}

	// I/O initialization
	if(lcd->dataPortConfigured == FALSE)
		return LCD_DATA_PORT_NOT_CONFIGURED;
	if(lcd->controlPortConfigured == FALSE)
		return LCD_CONTROL_PORT_NOT_CONFIGURED;
	clrMask(*(lcd->dataPORT), 0x0F, lcd->dataFirst);
	setMask(*(lcd->dataDDR), 0x0F, lcd->dataFirst);
	clrBit(*(lcd->controlPORT), lcd->controlE);
	setBit(*(lcd->controlDDR), lcd->controlE);
	clrBit(*(lcd->controlPORT), lcd->controlRS);
	setBit(*(lcd->controlDDR), lcd->controlRS);

	// Reset the controller, must be sent 3 times
	_delay_ms(15);
	lcdFunctionSet8Bits(lcd, LCD_FUNCTION_RESET);
	_delay_ms(5);
	lcdFunctionSet8Bits(lcd, LCD_FUNCTION_RESET);
	_delay_us(60);
	lcdFunctionSet8Bits(lcd, LCD_FUNCTION_RESET);
	_delay_us(60);

	// Configures the LCD to 4 bits interface
	lcdFunctionSet8Bits(lcd, LCD_FUNCTION_SET | LCD_FUNCTION_4_BITS);

	// Function set
	command = LCD_FUNCTION_SET | LCD_FUNCTION_4_BITS;
	if(lcd->functionFontSize == LCD_FONT_5X8)
		command |= LCD_FUNCTION_5x8_FONT;
	if(lcd->functionFontSize == LCD_FONT_5X10)
		command |= LCD_FUNCTION_5x10_FONT;
	if(lines == 1)
		command |= LCD_FUNCTION_1_LINE;
	if((lines == 2) || (lines == 4))
		command |= LCD_FUNCTION_2_LINES;
	lcdWriteCommand(lcd, command);

	// Turns display off
	lcdWriteCommand(lcd, LCD_DISPLAY_OFF);

	// Clears the screen
	lcdWriteCommand(lcd, LCD_CLEAR_SCREEN);

	// Entry set
	command = LCD_ENTRY_SET;
	if(lcd->entryIncDec == LCD_INCREMENT)
		command |= LCD_ENTRY_INCREMENT;
	else
		command |= LCD_ENTRY_DECREMENT;
	if(lcd->entryShiftDisplay == LCD_DISPLAY_SHIFT)
		command |= LCD_ENTRY_SHIFT;
	else
		command |= LCD_ENTRY_OVERWRITE;
	lcdWriteCommand(lcd, command);

	// Turns display on
	lcdWriteCommand(lcd, LCD_DISPLAY_ON);

	// Updates the struct variables
	lcd->initialized = TRUE;
	lcd->displayOn = TRUE;
	lcd->cursorOn = FALSE;
	lcd->cursorBlink = FALSE;
	lcd->cursorLine = 0;
	lcd->cursorColumn = 0;

	return LCD_OK;
}

/* -----------------------------------------------------------------------------
 * Function:	lcdClearScreen
 * Purpose:		Clears the entire screen and moves cursor to home position
 * Arguments:	lcd			Pointer to the LCD struct
 * Returns:		LCD_NOT_INITIALIZED
 *				LCD_OK
 * -------------------------------------------------------------------------- */

lcdResult_t lcdClearScreen(volatile lcdConfiguration_t * lcd)
{
	if(lcd->initialized == FALSE)
		return LCD_NOT_INITIALIZED;

	return lcdWriteCommand(lcd, LCD_CLEAR_SCREEN);
}

/* -----------------------------------------------------------------------------
 * Function:	lcdCursorHome
 * Purpose:		Moves cursor to home position
 * Arguments:	lcd			Pointer to the LCD struct
 * Returns:		LCD_NOT_INITIALIZED
 *				LCD_OK
 * -------------------------------------------------------------------------- */

lcdResult_t lcdCursorHome(volatile lcdConfiguration_t * lcd)
{
	if(lcd->initialized == FALSE)
		return LCD_NOT_INITIALIZED;

	return lcdWriteCommand(lcd, LCD_CURSOR_HOME);
}

/* -----------------------------------------------------------------------------
 * Function:	lcdDisplay
 * Purpose:		Turns display ON/OFF, without changing cursor or blink option
 * Arguments:	lcd			Pointer to the LCD struct
 *				state		ON or OFF (enumerations defined at logic_t)
 * Returns:		LCD_NOT_INITIALIZED
 *				LCD_OK
 * -------------------------------------------------------------------------- */

lcdResult_t lcdDisplay(volatile lcdConfiguration_t * lcd, logic_t state)
{
	uint8 command = LCD_DISPLAY_OFF;

	if(lcd->initialized == FALSE)
		return LCD_NOT_INITIALIZED;

	lcd->displayOn = (state & 0x01);
	if(lcd->displayOn)
		command |= 4;
	if(lcd->cursorOn)
		command |= 2;
	if(lcd->cursorBlink)
		command |= 1;
	return lcdWriteCommand(lcd, command);
}

/* -----------------------------------------------------------------------------
 * Function:	lcdCursor
 * Purpose:		Turns cursor ON/OFF, without changing display or blink option
 * Arguments:	lcd			Pointer to the LCD struct
 *				state		ON or OFF (enumerations defined at logic_t)
 * Returns:		LCD_NOT_INITIALIZED
 *				LCD_OK
 * -------------------------------------------------------------------------- */

lcdResult_t lcdCursor(volatile lcdConfiguration_t * lcd, logic_t state)
{
	uint8 command = LCD_DISPLAY_OFF;

	if(lcd->initialized == FALSE)
		return LCD_NOT_INITIALIZED;

	lcd->cursorOn = (state & 0x01);
	if(lcd->displayOn)
		command |= 4;
	if(lcd->cursorOn)
		command |= 2;
	if(lcd->cursorBlink)
		command |= 1;
	return lcdWriteCommand(lcd, command);
}

/* -----------------------------------------------------------------------------
 * Function:	lcdCursorBlink
 * Purpose:		Turns cursor blink ON/OFF, without changing display or cursor
 *				option
 * Arguments:	lcd			Pointer to the LCD struct
 *				state		ON or OFF (enumerations defined at logic_t)
 * Returns:		LCD_NOT_INITIALIZED
 *				LCD_OK
 * -------------------------------------------------------------------------- */

lcdResult_t lcdCursorBlink(volatile lcdConfiguration_t * lcd, logic_t state)
{
	uint8 command = LCD_DISPLAY_OFF;

	if(lcd->initialized == FALSE)
		return LCD_NOT_INITIALIZED;

	lcd->cursorBlink = (state & 0x01);
	if(lcd->displayOn)
		command |= 4;
	if(lcd->cursorOn)
		command |= 2;
	if(lcd->cursorBlink)
		command |= 1;
	return lcdWriteCommand(lcd, command);
}

/* -----------------------------------------------------------------------------
 * Function:	lcdSetEntryMode
 * Purpose:		Configures how the display will behave when a character is
 *				written
 * Arguments:	lcd			Pointer to the LCD struct
 *				dir			write direction (enumerations defined at
 *							lcdIncrementDecrement_t)
 *				mode		insert/overwrite (enumerations defined at
 *							lcdShiftOverwrite_t)
 * Returns:		LCD_NOT_INITIALIZED
 *				LCD_OK
 * -------------------------------------------------------------------------- */

lcdResult_t lcdSetEntryMode(volatile lcdConfiguration_t * lcd, lcdIncrementDecrement_t dir, lcdShiftOverwrite_t mode)
{
	uint8 command = LCD_ENTRY_SET;

	if(lcd->initialized == FALSE)
		return LCD_NOT_INITIALIZED;

	if(dir != LCD_INCDEC_NO_CHANGE)
		lcd->entryIncDec = (dir & 0x01);
	if(mode != LCD_DISPLAY_NO_CHANGE)
		lcd->entryShiftDisplay = (mode & 0x01);

	if(lcd->entryIncDec == LCD_INCREMENT)
		command |= LCD_ENTRY_INCREMENT;
	else
		command |= LCD_ENTRY_DECREMENT;
	if(lcd->entryShiftDisplay == LCD_DISPLAY_SHIFT)
		command |= LCD_ENTRY_SHIFT;
	else
		command |= LCD_ENTRY_OVERWRITE;
	return lcdWriteCommand(lcd, command);
}

/* -----------------------------------------------------------------------------
 * Function:	lcdCursorGoTo
 * Purpose:		Moves the cursor to position
 * Note:		Origin is considered as position (1, 1)
 * Arguments:	lcd			Pointer to the LCD struct
 *				line		line
 *				column		column
 * Returns:		LCD_NOT_INITIALIZED
 *				LCD_UNSUPPORTED_DISPLAY_SIZE
 *				LCD_UNSUPPORTED_PARAMETER
 *				LCD_OK
 * -------------------------------------------------------------------------- */

lcdResult_t lcdCursorGoTo(volatile lcdConfiguration_t * lcd, uint8 line, uint8 column)
{
	uint8 address = 0x00;

	if(lcd->initialized == FALSE)
		return LCD_NOT_INITIALIZED;

	if(line == 1){
		switch(lcd->size){
		case LCD_8X1:
		case LCD_16X1:
		case LCD_16X1b:
		case LCD_20X1:
		case LCD_24X1:
		case LCD_40X1:
		case LCD_8X2:
		case LCD_10x2:
		case LCD_12X2:
		case LCD_16X2:
		case LCD_20X2:
		case LCD_24X2:
		case LCD_30X2:
		case LCD_32X2:
		case LCD_40X2:
		case LCD_12X4:
		case LCD_16X4:
		case LCD_20X4:
			address += (column - 1);
			break;
		default:
			return LCD_UNSUPPORTED_DISPLAY_SIZE;
		}
	}else if(line == 2){
		switch(lcd->size){
		case LCD_8X1:
		case LCD_16X1:
		case LCD_16X1b:
		case LCD_20X1:
		case LCD_24X1:
		case LCD_40X1:
			return LCD_UNSUPPORTED_PARAMETER;
		case LCD_8X2:
		case LCD_10x2:
		case LCD_12X2:
		case LCD_16X2:
		case LCD_20X2:
		case LCD_24X2:
		case LCD_30X2:
		case LCD_32X2:
		case LCD_40X2:
		case LCD_12X4:
		case LCD_16X4:
		case LCD_20X4:
			address += (0x40 + column - 1);
			break;
		default:
			return LCD_UNSUPPORTED_DISPLAY_SIZE;
		}
	}else if(line == 3){
		switch(lcd->size){
		case LCD_8X1:
		case LCD_16X1:
		case LCD_16X1b:
		case LCD_20X1:
		case LCD_24X1:
		case LCD_40X1:
		case LCD_8X2:
		case LCD_10x2:
		case LCD_12X2:
		case LCD_16X2:
		case LCD_20X2:
		case LCD_24X2:
		case LCD_30X2:
		case LCD_32X2:
		case LCD_40X2:
			return LCD_UNSUPPORTED_PARAMETER;
		case LCD_12X4:
			address += (0x0C + column - 1);
			break;
		case LCD_16X4:
			address += (0x10 + column - 1);
			break;
		case LCD_20X4:
			address += (0x14 + column - 1);
			break;
		default:
			return LCD_UNSUPPORTED_DISPLAY_SIZE;
		}
	}else{
		switch(lcd->size){
		case LCD_8X1:
		case LCD_16X1:
		case LCD_16X1b:
		case LCD_20X1:
		case LCD_24X1:
		case LCD_40X1:
		case LCD_8X2:
		case LCD_10x2:
		case LCD_12X2:
		case LCD_16X2:
		case LCD_20X2:
		case LCD_24X2:
		case LCD_30X2:
		case LCD_32X2:
		case LCD_40X2:
			return LCD_UNSUPPORTED_PARAMETER;
		case LCD_12X4:
			address += (0x4C + column - 1);
			break;
		case LCD_16X4:
			address += (0x50 + column - 1);
			break;
		case LCD_20X4:
			address += (0x54 + column - 1);
			break;
		default:
			return LCD_UNSUPPORTED_DISPLAY_SIZE;
		}
	}

	lcd->cursorLine = line - 1;
	lcd->cursorColumn = column - 1;

	return lcdWriteCommand(lcd, (LCD_DDRAM_ADRESS | address));
}

/* -----------------------------------------------------------------------------
 * Function:	lcdStdio
 * Purpose:		Associate the LCD to the default output for printf function
 * Arguments:	lcd			Pointer to the LCD struct
 * Returns:		LCD_NOT_INITIALIZED
 *				LCD_OK
 * -------------------------------------------------------------------------- */

lcdResult_t lcdStdio(volatile lcdConfiguration_t * lcd)
{
	if(lcd->initialized == FALSE)
		return LCD_NOT_INITIALIZED;

	stdin = stdout = stderr = &lcdStream;
	defaultDisplay = lcd;

	return LCD_OK;
}

/* -----------------------------------------------------------------------------
 * Function:	lcdCursorMoveNextLine
 * Purpose:		Moves cursor to the beginning of the next line
 * Note:		The cursor will wrap automatically from the last line to the
 *				first line
 * Arguments:	lcd			Pointer to the LCD struct
 * Returns:		LCD_NOT_INITIALIZED
 *				LCD_UNSUPPORTED_DISPLAY_SIZE
 *				LCD_UNSUPPORTED_PARAMETER
 *				LCD_OK
 * -------------------------------------------------------------------------- */

lcdResult_t lcdCursorMoveNextLine(volatile lcdConfiguration_t * lcd)
{
	uint8 line = 0;

	if(lcd->initialized == FALSE)
		return LCD_NOT_INITIALIZED;

	switch(lcd->size){
	case LCD_8X1:
	case LCD_16X1:
	case LCD_16X1b:
	case LCD_20X1:
	case LCD_24X1:
	case LCD_40X1:
		return lcdCursorGoTo(lcd, (line + 1), 1);
	case LCD_8X2:
	case LCD_10x2:
	case LCD_12X2:
	case LCD_16X2:
	case LCD_20X2:
	case LCD_24X2:
	case LCD_30X2:
	case LCD_32X2:
	case LCD_40X2:
		line = (lcd->cursorLine == 1)?0:1;
		return lcdCursorGoTo(lcd, (line + 1), 1);
	case LCD_12X4:
	case LCD_16X4:
	case LCD_20X4:
		line = (lcd->cursorLine == 3)?0:(lcd->cursorLine + 1);
		return lcdCursorGoTo(lcd, (line + 1), 1);
	default:
		return LCD_UNSUPPORTED_DISPLAY_SIZE;
	}
}

/* -----------------------------------------------------------------------------
 * Function:	lcdCursorMoveFirstLine
 * Purpose:		Moves cursor to the beginning of the first line
 * Note:		The function just calls lcdCursorHome
 * Arguments:	lcd			Pointer to the LCD struct
 * Returns:		LCD_NOT_INITIALIZED
 *				LCD_OK
 * -------------------------------------------------------------------------- */

lcdResult_t lcdCursorMoveFirstLine(volatile lcdConfiguration_t * lcd)
{
	if(lcd->initialized == FALSE)
		return LCD_NOT_INITIALIZED;

	return lcdCursorHome(lcd);
}

/* -----------------------------------------------------------------------------
 * Function:	lcdCursorMove
 * Purpose:		Moves the cursor one position to the LEFT or RIGHT
 * Arguments:	lcd			Pointer to the LCD struct
 *				dir			LEFT or RIGHT (use enumerations defined at
 *							direction_t)
 * Returns:		LCD_NOT_INITIALIZED
 *				LCD_OK
 * -------------------------------------------------------------------------- */

lcdResult_t lcdCursorMove(volatile lcdConfiguration_t * lcd, direction_t dir)
{
	if(lcd->initialized == FALSE)
		return LCD_NOT_INITIALIZED;

	if(dir == LEFT){
		lcd->cursorColumn--;
		return lcdWriteCommand(lcd, 0x10);
	}
	lcd->cursorColumn++;
	return lcdWriteCommand(lcd, 0x14);
}

/* -----------------------------------------------------------------------------
 * Function:	lcdDisplayShift
 * Purpose:		Moves the entire display one position to the LEFT or RIGHT
 * Note:		The cursor is also moved in the opposite direction. The use of
 *				this function may cause problems if used before printf() and
 *				lcdCursorGoTo(). The effects of lcdDisplayShift() are cancelled
 *				by the lcdCursorHome() and lcdCursorMoveFirstLine() functions.
 * Arguments:	lcd			Pointer to the LCD struct
 *				dir			direction to move (use enumerations defined at
 *							direction_t)
 * Returns:		LCD_NOT_INITIALIZED
 *				LCD_OK
 * -------------------------------------------------------------------------- */

lcdResult_t lcdDisplayShift(volatile lcdConfiguration_t * lcd, direction_t dir)
{
	if(lcd->initialized == FALSE)
		return LCD_NOT_INITIALIZED;

	if(dir == LEFT){
		lcd->cursorColumn--;
		return lcdWriteCommand(lcd, 0x18);
	}
	lcd->cursorColumn++;
	return lcdWriteCommand(lcd, 0x1C);
}

// -----------------------------------------------------------------------------
// Private function definitions ------------------------------------------------

/* -----------------------------------------------------------------------------
 * Function:	lcdWriteCommand
 * Purpose:		Sends a command to the LCD
 * Arguments:	lcd			Pointer to the LCD struct
 *				command		Command to be sent to LCD
 * Returns:		LCD_OK
 * -------------------------------------------------------------------------- */

lcdResult_t lcdWriteCommand(volatile lcdConfiguration_t * lcd, uint8 command)
{
	clrBit(*(lcd->controlPORT), lcd->controlRS);		// LCD in command mode
	clrBit(*(lcd->controlPORT), lcd->controlE);			// Makes sure enable is LOW
	clrMask(*(lcd->dataPORT), 0x0F, lcd->dataFirst);	// Writes data (higher nibble)
	*(lcd->dataPORT) |= ((command >> 4) << lcd->dataFirst);
	setBit(*(lcd->controlPORT), lcd->controlE);			// Enable pulse start
	_delay_us(1);
	clrBit(*(lcd->controlPORT), lcd->controlE);			// Enable pulse end
	_delay_us(1);
	clrMask(*(lcd->dataPORT), 0x0F, lcd->dataFirst);	// Writes data (lower nibble)
	*(lcd->dataPORT) |= ((command & 0x0F) << lcd->dataFirst);
	setBit(*(lcd->controlPORT), lcd->controlE);			// Enable pulse start
	_delay_us(1);
	clrBit(*(lcd->controlPORT), lcd->controlE);			// Enable pulse end
	_delay_us(1);
	if((command == LCD_CLEAR_SCREEN) || (command == LCD_CURSOR_HOME)){
		_delay_ms(2);
		lcd->cursorLine = 0;
		lcd->cursorColumn = 0;
	}else
		_delay_us(40);

	return LCD_OK;
}

/* -----------------------------------------------------------------------------
 * Function:	lcdFunctionSet8Bits
 * Purpose:		Sends a command to the LCD in 8 bits data interface
 * Arguments:	lcd			Pointer to the LCD struct
 *				command		Command to be sent to LCD
 * Returns:		LCD_OK
 * -------------------------------------------------------------------------- */

lcdResult_t lcdFunctionSet8Bits(volatile lcdConfiguration_t * lcd, uint8 command)
{
	clrBit(*(lcd->controlPORT), lcd->controlRS);		// LCD in command mode
	clrBit(*(lcd->controlPORT), lcd->controlE);			// Makes sure enable is LOW
	clrMask(*(lcd->dataPORT), 0x0F, lcd->dataFirst);	// Writes data
	*(lcd->dataPORT) |= ((command >> 4) << lcd->dataFirst);
	setBit(*(lcd->controlPORT), lcd->controlE);			// Enable pulse start
	_delay_us(1);
	clrBit(*(lcd->controlPORT), lcd->controlE);			// Enable pulse end
	_delay_us(40);

	return LCD_OK;
}

/* -----------------------------------------------------------------------------
 * Function:	lcdWriteCharacter
 * Purpose:		Sends a character to be written at the LCD
 * Arguments:	lcd			Pointer to the LCD struct
 *				character	Character to be sent to LCD
 * Returns:		LCD_NOT_INITIALIZED
 *				LCD_OK
 * -------------------------------------------------------------------------- */

lcdResult_t lcdWriteCharacter(volatile lcdConfiguration_t * lcd, uint8 character)
{
	if(lcd->initialized == FALSE)
		return LCD_NOT_INITIALIZED;

	if(lcd->cursorColumn < 40){
		setBit(*(lcd->controlPORT), lcd->controlRS);		// LCD in command mode
		clrBit(*(lcd->controlPORT), lcd->controlE);			// Makes sure enable is LOW
		clrMask(*(lcd->dataPORT), 0x0F, lcd->dataFirst);	// Writes data (higher nibble)
		*(lcd->dataPORT) |= ((character >> 4) << lcd->dataFirst);
		setBit(*(lcd->controlPORT), lcd->controlE);			// Enable pulse start
		_delay_us(1);
		clrBit(*(lcd->controlPORT), lcd->controlE);			// Enable pulse end
		_delay_us(1);
		clrMask(*(lcd->dataPORT), 0x0F, lcd->dataFirst);	// Writes data (lower nibble)
		*(lcd->dataPORT) |= ((character & 0x0F) << lcd->dataFirst);
		setBit(*(lcd->controlPORT), lcd->controlE);			// Enable pulse start
		_delay_us(1);
		clrBit(*(lcd->controlPORT), lcd->controlE);			// Enable pulse end
		_delay_us(40);
		lcd->cursorColumn++;
	}

	return LCD_OK;
}

/* -----------------------------------------------------------------------------
 * Function:	lcdWriteStd
 * Purpose:		This function is called by the printf() function to write a
 *				character at the display pointed by defaultDisplay.
 * Arguments:	character	Character to be sent to LCD
 *				stream		Pointer to a FILE stream
 * Returns:		0 (This function always returns 0)
 * Note:		This function must not be used outside this file
 * -------------------------------------------------------------------------- */

int16 lcdWriteStd(int8 character, FILE * stream)
{
	uint8 columns = 0;
	uint8 i = 0;

	if(defaultDisplay->initialized == FALSE)
		return FALSE;

	switch(defaultDisplay->size){
	case LCD_8X1:
	case LCD_8X2:
		columns = 8;
		break;
	case LCD_10x2:
		columns = 10;
		break;
	case LCD_12X2:
	case LCD_12X4:
		columns = 12;
		break;
	case LCD_16X1:
	case LCD_16X1b:
	case LCD_16X2:
	case LCD_16X4:
		columns = 16;
		break;
	case LCD_20X1:
	case LCD_20X2:
	case LCD_20X4:
		columns = 20;
		break;
	case LCD_24X1:
	case LCD_24X2:
		columns = 24;
		break;
	case LCD_30X2:
		columns = 30;
		break;
	case LCD_32X2:
		columns = 32;
		break;
	case LCD_40X1:
	case LCD_40X2:
		columns = 40;
		break;
	}

	if(character == '\n'){
		for(i = defaultDisplay->cursorColumn;i < columns;i++)
			lcdWriteCharacter(defaultDisplay, ' ');
			lcdCursorMoveNextLine(defaultDisplay);
	}else{
		lcdWriteCharacter(defaultDisplay, character);
	}

	return 0;
}