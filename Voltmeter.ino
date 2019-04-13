 // Include the correct display library
 // For a connection via I2C using Wire include
#include <Wire.h>
#include "SSD1306Wire.h"
#include <Encoder.h>
#include "stats.h"

#define R1 1000000.0
#define R2 1000
#define REF_VALUE 106
#define REF_VOLTAGE 5.12

#define CAPTURE_TIME_SECONDS 60

#define REFRESH_TIME 50
#define BUFFER_SIZE 128

//#define DEBUG_LAYOUT
//#define DEBUG

SSD1306Wire  display(0x3c, D3, D5);
Encoder encoder(D6, D7);

double points[BUFFER_SIZE] = { 0 };
uint points_index = 0;
long last_point_time = 0;
long time_remaining = 0;

void setup()
{
	Serial.begin(115200);
	Serial.println();
	Serial.println();

	display.init();
	display.flipScreenVertically();
	display.setBrightness(255);

	display.setTextAlignment(TEXT_ALIGN_LEFT);
	display.setFont(ArialMT_Plain_10);
	display.drawString(0, 0, F("starting..."));
	display.display();

	randomSeed(millis());

	pinMode(D2, INPUT_PULLUP);
	attachInterrupt(D2, handleKey, RISING);
	
	delay(1000); //just for show :)
}

bool isButtonPressed = false;
long last_key_press = 0;
void handleKey() 
{
	isButtonPressed = true;
}

void clearArea(int16_t x, int16_t y, int16_t width, int16_t height)
{
	display.setColor(BLACK);
	display.fillRect(x, y, width, height);
	display.setColor(WHITE);
#ifdef DEBUG_LAYOUT
	display.drawRect(x, y, width, height);
#endif // DEBUG_LAYOUT
}

void drawValue(double value, const char *unit)
{
	clearArea(0, 0, 64, 34);

	display.setTextAlignment(TEXT_ALIGN_LEFT);
	display.setFont(ArialMT_Plain_24);
	display.drawString(0, 0, String(value, 1) + "v");
}

void drawStatistics(stats s, const char *unit)
{
	clearArea(64, 0, 64, 34);

	display.setFont(ArialMT_Plain_10);
	display.setTextAlignment(TEXT_ALIGN_RIGHT);
	display.drawString(128, 0, String(s.max) + unit);
	display.drawString(128, 10, String(s.min) + unit);
	display.drawString(128, 20, String(s.avg) + unit);
}

struct stats calcStatistics()
{
	stats result = { 255, 0, 0 };
	for (short i = 0; i < BUFFER_SIZE; i++)
	{
		if (points[i] > result.max) result.max = points[i];
		if (points[i] < result.min) result.min = points[i];
		result.avg += points[i];
	}
	result.avg = result.avg / BUFFER_SIZE;

	return result;
}

void drawGraph(long time, double voltage_max)
{
	clearArea(0, 34, 128, 30);

	double scale_max = max(REF_VOLTAGE, voltage_max);
	for (short i = 0; i < BUFFER_SIZE; i++)
	{
		int index = points_index + i;
		if (index >= BUFFER_SIZE) index -= BUFFER_SIZE;

		if (points[index] > 0.05 && points[index] <= voltage_max)
		{
			int y = 64 - points[index] / scale_max * 30;
			display.setPixel(128 - i, y);
		}
	}
}

void waitForRefresh(long time)
{
	time_remaining = REFRESH_TIME - (millis() - time);
	if (time_remaining > 0) delay(time_remaining);
}

bool canCapture(long time)
{
	return time - last_point_time >= CAPTURE_TIME_SECONDS * 1000 / BUFFER_SIZE;
}

void captureValue(double value, long time)
{
	last_point_time = time;
	points[points_index] = value;
	points_index++;

	if (points_index >= BUFFER_SIZE) points_index = 0;
}


void drawSettings(int32_t rot)
{
	int32_t rotary = -rot / 2 -1;
	if (rotary < 0) rotary += 11;

	display.setFont(ArialMT_Plain_10);
	display.setTextAlignment(TEXT_ALIGN_LEFT);

	String labels[] = {
			"0"
		  , "1"
		  , "2"
		  , "3"
		  , "4"
		  , "5"
		  , "6"
		  , "7"
		  , "8"
		  , "back"
	};

	int x = 0;
	int y = 1;
	for (int i = 0; i < 10; i++)
	{
		if (i == 5)
		{
			x = 64;
			y = 1;
		}

		if (rotary == i)
		{
			display.setColor(WHITE);
			display.fillRect(x, y, 64, 11);
			display.setColor(BLACK);
		}
		else
		{
			display.setColor(BLACK);
			display.fillRect(x, y, 64, 11);
			display.setColor(WHITE);
		}


		display.drawString(x, y, labels[i]);
		y += 11;
	}
}

void loop() 
{
	long time = millis();
	int32_t rot = encoder.read();
	int value = analogRead(A0);
	//value = random(0, 1024); //for testing only!

	double voltage = REF_VOLTAGE * (((double)value) / REF_VALUE);
	voltage = (voltage / 1024) / (R2 / (R1 + R2));

	bool cap = canCapture(time);
	if (cap) captureValue(voltage, time);

	if (rot == 0)
	{
		drawValue(voltage, "v");

		if (cap)
		{
			stats b = calcStatistics();
			drawStatistics(b, "v");
			drawGraph(time, b.max);
		}
	}
	else
	{
		drawSettings(rot);
	}




	// software debounce
	if (isButtonPressed && time - last_key_press > 50) 
	{
		isButtonPressed = false;
		last_key_press = time;
		
		encoder.write(0); // Reset the counter
	}

#ifdef DEBUG
	display.setTextAlignment(TEXT_ALIGN_LEFT);
	display.setFont(ArialMT_Plain_10);
	display.drawString(0, 22, "debug: " + String(rot));
#endif // DEBUG

	display.display();
	waitForRefresh(time);
}

