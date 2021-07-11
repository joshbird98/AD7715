#include "AD7715.h"
#include "BPM.h"
#include <SPI.h>
#include <stdint.h>

/**************************************************************************/
/*!
    @brief  Interfaces BPM PCB with AD7715s - CPP FILE
*/
/**************************************************************************/

uint32_t conversion_factor; //global variable - must be initialized in setup then used by loop

struct Adc adc_1;
struct Adc adc_2;
struct Adc adc_3;
struct Adc adc_4;

// Sets up ADC struct and configures AD7715 over SPI
uint8_t ADC_Setup( struct Adc *adc, uint8_t cs_pin, uint8_t data_ready_pin, uint8_t gain, uint16_t refresh_rate, uint8_t storage_size) {
	
	pinMode(cs_pin, OUTPUT);
	digitalWrite(cs_pin, HIGH);
	pinMode(data_ready_pin, INPUT);
	
	// Set up ADC struct
	adc->cs_pin = cs_pin;
	adc->data_ready_pin = data_ready_pin;
	if ((gain == 1) or (gain == 2) or (gain == 32) or (gain == 128))
	{
		adc->gain = gain;
	}
	else { adc->gain = 1; }
	if ((refresh_rate == 50) or (refresh_rate == 60) or (refresh_rate == 250) or (refresh_rate == 500))
	{
		adc->refresh_rate = refresh_rate;
	}
	else { adc->refresh_rate = 50; }
	adc->max_sample_period = AD7715_READ_TIMEOUT_FACTOR / refresh_rate;
	//Serial.println(AD7715_READ_TIMEOUT_FACTOR);
	//Serial.println(refresh_rate);
	if ( (AD7715_READ_TIMEOUT_FACTOR % refresh_rate) > 0)
	{
		adc->max_sample_period += 2;
	}
	
	
	adc->res_pos = sizeof adc->results / sizeof adc->results[0];
	memset(adc->results, 0, sizeof adc->results); // First result ends up in position zero
	switch (cs_pin) {
		case cs_1:
		  adc->offset = CURRENT_OFFSET_1;
		  break;
		case cs_2:
		  adc->offset = CURRENT_OFFSET_2;
		  break;
		case cs_3:
		  adc->offset = CURRENT_OFFSET_3;
		  break;
		case cs_4:
		  adc->offset = CURRENT_OFFSET_4;
		  break;
	}
	// Configure and calibrate ADC
	if (ADC_Configure_Calibrate(*adc)) return 1;
	return 0;
}


// This function configures and calibrates a single AD7715
uint8_t ADC_Configure_Calibrate( struct Adc adc) {
	//Serial.println("\n\nConfiguring and calibrating ADC");
	// Calculates values to send to AD7715
	uint8_t comms_value = 0x10;
	uint8_t setup_value = 0x66;
	
	switch(adc.gain) {
		case 1:
			comms_value |= 0x00;
			break;
		case 2:
			comms_value |= 0x01;
			break;
		case 32:
			comms_value |= 0x10;
			break;
		case 128:
			comms_value |= 0x11;
			break;
		default:
			comms_value |= 0x00;
			break;
	}
	
	switch(adc.refresh_rate) {
		case 50:
			setup_value |= 0x00;
			break;
		case 60:
			setup_value |= 0x08;
			break;
		case 250:
			setup_value |= 0x10;
			break;
		case 500:
			setup_value |= 0x18;
			break;
		default:
			setup_value |= 0x00;
			break;
	}
	
	uint8_t remaining_attempts = 3;
	
	while (remaining_attempts > 0) {
		
		// Force AD7715 into default state to ensure comms sync
		//Serial.println("Trying to force default state");
		SPI.begin();
		digitalWrite(adc.cs_pin, LOW);
		SPI.transfer(0xFF);
		SPI.transfer(0xFF);
		SPI.transfer(0xFF);
		SPI.transfer(0xFF);
		digitalWrite(adc.cs_pin, HIGH);
		SPI.end();
		
		// Read current contents of setup register
		SPI.begin();
		digitalWrite(adc.cs_pin, LOW);
		SPI.transfer(0x18);
		uint8_t result = 0;
		result = SPI.transfer(0);
		digitalWrite(adc.cs_pin, HIGH);
		SPI.end();
		//Serial.print("Setup register currently contains 0x");
		//Serial.println(result, HEX);
		
		// Attempt configuration
		//Serial.println("Attempting configuration...");
		SPI.begin();
		digitalWrite(adc.cs_pin, LOW);
		SPI.transfer(comms_value);
		SPI.transfer(setup_value);
		// Wait for configuration to complete
		uint32_t start_time = millis();
		uint32_t time_elapsed = 0;
		uint8_t incomplete = digitalRead(adc.data_ready_pin);
		while ( (incomplete == HIGH) && (time_elapsed <= AD7715_SETUP_TIMEOUT) ) {
			time_elapsed = millis() - start_time;
			incomplete = digitalRead(adc.data_ready_pin);
		}
		if (time_elapsed >= AD7715_SETUP_TIMEOUT) {
			//Serial.println("Setup timeout error. Invalid response from AD7715");
		}
		digitalWrite(adc.cs_pin, HIGH);
		SPI.end();
		
		// Checking contents of setup register
		result = 0;
		//Serial.println("Checking contents...");
		SPI.begin();
		digitalWrite(adc.cs_pin, LOW);
		SPI.transfer(comms_value |= 0x08);
		result = SPI.transfer(0);
		digitalWrite(adc.cs_pin, HIGH);
		SPI.end();
		//Serial.print("Setup register now contains: 0x");
		//Serial.println(result, HEX);
		
		// Check for success
		if (result == (setup_value & 0x3F)) {
			//Serial.println("Successful configuration and calibration");
			// Reset measurements
			memset(adc.results, 0, sizeof(adc.results));
			adc.res_pos = 0;
			adc.total_samples = 0;
			return 1;
		}
		else {
			remaining_attempts -= 1;
			//Serial.println("Unsuccessful configuration and calibration");
			if (remaining_attempts > 0) {
				//Serial.print(remaining_attempts);
				//Serial.println(" attempt(s) remaining. Trying again... ");
			}
		}
	}
	
	// Did not perform a successful write to setup register
	return 0;
}


// This function adds a raw binary result for the selected ADC
uint8_t ADC_Read( struct Adc *adc ) {
	
	while (digitalRead(adc->data_ready_pin) == LOW)
	{
		delayMicroseconds(1);
	}
	uint16_t adc_result = 0;

	// Reading contents of Data Register
	SPI.begin();
	digitalWrite(adc->cs_pin, LOW);
	SPI.transfer(0x38);
	adc_result = SPI.transfer(0);
	adc_result <<= 8; 
	adc_result |= SPI.transfer(0);
	digitalWrite(adc->cs_pin, HIGH);
	SPI.end();
	
	// Write result into ADC struct
	adc->res_pos += 1;
	if (adc->res_pos >= ((sizeof(adc->results)/2))) {
		adc->res_pos = 0;
	}
	adc->results[adc->res_pos] = adc_result;
	if (adc->total_samples < UINT32_MAX) {
		adc->total_samples += 1;
	}
	return 1; // Successfuly recorded a value
}


// Finds the average ADC result from the last [timeframe_ms] milliseconds
// Note that the average will only use results from the most recent configuration
float ADC_Average_t( struct Adc adc, uint32_t timeframe_ms) {
	// Calculate how many samples need to (or can) be averaged
	timeframe_ms = min(timeframe_ms, ((1000 * sizeof(adc.results)) / adc.refresh_rate));	// limit timeframe to max size of results
	uint32_t num_samples = (timeframe_ms * adc.refresh_rate ) / 1000;
	num_samples = min(num_samples, adc.total_samples);						// limit num_samples to number available
	
	uint32_t sum = 0;
	uint16_t tmp_pos = adc.res_pos;
	uint32_t num_samples_rem = num_samples;
	while (num_samples_rem > 0) {
		sum += adc.results[tmp_pos];
		tmp_pos -= 1;
		if (tmp_pos >= sizeof(adc.results)) tmp_pos = ((sizeof(adc.results) / sizeof(adc.results[0])) - 1);
		num_samples_rem -= 1;
	}
	float average = (float)sum / (float)num_samples;
	return average;
}


// Finds the average ADC result from the last [samples] samples
// Note that the average will only use results from the most recent configuration
float ADC_Average_s( struct Adc adc, uint8_t samples) {
	// Limit number of samples to number available
	uint32_t num_samples = min(samples, adc.total_samples);
	
	uint32_t sum = 0;
	uint16_t tmp_pos = adc.res_pos;
	uint32_t num_samples_rem = num_samples;
	//Serial.print("\n");
	while (num_samples_rem > 0) {
		sum += adc.results[tmp_pos];
		//Serial.print(adc.results[tmp_pos]);
		//Serial.print(",");
		tmp_pos -= 1;
		if (tmp_pos >= sizeof(adc.results)) tmp_pos = ((sizeof(adc.results) / sizeof adc.results[0]) - 1);
		num_samples_rem -= 1;
	}
	//Serial.print("\nSUM: ");
	//Serial.println(sum);
	float average = (float)sum / (float)num_samples;
	return average;
}


// This function takes an ADC result (as a FLOAT) and converts to current
// Units available are 'p', 'n', 'u' or 'a' or automatic
float ADC_Value_To_Current( float value, uint8_t unit, uint32_t conversion_factor, uint8_t gain, float offset) {
	float current = (value * (float)conversion_factor) / gain;
	current = current - offset;
	switch(unit) {
		case 'p':
			return current;
		case 'n':
			return current / 1000.0;
		case 'u':
			return current / 1000000.0;
		case 'a':
			if (current < 1000) {
				Serial.print(current);
				Serial.println(F(" [pA]"));
			}
			else if (current < 1000000) {
				Serial.print(current / 1000.0);
				Serial.println(F(" [nA]"));
			}
			else {
				Serial.print(current / 1000000.0);
				Serial.println(F(" [uA]"));
			}
		default:
			return current;
	}
	return 0;
}


// This function prints the most recent ADC measurement [pA]
uint8_t ADC_Serial_Print_pA( struct Adc adc, uint32_t conversion_factor ) {
	Serial.println(ADC_Value_To_Current(adc.results[adc.res_pos], 'p', conversion_factor, adc.gain, adc.offset));
	return 1;
}