#include <project.h>
#include <stdlib.h>
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
bool Button1 = false, Button2 = false;
unsigned long Millis = 0, Pressed = 0;

CY_ISR(MILLIS_ISR)
{
	Millis++;
}

CY_ISR(BUTTON1_ISR)
{
	Button1 = true;
	RESET(Pressed);
}

CY_ISR(BUTTON2_ISR)
{
	Button2 = true;
	RESET(Pressed);
}

void update(unsigned char buffer[BUFFER_LENGTH][BUFFER_WIDTH],
	unsigned long *lastChange, unsigned long *lastScroll, unsigned long lastBell,
	int readRow, int *readColumn)
{
	int i, row, offset, length;
	
	length = strlen(buffer[readRow]);
	if (length > 16 && ELAPSED(*lastChange, 1000))
	{
		if (*readColumn < length && ELAPSED(*lastScroll, 100))
		{
			*readColumn = *readColumn == 0 ? 17 : *readColumn + 1;
			RESET(*lastScroll);
		}
		else if (ELAPSED(*lastScroll, 1000))
		{
			*readColumn = 0;
			RESET(*lastChange);
		}
	}
	if (ELAPSED(lastBell, 1000))
	{
		LED1_Write(0);
	}
	LCD_ClearDisplay();
	row = readRow - 1;
	if (row < 0)
	{
		row = BUFFER_LENGTH - 1;
	}
	length = MIN(16, strlen(buffer[row]));
	for (i = 0; i < length; i++)
	{
		LCD_Position(0, i);
		LCD_PutChar(buffer[row][i]);
	}
	offset = MAX(0, *readColumn - 16);
	length = MIN(16, strlen(buffer[readRow]) - offset);
	for (i = 0; i < length; i++)
	{
		LCD_Position(1, i);
		LCD_PutChar(buffer[readRow][offset + i]);
	}
}

int main()
{
	typedef enum {
		LineFeed = 2,
		CarriageReturn = 8,
		Bell = 5,
		FigureShift = 27,
		LetterShift = 31
	} ControlCodes;
	static unsigned char LettersTable[] = {
		' ', 'E', 0, 'A', ' ', 'S', 'I', 'U',
	  	0, 'D', 'R', 'J', 'N', 'F', 'C', 'K',
	  	'T', 'Z', 'L', 'W', 'H', 'Y', 'P', 'Q',
	  	'O', 'B', 'G', 0, 'M', 'X', 'V', 0
	}, FiguresTable[] = {
	 	' ', '3', 0, '-', ' ', 0, '8', '7',
	  	0, '$', '4', '\'', ',', '!', ':', '(',
	  	'5', '"', ')', '2', '#', '6', '0', '1',
	  	'9', '?', '&', 0, '.', '/', ';', 0
	};
	bool settings = false;
	double baud = 50;
	int i, writeColumn = 0, writeRow = 0, readColumn = 0, readRow = 0, row;
	unsigned long lastUpdate = 0, lastChange = 0, lastScroll = 0, lastBell = 0;
	unsigned char value, buffer[BUFFER_LENGTH][BUFFER_WIDTH];
    ControlCodes shift = LetterShift;
	
	LCD_Start();
	MillisTimer_Start();
	MillisISR_StartEx(MILLIS_ISR);
	Button1ISR_StartEx(BUTTON1_ISR);
	Button2ISR_StartEx(BUTTON2_ISR);
	CyGlobalIntEnable;
	memset(buffer, '\0', BUFFER_LENGTH * BUFFER_WIDTH);
	
    while (true)
	{
		if (Button1 && Button2)
		{
			settings = !settings;
			Button1 = Button2 = false;
		}
		if (settings)
		{
			if (ELAPSED(Pressed, 100))
			{
				if (Button1)
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
					Button1 = false;
				}
				else if (Button2)
				{
					if (baud == 75)
					{
						baud = 50;
					}
					else if (baud == 50)
					{
						baud = 45.45;
					}
					else if (baud == 45.45)
					{
						baud = 75;
					}
					Button1 = false;
				}
				LCD_ClearDisplay();
				LCD_Position(0, 0);
				LCD_PrintString("Settings");
				LCD_Position(1, 0);
				LCD_PrintString("Baud:");
				LCD_Position(1, 6);
				LCD_PrintNumber(baud);
				RESET(lastUpdate);
			}
		}
		else
		{
			while (!Button1 && !Button2)
			{
				if (InputSignal_Read())
				{
					CyDelay(1000 / baud + 1000 / baud / 2);
					if (InputSignal_Read())
					{
						break;
					}
				}
				else if (ELAPSED(lastUpdate, 100))
				{
					update(buffer, &lastChange, &lastScroll, lastBell, readRow, &readColumn);
					RESET(lastUpdate);
				}
			}
			while (!Button1 && !Button2)
			{
				if (!InputSignal_Read())
				{
					break;
				}
				else if (ELAPSED(lastUpdate, 100))
				{
					update(buffer, &lastChange, &lastScroll, lastBell, readRow, &readColumn);
					RESET(lastUpdate);
				}
			}
			if (!Button1 && !Button2)
			{
				CyDelay(1000 / baud / 2);
				value = 0;
				for (i = 0; i < 5; i++)
				{
					CyDelay(1000 / baud);
					value |= InputSignal_Read() << i;
				}
				switch(value)
				{
					case LineFeed:
						writeColumn = 0;
						writeRow++;
						if (writeRow >= BUFFER_LENGTH)
						{
							writeRow = 0;
						}
						memset(buffer[writeRow], '\0', BUFFER_WIDTH);
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
							memset(buffer[writeRow], '\0', BUFFER_WIDTH);
						}
						if (shift == LetterShift)
						{
							buffer[writeRow][writeColumn] = LettersTable[value];
						}
						else
						{
							if (value == Bell)
							{
								LED1_Write(1);
								RESET(lastBell);
								break;
							}
							buffer[writeRow][writeColumn] = FiguresTable[value];
						}
						writeColumn++;
						readColumn = writeColumn;
						readRow = writeRow;
				}
			}
			if (ELAPSED(Pressed, 100))
			{
				if (Button1)
				{
					row = readRow - 1;
					if (row < 0)
					{
						row = BUFFER_LENGTH - 1;
					}
					if (strlen(buffer[row]))
					{
						readRow = row;
					}
					readColumn = 0;
					Button1 = false;
					RESET(lastChange);
				}
				else if (Button2)
				{
					row = readRow + 1;
					if (row >= BUFFER_LENGTH)
					{
						row = 0;
					}
					if (strlen(buffer[row]))
					{
						readRow = row;
					}
					readColumn = 0;
					Button2 = false;
					RESET(lastChange);
				}
			}
			update(buffer, &lastChange, &lastScroll, lastBell, readRow, &readColumn);
			RESET(lastUpdate);
		}
    }
}