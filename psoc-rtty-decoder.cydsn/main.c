#include <project.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BUFFER_LENGTH 75
#define BUFFER_WIDTH 100
#define MIN(a, b) a < b ? a : b
#define MAX(a, b) a > b ? a : b
#define ELAPSED(a, b) Millis - a >= b
#define RESET(a) a = Millis;

typedef enum {
	false,
	true,
} bool;
bool Button1Pressed = false, Button2Pressed = false;
unsigned long Millis = 0, Button1Timer = 0, Button2Timer = 0;

CY_ISR(MILLIS_ISR)
{
	Millis++;
}

CY_ISR(BUTTON1_ISR)
{
	if (ELAPSED(Button1Timer, 100))	{
		Button1Pressed = true;
		RESET(Button1Timer);
	}
}

CY_ISR(BUTTON2_ISR)
{
	if (ELAPSED(Button2Timer, 100))
	{
		Button2Pressed = true;
		RESET(Button2Timer);
	}
}

void settingsUpdate(char name[], char *value)
{
	int i, length;
	
	LCD_Position(0, 0);
	LCD_PrintString("--- Settings ---");
	LCD_Position(1, 0);
	LCD_PrintString(name);
	length = strlen(name);
	LCD_Position(1, length);
	LCD_PrintString(": ");
	LCD_Position(1, length + 2);
	LCD_PrintString(value);
	for (i = length + 2 + strlen(value); i < 16; i++)
	{
		LCD_Position(1, i);
		LCD_PutChar(' ');
	}
}

void decodingUpdate(unsigned char lineBuffer[][BUFFER_WIDTH],
	unsigned long *changeTimer, unsigned long *scrollTimer,
	const unsigned long bellTimer, const int readRow, int *readColumn)
{
	int i, columnOffset, rowLength;
	
	rowLength = strlen(lineBuffer[readRow]);
	if (rowLength > 16 && ELAPSED(*changeTimer, 1000))
	{
		if (*readColumn < rowLength && ELAPSED(*scrollTimer, 100))
		{
			if (*readColumn == 0)
			{
				*readColumn = 17;
			}
			else
			{
				(*readColumn)++;
			}
			RESET(*scrollTimer);
		}
		else if (ELAPSED(*scrollTimer, 1000))
		{
			*readColumn = 0;
			RESET(*changeTimer);
		}
	}
	if (ELAPSED(bellTimer, 1000))
	{
		LED1_Write(0);
	}
	LCD_ClearDisplay();
	LCD_Position(0, 0);
	LCD_PrintString("--- Decoding ---");
	columnOffset = MAX(0, *readColumn - 16);
	rowLength = MIN(16, strlen(lineBuffer[readRow]) - columnOffset);
	for (i = 0; i < rowLength; i++)
	{
		LCD_Position(1, i);
		LCD_PutChar(lineBuffer[readRow][columnOffset + i]);
	}
}

int main()
{
	typedef enum {
		setBaud,
		setUnshift,
		decode,
	} Mode;
	typedef enum {
		LineFeed = 2,
		Space = 4,
		CarriageReturn = 8,
		Bell = 5,
		FigureShift = 27,
		LetterShift = 31
	} ControlCode;
	const unsigned char lettersTable[] = {
		' ', 'E', 0, 'A', ' ', 'S', 'I', 'U',
	  	0, 'D', 'R', 'J', 'N', 'F', 'C', 'K',
	  	'T', 'Z', 'L', 'W', 'H', 'Y', 'P', 'Q',
	  	'O', 'B', 'G', 0, 'M', 'X', 'V', 0
	}, figuresTable[] = {
	 	' ', '3', 0, '-', ' ', 0, '8', '7',
	  	0, '$', '4', '\'', ',', '!', ':', '(',
	  	'5', '"', ')', '2', '#', '6', '0', '1',
	  	'9', '?', '&', 0, '.', '/', ';', 0
	};
	Mode mode = setBaud;
	bool unshift = false;
	float baud = 45.45;
	int i, writeColumn = 0, writeRow = 0, readColumn = 0, readRow = 0, tempRow;
	unsigned long updateTimer = 0, changeTimer = 0, scrollTimer = 0, bellTimer = 0;
	unsigned char value, lineBuffer[BUFFER_LENGTH][BUFFER_WIDTH];
	char tempString[6];
    ControlCode shift = LetterShift;
	
	LCD_Start();
	MillisTimer_Start();
	MillisISR_StartEx(MILLIS_ISR);
	Button1ISR_StartEx(BUTTON1_ISR);
	Button2ISR_StartEx(BUTTON2_ISR);
	CyGlobalIntEnable;
	memset(lineBuffer, '\0', BUFFER_LENGTH * BUFFER_WIDTH);
	
    while (true)
	{
		if (Button1Pressed)
		{
			switch (mode)
			{
				case setBaud:
					mode = setUnshift;
					break;
				case setUnshift:
					mode = decode;
					break;
				case decode:
					mode = setBaud;
			}
			Button1Pressed = false;
		}
		switch (mode)
		{
			case setBaud:
				if (Button2Pressed)
				{
					if (baud == 45.45)
					{
						baud = 50;
					}
					else if (baud == 50)
					{
						baud = 75;
					}
					else if (baud == 75)
					{
						baud = 45.45;
					}
					Button2Pressed = false;
				}
				if (ELAPSED(updateTimer, 100))
				{
					sprintf(tempString, "%2.2f", baud);
					settingsUpdate("Baud", tempString);
					RESET(updateTimer);
				}
				break;
			case setUnshift:
				if (Button2Pressed)
				{
					unshift = !unshift;
					Button2Pressed = false;
				}
				if (ELAPSED(updateTimer, 100))
				{
					settingsUpdate("Unshift", unshift ? "yes" : "no");
					RESET(updateTimer);
				}
				break;
			case decode:
				while (!Button1Pressed && !Button2Pressed)
				{
					if (!Signal1_Read())
					{
						CyDelay(1500 / baud);
						if (!Signal1_Read())
						{
							break;
						}
					}
					else if (ELAPSED(updateTimer, 100))
					{
						decodingUpdate(lineBuffer, &changeTimer, &scrollTimer, bellTimer, readRow, &readColumn);
						RESET(updateTimer);
					}
				}
				while (!Button1Pressed && !Button2Pressed)
				{
					if (Signal1_Read())
					{
						break;
					}
					else if (ELAPSED(updateTimer, 100))
					{
						decodingUpdate(lineBuffer, &changeTimer, &scrollTimer, bellTimer, readRow, &readColumn);
						RESET(updateTimer);
					}
				}
				if (!Button1Pressed && !Button2Pressed)
				{
					CyDelay(500 / baud);
					value = 0;
					for (i = 0; i < 5; i++)
					{
						CyDelay(1000 / baud);
						value |= !Signal1_Read() << i;
					}
					switch(value)
					{
						case LineFeed:
							writeRow++;
							if (writeRow >= BUFFER_LENGTH)
							{
								writeRow = 0;
							}
							memset(lineBuffer[writeRow], '\0', BUFFER_WIDTH);
							break;
						case CarriageReturn:
							writeColumn = 0;
							break;
						case LetterShift:
							shift = LetterShift;
							break;
						case FigureShift:
							shift = FigureShift;
							break;
						default:
							if (writeColumn >= BUFFER_WIDTH - 1)
							{
								writeColumn = 0;
								writeRow++;
								if (writeRow >= BUFFER_LENGTH)
								{
									writeRow = 0;
								}
								memset(lineBuffer[writeRow], '\0', BUFFER_WIDTH);
							}
							if (shift == LetterShift)
							{
								lineBuffer[writeRow][writeColumn] = lettersTable[value];
							}
							else if (shift == FigureShift)
							{
								if (value == Bell)
								{
									LED1_Write(1);
									RESET(bellTimer);
									break;
								}
								lineBuffer[writeRow][writeColumn] = figuresTable[value];
							}
							if (unshift && value == Space)
							{
								shift = LetterShift;
							}
							writeColumn++;
							readColumn = writeColumn;
							readRow = writeRow;
					}
				}
				if (Button2Pressed)
				{
					tempRow = readRow - 1;
					if (tempRow < 0)
					{
						tempRow = BUFFER_LENGTH - 1;
					}
					if (strlen(lineBuffer[tempRow]))
					{
						readRow = tempRow;
					}
					else
					{
						readRow = writeRow;
					}
					readColumn = 0;
					Button2Pressed = false;
					RESET(changeTimer);
				}
				decodingUpdate(lineBuffer, &changeTimer, &scrollTimer, bellTimer, readRow, &readColumn);
				RESET(updateTimer);
		}
    }
}