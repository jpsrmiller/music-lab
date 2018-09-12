// ***************************************************************************************
// ArduinoMusicLab.ino - Program originally written for a Build Your Music Arduino Lab
//     at Science Museum Oklahoma Tinkerfest 2018.  The intent of this lab is that
//     visitors can assemble a pre-programmed Arduino to a 16x2 I2C Character LCD and 
//     KY-040 Rotary Encoder and then connect it to an 8-note diatonic C-to-C xylophone  
//     (with solenoids and motor driver shield) to play a few simple songs.
//     Songs are hard-coded into the program via PROGMEM.
// Parts list and instructions for building and doing this Lab are at:
//      https://buildmusic.net/tutorials/music-lab/
//
//    Author:         John Miller
//    Revision:       1.0.0
//    Date:           9/1/2018
//    Project Source: https://github.com/jpsrmiller/music-lab
// 
// The program may be freely copied, modified, and used for any Arduino projects
//
// The KY-040 Rotary Encoder is connected to the following Arduino Uno pins
//      o CLK --> Pin 2
//      o DT  --> Pin 3
//      o SW  --> Pin 4
//      o +   --> Pin 5
//      o GND --> Pin 6
// 
// The LCD is connected to the following Arduino Uno pins
//      o GND --> GND
//      o VCC --> 5V
//      o SDA --> Pin A4 (SDA)
//      o SCL --> Pin A5 (SCL)
//
// The Rotary Encoder uses Arduino pins for the GND and +5V so that the remaining
//   5V and GND pins on the Arduino Uno can be used for other peripheral devices
// This works because the the Rotary Encoder draws less than the 40 mA
//   maximum current allowed on the Arduino Uno I/O pins  
//
// The Master Arduino containing this program, needs to be connected to a Slave
//    Arduino (which has the Motor Driver Shield) via Serial.
//    See https://buildmusic.net/tutorials/motor-driver/ for details
// The TX of the Master is wired to the RX of the Slave, and the GND pins in the
// Master and Slave need to be connected.
//
// PROGMEM is used to store Song Title, Song Data, and Tempo
// Song Data is encoded as follows:
//      0x80 to 0x8C --> Play a Note (0x80=C, 0x82=D, 0x84=E, etc.)
//      0x01 to 0x7F --> Pause a specified length of time
//                       0x01 (1)  = Sixteenth-Note
//                       0x02 (2)  = Eighth-Note
//                       0x03 (3)  = Three-Sixteenth-Note
//                       0x04 (4)  = Quarter-Note
//                       0x06 (6)  = Three-Eighths-Note
//                       0x08 (8)  = Half-Note
//                       0x0C (12) = Three-Quarter-Note
//                       0x10 (16) = Whole-Note
//      0x00         --> End of Song 
//      0x8D to 0xFF --> End of Song
//
// *** External Libraries Required ***
// The following libraries must be downloaded and copied into the Arduino "libraries"
// folder in order for the program to compile:
//    o OneButton - https://github.com/mathertel/OneButton
//    o LiquidCrystal_I2C - https://github.com/fdebrabander/Arduino-LiquidCrystal-I2C-library
//
// ***********************************************************************************

#include <Arduino.h>
#include <Wire.h>
#include <OneButton.h>
#include <LiquidCrystal_I2C.h>
#include <avr/pgmspace.h>

// Set to 1 to output the note names (e.g. C5, D5, ...) via serial.  
// This is used for testing that songs are playing correctly without needing
//   an actual xylophone.
#define PLAY_DEBUG_MODE	0

// Baud rate for Serial messages from Master to Slave.  Baud rates in Master
// and Slave programs must match.
#define SERIAL_BAUD_RATE	57600

// Define the IO Pins Used for the Rotary Encoder
#define PIN_ROTARY_CLK    2   // Used for generating interrupts using CLK signal
#define PIN_ROTARY_DAT    3   // Used for reading DT signal
#define PIN_ROTARY_SW     4   // Used for the Rotary push button switch
#define PIN_ROTARY_5V     5   // Set to HIGH to be the 5V pin for the Rotary Encoder
#define PIN_ROTARY_GND    6   // Set to LOW to be the GND pin for the Rotary Encoder

// Unused Pins - Set all unused pins to Output / Low so that they are at Ground
// This allows one to plug the GND wire going to the Slave into any unused pin,
// not just the pin labeled GND.
#define PIN_UNUSED_1       7  
#define PIN_UNUSED_2       8
#define PIN_UNUSED_3       9  
#define PIN_UNUSED_4       10  
#define PIN_UNUSED_5       11  
#define PIN_UNUSED_6       12  
#define PIN_UNUSED_7       13  
#define PIN_UNUSED_8       14  
#define PIN_UNUSED_9       15  
#define PIN_UNUSED_10      16  
#define PIN_UNUSED_11      17  

// Most I2C LCD's have an I2C Address of either 0x27 or 0x3F
// If the LCD doesn't work with one address, try the other
//#define LCD_I2C_ADDRESS 0x27
#define LCD_I2C_ADDRESS     0x3F

// Define the size of the LCD.  Most LCD's are either 16x2 or 20x4
#define LCD_ROW_COUNT       2    // Number of Rows
#define LCD_COL_COUNT       16   // Number of Characters per Row

// User-friendly names of the notes, used in the songDataXX definitions
#define c  0x80  // Low C
#define d  0x82  // D
#define e  0x84  // E
#define f  0x85  // F
#define g  0x87  // G
#define a  0x89  // A
#define b  0x8B  // B
#define C  0x8C  // High C

byte songPlaying;         // True if a song is currently playing, False otherwise
const byte* songDataPtr;  // Pointer to the PROGMEM address where song data is stored

int tempo;                // Song tempo in Beats per Minute
int songIndex;            // Index of Currently Selected Song
const byte slaveAddr = 0; // Address of Slave Arduino with Motor Driver Shield

#define SONG_COUNT	12    // Number of Songs defined in PROGMEM

// Define the PROGMEM variables for all of the songs
//     Song Title is a char array (string)
//     Song Data is an array of bytes - See program header for definition of values
//     Tempo (in Beats per Minute) is specified with a #define

const PROGMEM char songTitle0[] = "Scale";
const PROGMEM byte songData0[] = { c,4,d,4,e,4,f,4,g,4,a,4,b,4,C,4,0};
#define songTempo0 150

const PROGMEM char songTitle1[] = "Chords";
const PROGMEM byte songData1[] = { c,e,g,C,4,c,f,a,C,4,c,e,g,C,4,d,f,g,b,4,c,e,g,C,4,0 };
#define songTempo1 120

const PROGMEM char songTitle2[] = "Do Re Mi";
const PROGMEM byte songData2[] = {c,6,d,2,e,6,c,2,e,4,c,4,e,8,d,6,e,2,f,2,f,2,e,2,d,2,f,16,
	e,6,f,2,g,6,e,2,g,4,e,4,g,8,f,6,g,2,a,2,a,2,g,2,f,2,a,16,0 };
#define songTempo2 150

const PROGMEM char songTitle3[] = "Grieg Morning";
const PROGMEM byte songData3[] = {
	g,4,e,4,d,4,c,4,d,4,e,4,g,4,e,4,d,4,c,4,d,2,e,2,d,2,e,2,g,4,e,4,
	g,4,a,4,e,4,a,4,g,4,e,4,d,4,c,12,g,4,e,4,d,4,c,4,d,4,e,4,g,4,e,4,
	d,4,c,4,d,2,e,2,d,2,e,2,g,4,e,4,g,4,a,4,e,4,a,4,b,4,g,4,b,4,C,0 };
#define songTempo3 150

const PROGMEM char songTitle4[] = "Old MacDonald";
const PROGMEM byte songData4[] = { C,4,C,4,C,4,g,4,a,4,a,4,g,8,e,4,e,4,d,4,d,4,c,12,
	g,4,C,4,C,4,C,4,g,4,a,4,a,4,g,8,e,4,e,4,d,4,d,4,c,12,g,2,g,2,C,4,C,4,C,4,g,2,g,2,C,4,C,4,C,8,
	C,2,C,2,C,4,C,2,C,2,C,4,C,2,C,2,C,2,C,2,C,4,C,4,C,4,C,4,C,4,g,4,a,4,a,4,g,8,e,4,e,4,d,4,d,4,c,12,0 };
#define songTempo4 200

const PROGMEM char songTitle5[] = "Itsy Bitsy Spidr";
const PROGMEM byte songData5[] = { g,2,c,3,c,1,c,3,d,1,e,4,e,3,e,1,d,3,c,1,d,3,e,1,c,8,
	e,4,e,3,f,1,g,4,g,4,f,3,e,1,f,3,g,1,e,8,c,4,c,3,d,1,e,4,e,4,d,3,c,1,d,3,e,1,c,4,
	g,3,g,1,c,3,c,1,c,3,d,1,e,4,e,3,e,1,d,3,c,1,d,3,e,1,c,8,0 };
#define songTempo5 120

const PROGMEM char songTitle6[] = "Ode to Joy";
const PROGMEM byte songData6[] = { e,4,e,4,f,4,g,4,g,4,f,4,e,4,d,4,c,4,c,4,d,4,e,4,e,6,d,2,d,8,
	e,4,e,4,f,4,g,4,g,4,f,4,e,4,d,4,c,4,c,4,d,4,e,4,d,6,c,2,c,8,
	d,4,d,4,e,4,c,4,d,4,e,2,f,2,e,4,c,4,d,4,e,2,f,2,e,4,d,4,c,4,d,4,g,8,
	e,4,e,4,f,4,g,4,g,4,f,4,e,4,d,4,c,4,c,4,d,4,e,4,d,6,c,2,c,8,0 };
#define songTempo6 160

const PROGMEM char songTitle7[] = "Twinkle Star";
const PROGMEM byte songData7[] = { c,4,c,4,g,4,g,4,a,4,a,4,g,8,f,4,f,4,e,4,e,4,d,4,d,4,c,8,
	g,4,g,4,f,4,f,4,e,4,e,4,d,8,g,4,g,4,f,4,f,4,e,4,e,4,d,8,
	c,4,c,4,g,4,g,4,a,4,a,4,g,8,f,4,f,4,e,4,e,4,d,4,d,4,c,8,0 };
#define songTempo7 150

const PROGMEM char songTitle8[] = "Water Music";
const PROGMEM byte songData8[] = {
	8,c,8,d,8,e,4,c,8,d,4,e,4,c,4,d,4,g,8,d,4,e,4,d,2,c,2,d,4,g,8,d,
	4,e,4,d,2,c,2,d,12,e,g,4,e,g,4,e,g,4,e,g,4,f,2,e,2,f,4,d,f,4,d,f,
	4,d,f,4,d,f,4,e,2,d,2,e,4,e,g,4,e,g,4,e,g,4,e,g,4,f,2,e,2,f,4,d,
	f,4,d,f,4,d,f,4,f,12,g,4,e,c,4,d,4,e,c,4,f,4,d,12,c,4,c,12,e,g,4,e,
	g,4,e,g,4,f,d,4,g,2,f,2,e,c,4,g,e,4,f,d,4,e,c,4,e,c,4,d,2,c,2,d,
	4,e,g,4,e,g,4,e,g,4,f,d,4,g,2,f,2,e,c,4,g,e,4,f,d,4,e,c,4,e,c,4,
	d,2,c,2,d,4,e,g,4,e,g,4,e,g,4,e,g,4,f,2,e,2,f,4,d,f,4,d,f,4,d,f,
	4,d,f,12,g,4,e,c,4,d,4,e,c,4,f,4,d,12,c,4,c,0 };
#define songTempo8 200

const PROGMEM char songTitle9[] = "Vivaldi Spring";
const PROGMEM byte songData9[] = {
	12,c,4,c,e,4,c,e,4,c,e,4,d,2,c,2,e,g,12,g,2,f,2,c,e,4,c,e,4,c,e,4,
	d,2,c,2,e,g,12,g,2,f,2,c,e,4,f,2,g,2,d,f,4,c,e,4,d,12,c,4,c,e,4,c,
	e,4,c,e,4,d,2,c,2,e,g,12,g,2,f,2,c,e,4,c,e,4,c,e,4,d,2,c,2,e,g,12,
	g,2,f,2,c,e,4,f,2,g,2,d,f,4,c,e,4,d,12,c,4,e,g,4,f,2,e,2,f,4,g,4,
	f,a,4,e,g,8,c,4,e,g,4,f,2,e,2,f,4,g,4,f,a,4,e,g,8,c,4,f,a,4,e,g,
	8,f,4,e,4,d,2,c,2,d,8,c,12,c,4,e,g,4,f,2,e,2,f,4,g,4,f,a,4,e,g,8,
	c,4,e,g,4,f,2,e,2,f,4,g,4,f,a,4,e,g,8,c,4,f,a,4,e,g,8,f,4,e,4,d,
	2,c,2,d,8,c,0 };
#define songTempo9 180

const PROGMEM char songTitle10[] = "Oh Susanna";
const PROGMEM byte songData10[] = {
	6,c,1,d,1,e,2,g,2,g,2,a,2,g,2,e,2,c,3,d,1,e,2,e,2,d,2,c,2,d,6,c,
	1,d,1,e,2,g,2,g,3,a,1,g,2,e,2,c,3,d,1,e,2,e,2,d,2,d,2,c,6,c,1,d,
	1,e,2,g,2,g,3,a,1,g,2,e,2,c,3,d,1,e,2,e,2,d,2,c,2,d,6,c,1,d,1,e,
	2,g,2,g,2,a,2,g,2,e,2,c,3,d,1,e,2,e,2,d,2,d,2,c,8,f,4,f,4,a,c,2,
	a,4,a,2,g,2,g,2,e,2,c,2,d,6,c,1,d,1,e,2,g,2,g,2,a,2,g,2,e,2,c,3,
	d,1,e,2,e,2,d,2,d,2,c,0 };
#define songTempo10 120

const PROGMEM char songTitle11[] = "Over the River";
const PROGMEM byte songData11[] = {
	e,g,2,e,g,2,e,g,2,e,g,2,c,e,2,d,f,2,e,g,4,e,g,2,e,g,4,e,g,2,f,C,
	4,f,C,2,f,b,4,f,a,2,e,g,10,e,g,2,f,4,f,2,f,4,f,2,c,e,4,c,e,2,c,e,
	4,c,e,2,c,d,4,c,d,2,c,d,4,c,e,2,d,6,d,g,6,e,g,2,e,g,2,e,g,2,e,g,
	2,c,e,2,d,f,2,e,g,4,e,g,2,e,g,4,e,g,2,f,C,2,f,C,2,f,C,2,f,b,4,f,
	a,2,e,g,10,e,g,2,c,C,4,c,C,2,c,b,4,c,a,2,c,g,4,c,e,2,c,4,d,2,c,e,
	4,f,2,e,4,d,2,c,0 };
#define songTempo11 150

// Assemble the PROGMEM variables into PROGMEM Arrays

const PROGMEM char* const songTitles[] = { songTitle0, songTitle1, songTitle2, songTitle3,
	songTitle4, songTitle5, songTitle6, songTitle7, songTitle8, songTitle9, songTitle10, songTitle11 };

const PROGMEM byte* const songData[] = { songData0, songData1, songData2, songData3,
	songData4, songData5, songData6, songData7, songData8, songData9, songData10, songData11 };

const PROGMEM int songTempos[] = { songTempo0, songTempo1, songTempo2, songTempo3,
	songTempo4, songTempo5, songTempo6, songTempo7, songTempo8, songTempo9, songTempo10, songTempo11 };


// OneButton class handles Debounce and detects button press
OneButton btnRot(PIN_ROTARY_SW, HIGH);		  // Rotary Select button

LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, LCD_COL_COUNT, LCD_ROW_COUNT);

// Used for the Rotary Encoder interrupt routines PinA() and PinB()
volatile int rotaryCount = 0;
const int rotaryMax = SONG_COUNT-1;
byte rotaryChanged;

// Disables the Rotary Encoder interrupts while the LCD is being updated
byte rotaryDisabled;

volatile byte aFlag = 0; // lets us know when we're expecting a rising edge on pinA 
						 // to signal that the encoder has arrived at a detent
volatile byte bFlag = 0; // lets us know when we're expecting a rising edge on pinB 
						 // to signal that the encoder has arrived at a detent 
						 // (opposite direction to when aFlag is set)
volatile byte reading = 0; //somewhere to store the direct values we read from our interrupt 
						   // pins before checking to see if we have moved a whole detent

// ****************************************************************************
// PinA() - Called by the Interrupt pin when the Rotary Encoder Turned
//    Routine taken from:  
//    https://exploreembedded.com/wiki/Interactive_Menus_for_your_project_with_a_Display_and_an_Encoder
// ****************************************************************************
void PinA() {

	if (rotaryDisabled) return;

	cli(); //stop interrupts happening before we read pin values
		   // read all eight pin values then strip away all but pinA and pinB's values
	reading = PIND & 0xC;

	//check that both pins at detent (HIGH) and that we are expecting detent on this pin's rising edge
	if (reading == B00001100 && aFlag) {
		rotaryUp();
		bFlag = 0; //reset flags for the next turn
		aFlag = 0; //reset flags for the next turn
	}
	//signal that we're expecting pinB to signal the transition to detent from free rotation
	else if (reading == B00000100) bFlag = 1;
	sei(); //restart interrupts
}

// ****************************************************************************
// PinB() - Called by the Interrupt pin when the Rotary Encoder Turned
//    Routine taken from:  
//    https://exploreembedded.com/wiki/Interactive_Menus_for_your_project_with_a_Display_and_an_Encoder
// ****************************************************************************
void PinB() {

	if (rotaryDisabled) return;

	cli(); //stop interrupts happening before we read pin values
		   //read all eight pin values then strip away all but pinA and pinB's values
	reading = PIND & 0xC;
	//check that both pins at detent (HIGH) and that we are expecting detent on this pin's rising edge 
	if (reading == B00001100 && bFlag) {
		rotaryDown();
		bFlag = 0; //reset flags for the next turn
		aFlag = 0; //reset flags for the next turn
	}
	//signal that we're expecting pinA to signal the transition to detent from free rotation
	else if (reading == B00001000) aFlag = 1;
	sei(); //restart interrupts
}

// ****************************************************************************
// rotaryUp() - Rotary Encoder is turned 1 detent to the Right (clockwise)
// **************************************************************************** 
void rotaryUp()
{
	rotaryCount++;
	if (rotaryCount > rotaryMax) rotaryCount = rotaryMax;
	rotaryChanged = 1;
}

// **********************************************************************************
// rotaryDown() - Rotary Encoder is turned 1 detent to the Left (counter-clockwise) 
// **********************************************************************************
void rotaryDown()
{
	rotaryCount--;
	if (rotaryCount < 0) rotaryCount = 0;
	rotaryChanged = 1;
}

// ****************************************************************************
// rotaryClick() - Rotary Encoder Select Switch is pressed
// ****************************************************************************
void rotaryClick()
{
	songPlaying = !songPlaying;
}

// ****************************************************************************
// rotaryLongPress() - Rotary Encoder Select Switch is Held Down (Long Press)
// ****************************************************************************
void rotaryLongPress()
{
	// Function is not implemented
}

// ****************************************************************************
// initializeRotaryEncoder() - Initialize the pins and interrupt functions
//                             for the Rotary Encoder
// ****************************************************************************
void initializeRotaryEncoder()
{
	// Set the Directions of the I/O Pins
	pinMode(PIN_ROTARY_CLK, INPUT_PULLUP);
	pinMode(PIN_ROTARY_DAT, INPUT_PULLUP);
	pinMode(PIN_ROTARY_SW, INPUT_PULLUP);
	pinMode(PIN_ROTARY_GND, OUTPUT);
	pinMode(PIN_ROTARY_5V, OUTPUT);

	// Set the 5V and GND pins for the Rotary Encoder
	digitalWrite(PIN_ROTARY_GND, LOW);
	digitalWrite(PIN_ROTARY_5V, HIGH);

	// set an interrupt on PinA and PinB, looking for a rising edge signal and 
	// executing the "PinA" and "PinB" Interrupt Service Routines
	attachInterrupt(0, PinA, RISING);
	attachInterrupt(1, PinB, RISING);

	// Define the functions for Rotary Encoder Click and Long Press
	btnRot.attachClick(&rotaryClick);
	btnRot.attachLongPressStart(&rotaryLongPress);
	btnRot.setPressTicks(2000);

	rotaryDisabled = 0;
}

// ****************************************************************************
// initializedUnusedPins() - Set any pins that are not used as digital
//    outputs with a LOW state.  This will make those pins effectively
//    function as a ground.
// ****************************************************************************
void initizlizeUnusedPins()
{
	pinOutLow(PIN_UNUSED_1);
	pinOutLow(PIN_UNUSED_2);
	pinOutLow(PIN_UNUSED_3);
	pinOutLow(PIN_UNUSED_4);
	pinOutLow(PIN_UNUSED_5);
	pinOutLow(PIN_UNUSED_6);
	pinOutLow(PIN_UNUSED_7);
	pinOutLow(PIN_UNUSED_8);
	pinOutLow(PIN_UNUSED_9);
	pinOutLow(PIN_UNUSED_10);
	pinOutLow(PIN_UNUSED_11);
}

// ****************************************************************************
// pinOutLow() - Set a pin as an Output and Low
// ****************************************************************************
void pinOutLow(byte pinNum)
{
	pinMode(pinNum, OUTPUT);
	digitalWrite(pinNum, LOW);
}

// ****************************************************************************
// initializeLcd() - Initialize the LCD
// ****************************************************************************
void initializeLcd()
{
	lcd.begin();
	lcd.backlight();
	lcd.clear();
	printSelectSong();
}

// ****************************************************************************
// printSelectedSong() - Update the LCD with Currently Selected Song
// ****************************************************************************
void printSelectedSong()
{
	songIndex = rotaryCount;
	rotaryDisabled = 1;
	lcd.setCursor(0, 1);
	lcdPrintProgStr((const char *)pgm_read_word(&songTitles[songIndex]));
	rotaryDisabled = 0;
	rotaryChanged = 0;
}

// ****************************************************************************
// lcdPrintProgStr() - Print a PROGMEM String directly to LCD
// ****************************************************************************
void lcdPrintProgStr(const char * str)
{
	byte i, n;	
	n = strlen_P(str);
	for (i = 0; i < n; i++) lcd.write(pgm_read_byte(str + i));
	for (i = n; i < LCD_COL_COUNT; i++) lcd.write(' ');
} 

// ****************************************************************************
// printSelectSong() - Text shown when in Song Selection Mode
// ****************************************************************************
void printSelectSong()
{
	lcd.setCursor(0, 0);
	lcd.print(F("Select a Song   "));
}

// ****************************************************************************
// printSongPlaying() - Text shown while Playing a Song
// ****************************************************************************
void printSongPlaying()
{
	lcd.setCursor(0, 0);
	lcd.print(F("--- Playing  ---"));
}

// ****************************************************************************
// setup() - Initialization Function
// ****************************************************************************
void setup()
{
	initializeRotaryEncoder();
	initizlizeUnusedPins();
	Serial.begin(SERIAL_BAUD_RATE);
	initializeLcd();
	rotaryChanged = 1;
}

// ****************************************************************************
// loop() - Main Program Loop Function 
// ****************************************************************************
void loop()
{
	if (songPlaying) playSelectedSong();
	if(rotaryChanged) printSelectedSong();
	btnRot.tick();
	delay(100);
}

// ****************************************************************************
// playSelectedSong() - Plays the currently selected song.  Reads tht tempo
//    from PROGMEM, and then reads and processes one song data byte at a time
// ****************************************************************************
void playSelectedSong()
{
	rotaryDisabled = 1;
	printSongPlaying();
	tempo = pgm_read_word(&songTempos[songIndex]);
	songDataPtr = (const byte *)pgm_read_word(&songData[songIndex]);
	while (processSongData(pgm_read_byte(songDataPtr++)));
	songPlaying = 0;
	Serial.println(F(""));
	printSelectSong();
	rotaryDisabled = 0;
}

// ****************************************************************************
// processSongData() - Process a byte of song data.
//     dataVal between 0x01 and 0x7F means to Pause
//     dataVal between 0x80 and 0x87 means to Play a Note
//     other values mean End of Song
// ****************************************************************************
byte processSongData(byte dataVal)
{
	if (dataVal == 0) return 0;
	if (dataVal > C) return 0;
	if (!songPlaying) return 0;

	if (dataVal >= c) playNote(dataVal);
	if (dataVal < c) pause(dataVal);
	
	return 1;
}

// ****************************************************************************
// pause() - Pause the specified number of sixteenth-notes
// ****************************************************************************
void pause(uint8_t num16ths)
{
	int i;
	int delayTicks = (unsigned long)num16ths * 120 * 125 / tempo / 20;
	for (i = 0; i < delayTicks; i++)
	{
		btnRot.tick();
		delay(20);
		if (!songPlaying) return;
	}
	
}

// ****************************************************************************
// playNote() - Send command to play a single note 
// ****************************************************************************
void playNote(byte note)
{
	byte ch;

	if (PLAY_DEBUG_MODE)
		playNoteSerialTest(note);
	else
	{
		ch = noteToMotorShieldChannel(note);
		if(ch>=0 && ch<=7)
			sendMotorShieldCommand(slaveAddr, 1 << ch);
	}
		
}

// ****************************************************************************
// sendMotorShieldCommand() - Send a Serial Command to the Slave
//       Serial Command is of the form: <aabb>
//       where aa is the Slave Address in Hexadecimal
//       and   bb is a Bitmask of the channels to be energized
// ****************************************************************************
void sendMotorShieldCommand(uint8_t boardNum, uint8_t pdata)
{
	Serial.print(F("<"));
	if (boardNum < 0x10)
		Serial.print(F("0"));
	Serial.print(boardNum, HEX);
	if (pdata < 0x10)
		Serial.print(F("0"));
	Serial.print(pdata, HEX);
	Serial.print(F(">"));
	return;
}

// ****************************************************************************
// noteToMotorShieldChannel() - Mapping from musical note (c,d,e, ...)
//   to channel on the Motor Driver Shield (0,1,2, ...)
// ****************************************************************************
byte noteToMotorShieldChannel(byte note)
{
	switch (note)
	{
	case c: return 0;
	case d: return 1;
	case e: return 2;
	case f: return 3;
	case g: return 4;
	case a: return 5;
	case b: return 6;
	case C: return 7;
	}
	return 0x7F;
}

// ****************************************************************************
// playNoteSerialTest() - Sends the name of the note to Serial instead of
//    the Motor Driver command.  This is used to listen to songs and test 
//    that they are playing correctly without needeing an actual xylophone.
// ****************************************************************************
void playNoteSerialTest(byte note)
{
	switch (note)
	{
	case c: Serial.print(F("C5")); break;
	case d: Serial.print(F("D5")); break;
	case e: Serial.print(F("E5")); break;
	case f: Serial.print(F("F5")); break;
	case g: Serial.print(F("G5")); break;
	case a: Serial.print(F("A5")); break;
	case b: Serial.print(F("B5")); break;
	case C: Serial.print(F("C6")); break;
	}
	Serial.print(F(" "));
}