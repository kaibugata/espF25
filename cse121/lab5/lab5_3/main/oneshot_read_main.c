/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

const static char *TAG = "EXAMPLE";
const static char *LED = "MORSE";

/*---------------------------------------------------------------
        ADC General Macros
---------------------------------------------------------------*/
//ADC1 Channels
#if CONFIG_IDF_TARGET_ESP32
#define EXAMPLE_ADC1_CHAN0          ADC_CHANNEL_2
#else
#define EXAMPLE_ADC1_CHAN0          ADC_CHANNEL_2
#endif


#define EXAMPLE_ADC_ATTEN           ADC_ATTEN_DB_12

static int adc_raw[2][10];
static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);
static void example_adc_calibration_deinit(adc_cali_handle_t handle);




typedef struct {
	const char *morse;
	char letter;
} MorseMap;

const MorseMap morseTable[] = {
	{".-", 'A'},{"-...", 'B'},{"-.-.", 'C'},{"-..",'D'},{".",'E'},{"..-.",'F'},{"--.",'G'},{"....",'H'},{"..",'I'},{".---",'J'},{"-.-",'K'},{".-..",'L'},{"--",'M'},{"-.",'N'},{"---",'O'},{".--.",'P'},{"--.-",'Q'},{".-.",'R'},{"...",'S'},{"-",'T'},{"..-",'U'},{"...-",'V'},{".--",'W'},{"-..-",'X'},{"-.--",'Y'},{"--..",'Z'},{".----",'1'},{"..---",'2'},{"...--",'3'},{"....-",'4'},{".....",'5'},{"-....",'6'},{"--...",'7'},{"---..",'8'},{"----.",'9'},{"-----",'0'},{"",'\0'}
};



char messageBuffer[200];
int msgIndex = 0;

void appendChar(char c){
	//ESP_LOGI(TAG, "entered appendChar with %c\n", c);
	if(c == '_'){//call this in the app_main loop when __ needs to be sent
		if(msgIndex < 200-1){
			messageBuffer[msgIndex++] = ' ';
			messageBuffer[msgIndex] = '\0';
		}
		return;
	}
	
	if(c == '!'){
		msgIndex = 0;
		messageBuffer[0] = '\0';
		return;
	}

	if(msgIndex < 200-1){
		messageBuffer[msgIndex++] = c;
		messageBuffer[msgIndex] = '\0';
	} else {
		printf("you fucked up too long buffer\n");
	}
}


#define MAX_MORSE_LEN 7 //max char 5 + "/" + "\0"
char morseResult[MAX_MORSE_LEN];

void processSymbol(char symbol) {
	static char morseLetter[MAX_MORSE_LEN];
	static int index = 0;

	if(symbol == '.' || symbol == '-') {
		if(index < MAX_MORSE_LEN -1){
			morseLetter[index++] = symbol;
			morseLetter[index] = '\0';		
		}
	}
	else if(symbol == '/' || symbol == '_'){
		morseLetter[index] = '\0';
		strcpy(morseResult,morseLetter);
		//ESP_LOGI(TAG, "morseResult: %s\n", morseResult);
		index = 0;
		morseLetter[0] = '\0';
	}
}


char morseTochar(const char *morse){
	//ESP_LOGI(TAG, "morse used in morsetochar: %s\n",morse);
	for(int i = 0; morseTable[i].morse[0] != '\0'; i++){
		if(strcmp(morse, morseTable[i].morse) == 0){
			//ESP_LOGI(TAG, "char returned: %c\n", morseTable[i].letter);
			return morseTable[i].letter;
		}
	}
	return '!';
}


void app_main(void)
{
    //-------------ADC1 Init---------------//
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    //-------------ADC1 Config---------------//
    adc_oneshot_chan_cfg_t config = {
        .atten = EXAMPLE_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, EXAMPLE_ADC1_CHAN0, &config));

    //-------------ADC1 Calibration Init---------------//
    adc_cali_handle_t adc1_cali_chan0_handle = NULL;
    bool do_calibration1_chan0 = example_adc_calibration_init(ADC_UNIT_1, EXAMPLE_ADC1_CHAN0, EXAMPLE_ADC_ATTEN, &adc1_cali_chan0_handle);



    bool first_ = 0;
    bool first_1_ = 0;
    char result;
    int highCounter = 0;
    int lowCounter = 0;
    int noiseCounter = 0;

    
    //CURRENT RATE:1 char/.21 sec -> 4.76 char/1 sec (MAXIMUM WITHOUT NEEDING TO DECREASE DELAY)
    //FAILS AT: 1 char/.19 sec -> 5.26 char/1 sec (Causes Watchdog Errors)
    //DIFFERENCE  (5.26-4.76)/4.76 = 10.5% < 25%

    while (1) {
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN0, &adc_raw[0][0]));
        //ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, EXAMPLE_ADC1_CHAN0, adc_raw[0][0]);
        //ESP_LOGI(TAG, "highCnt: %d, lowCnt: %d, noiseCnt: %d",highCounter,lowCounter,noiseCounter);
        vTaskDelay(pdMS_TO_TICKS(10));//initial val 1000 //change to 10 for curr rate


 
    if(adc_raw[0][0]  > 21){//the led is currently on
	//ESP_LOGI(LED,"rawData high");
	if(lowCounter != 0){//if led has been off for some time
		if(lowCounter >= 8  && lowCounter < 11 && noiseCounter == 0){//test for / lowcounter >= 8 && lowCOunter < 11
			//ESP_LOGI(LED, "/");
			processSymbol('/');
			//printf("%s\n",morseResult);
			if(morseResult[0] != '\0'){
				//ESP_LOGI(TAG, "YAY MORSE CODE IS VALID\n");
				result = morseTochar(morseResult);
				appendChar(result);
				//printf("%s\n", messageBuffer);
			}else{
				//ESP_LOGI(TAG, "MORSE IS NOT VALID %s\n",morseResult);
			}
			//printf("%c\n", result);
		}
		if(noiseCounter == 1){//its for real on
			lowCounter = 0;
			noiseCounter = 0;
		}
		noiseCounter++;
		highCounter++;	
	} else{
		noiseCounter = 0;
		lowCounter = 0;
		highCounter++;
	}
    }else{
    	noiseCounter = 0;
	if(highCounter >= 2 && highCounter <= 4){//.  //highcounter >= 2 && highcounter <= 4
		//ESP_LOGI(LED, ".");
		processSymbol('.');
		highCounter = 0;
	}else if(highCounter > 4  && highCounter <= 7){//-  //highcounter > 4 && highcounter <= 7
		//ESP_LOGI(LED, "-");
		processSymbol('-');
		highCounter = 0;
	}

	
	if(lowCounter == 14){// "  " lowCOunter == 14
		//ESP_LOGI(LED, "____");
		//ESP_LOGI(TAG, "first_ is %d\n", first_);
		if(first_){
		  processSymbol('_');
		  //printf("%s\n",morseResult);
		  result = morseTochar(morseResult);
		  appendChar(result);
		  //printf("%c\n", result);
		  appendChar('_');
		  //ESP_LOGI(TAG, "big things are happening\n");
		  //printf("%s\n", messageBuffer);
		} else{
			first_ = 1;
		}
		highCounter = 0;
	}else if(lowCounter == 33){//end of entire text  //lowcounter == 33
		//ESP_LOGI(LED, "!");
		if(first_1_){//maybe will cause issue later on idk make sure to check on this
			printf("%s\n", messageBuffer);
			appendChar('!');
		} else{
			first_1_ = 1;
		}
		highCounter = 0;
	}

	lowCounter++;
	highCounter = 0;
    }
}

    //Tear Down
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
    if(do_calibration1_chan0){
    	example_adc_calibration_deinit(adc1_cali_chan0_handle);
    }


    //FOR CURRENT CONFIGURATION:
    //11 or 12   = dash(if adc >= 21)
    //4 or 5  = dot(if adc >= 21)
    //approx 15 = space between letters (if adc < 21)
    //approx 29 = space between words (if adc < 21)
    //approx 71   = space between lines (if adc < 21)
}

/*---------------------------------------------------------------
        ADC Calibration
---------------------------------------------------------------*/
static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

static void example_adc_calibration_deinit(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Curve Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));

#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Line Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif
}
