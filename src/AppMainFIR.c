	#include "main.h"
  #include <stdio.h>
	#include <string.h>
	#include "arm_math.h"
	#include "FIR_direct_lpf_coefficients.h"
	#include "FIR_direct_bpf_coefficients.h"
  #include "FIR_direct_hpf_coefficients.h"
	
void process_task(void);


extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern DMA_HandleTypeDef hdma_adc1;	
extern DAC_HandleTypeDef hdac;
extern DMA_HandleTypeDef hdma_dac1;
extern TIM_HandleTypeDef htim2;	

#define FRAME_LENGTH   256
#define n_sample       (FRAME_LENGTH/2)

uint32_t ADC_Buffer[FRAME_LENGTH];
uint32_t DAC_Buffer[FRAME_LENGTH];
		

float32_t A1[n_sample], B1[n_sample], C1[n_sample]; //triple buffer
float32_t *input1=A1, *process1=B1, *output1=C1, *temp1; //triple buffer pointers
float32_t A2[n_sample], B2[n_sample], C2[n_sample]; //triple buffer
float32_t *input2=A2, *process2=B2, *output2=C2, *temp2; //triple buffer pointers


// Filter
float32_t lpf_out[n_sample];
float32_t bpf_out[n_sample]; 
float32_t hpf_out[n_sample];

float32_t firStateF32_hpf[n_sample + K - 1];
float32_t firStateF32_bpf[n_sample + K - 1];
float32_t firStateF32_lpf[n_sample + K - 1];

arm_fir_instance_f32 S_hpf;
arm_fir_instance_f32 S_bpf;
arm_fir_instance_f32 S_lpf;


uint32_t* volatile pIn;
uint32_t* volatile pOut;
int16_t* pIn1 ,* pIn2;
int16_t* pOut1,* pOut2;

uint32_t  volatile FlagDataReady=0;
uint32_t offset =2000; 
char flag=0;
	
	
void AppMain(void)
{
	  //HAL_ADC_Start(&hadc2);
	//  HAL_ADCEx_MultiModeStart_DMA(&hadc1,ADC_Buffer,FRAME_LENGTH);
	  HAL_ADC_Start_DMA(& hadc1, ADC_Buffer, FRAME_LENGTH);
	  HAL_DAC_Start_DMA(&hdac,DAC_CHANNEL_1,DAC_Buffer,FRAME_LENGTH,DAC_ALIGN_12B_R);
	  HAL_TIM_Base_Start(&htim2);
	  DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_TIM2_STOP;
	 
	  arm_fir_init_f32(&S_hpf, K, h_high, firStateF32_hpf , n_sample);
		arm_fir_init_f32(&S_bpf, K, h_band, firStateF32_bpf , n_sample);
		arm_fir_init_f32(&S_lpf, K, h_low, firStateF32_lpf , n_sample);
		while(1)
		{
			   if(FlagDataReady)
				 {
					  FlagDataReady=0;
					  process_task();
				 }
		}
}


void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
	   
	   pIn=&ADC_Buffer[FRAME_LENGTH/2];
	   pOut= &DAC_Buffer[FRAME_LENGTH/2];
	   FlagDataReady=1;
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
	   
	   pIn = ADC_Buffer;
	   pOut= DAC_Buffer;
	   FlagDataReady=1;
}
int i;
void process_task(void)
{
//	 offset=0;
//		  for(i=0; i<n_sample;i++)
//	    {
//				 offset+=(pIn[i] & 0xFFFF);
//			}
//			offset /=n_sample;
	
	   for(i=0; i<n_sample;i++)
	   {
			   input1[i] =(float)(pIn[i] & 0xFFFF)-offset;
			   input2[i] = ((pIn[i]>>16) & 0xFFFF);// -offset;
			   pOut[i]=0;
			   pOut[i] = ((uint32_t)( output2[i]/*+1800*/)<<16)&0xFFFF0000;
			   pOut[i]|= ((uint32_t)( output1[i]+offset))&0x0000FFFF;
		 }
	
		 temp1= input1; input1=output1; output1=process1; process1=temp1;
		  _printf("i=%i \n",i);
	
				
			//	 arm_fir_f32(&S_lpf, process1, process1, n_sample);	
		  //   arm_fir_f32(&S_bpf, process1, process1, n_sample);
         arm_fir_f32(&S_hpf, process1, process1, n_sample);	
	
}


