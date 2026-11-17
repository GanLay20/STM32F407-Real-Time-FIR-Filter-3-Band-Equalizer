
#include "arm_math.h"
#include "FIR_lpf_coefficients.h"
#include <assert.h>

 void plot(float *buf, uint32_t size);

void convolution_sum (float x[], float h[], float y[], int size1 , int size2){
	int n,k;
	float sum;
		for(n=0;n<size1;n++)
		{
			 sum=0.0;
			 for(k=0;k<size2;k++)
			 {
				  if(k<(n+1)) 
						sum+=h[k]*x[n-k];
			 }
				y[n]=sum;
		}
}

 #define N 512

 int n;
 float x[N],y[N];
 float value;
 int AppMain (void){

	 for(n=0;n<N;n++)
	 x[n] = arm_sin_f32(PI*n/128)+arm_sin_f32(PI*n/4);
   plot(x,N);
	 
	 convolution_sum (x,h,y,N,K); 
   plot(y,N);
	 
	 assert(0);
 return(0);
 }
 
 void plot(float *buf, uint32_t size)
 {
	 float max;
	 uint32_t index;
	   arm_max_f32(buf,size,&max,&index);
	   arm_scale_f32(buf,1.0F/max,buf,size);
	 
	 for(int i=0;i<size;i++)
	 {
		  value=buf[i];
		  for(int j=0; j<60000;j++)
		           ;
	 }
	   
	 //assert(0);
 }
 
	 