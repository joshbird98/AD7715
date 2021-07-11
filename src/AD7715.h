#ifndef AD7715_H
#define AD7715_H

#ifndef ARDUINO
#include <stdint.h>
#elif ARDUINO >= 100
#include "Arduino.h"
#include "Print.h"
#else
#include "WProgram.h"
#endif

#define VERSION 1.0

/* Constants */
#define MAX_CURRENT_pA 28868360
#define MAX_ADC_OUT 65535
#define AD7715_SETUP_TIMEOUT 350
#define AD7715_READ_TIMEOUT_FACTOR 1050
#define CURRENT_OFFSET_1 312852.793
#define CURRENT_OFFSET_2 1760 //I think the output is being limited... possibly some offset on the AD711 causes 0A to have negative voltage
#define CURRENT_OFFSET_3 85541.8666
#define CURRENT_OFFSET_4 220410.5263

/* Functions */

uint8_t ADC_Setup( struct Adc *adc, uint8_t cs_pin, uint8_t data_ready_pin, uint8_t gain, uint16_t refresh_rate, uint8_t storage_size);
uint8_t ADC_Configure_Calibrate( struct Adc adc);
uint8_t ADC_Read( struct Adc *adc );
float ADC_Average_t( struct Adc adc, uint32_t timeframe_ms);
float ADC_Average_s( struct Adc adc, uint8_t samples);
float ADC_Value_To_Current( float value, uint8_t unit, uint32_t conversion_factor, uint8_t gain, float offset);
uint8_t ADC_Serial_Print_pA( struct Adc adc, uint32_t conversion_factor );

/* Structs */
struct Adc {
	uint8_t gain;					// [1,2,32,128]
	uint16_t refresh_rate;			// [50, 60, 250, 500] Hz
	uint16_t max_sample_period;
	uint8_t cs_pin;		
	uint8_t data_ready_pin;
	uint16_t results[100];			// Stores results as a rolling array
	uint16_t res_pos;				// Shows position of most recent result in results[]
	uint32_t total_samples;  		// Total number of samples recorded during this setting
	float offset;				// Specific offset for each ADC [pA]
};

extern struct Adc adc_1;
extern struct Adc adc_2;
extern struct Adc adc_3;
extern struct Adc adc_4;
extern uint32_t conversion_factor;

#endif