#include "main.h"
#include "stm32f1xx_hal.h"
#include <string.h>

/* Temp
*/
int SEND = 0;
int Delay = 3000;

IWDG_HandleTypeDef hiwdg;
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
typedef struct Message Message;

struct Message{
	char *Command;
	char *Response;
	int NoCheck; // if NoCheck == 1 go to next instruction , ~take car of response
	Message* Next;
};
//MODULE STARTUP
char AT_Command[] = "AT\r"; 
char AT_Response[]= "OK";
char SimReady_Command[] = "AT+CPIN?\r"; 
char SimReady_Response[]= "+CPIN: SIM PIN";
char PIN_Command[]= "AT+CPIN=\"1111\"\r"; 
char PIN_Response[]= "OK";

//GPRS
char Cgatt_Command[] = "AT+CGATT?\r";
char Cgatt_Response[] = "+CGATT: 1";
char Cstt_Command[] = "AT+CSTT=\"mcinet\"\r";
char Cstt_Response[] = "OK";
char Ciicr_Command[] = "AT+CIICR\r";
char Ciicr_Response[] = "OK";
char Cifsr_Command[] = "AT+CIFSR\r";
char Cifsr_Response[];
char CipStart_Command[] ="AT+CIPSTART=\"TCP\",\"tohera.ir\",\"8888\"\r";
char CipStart_Response[] = "CONNECT OK\r\n";
char CipSend_Command[] = "AT+CIPSEND\r";
char End_Data[] = "1a";
char End_Data_Response[]= "SEND OK\r";

//GNS
char CGNSPWR_Command[] = "AT+CGNSPWR=1\r";
char CGNSPWR_Response[] = "OK";
char CGNSINF_Command[] = "AT+CGNSINF\r";
char Loc[19];



Message* msg;

//Start Up module
Message At; // To check module is Ready
Message SimReady; // To check SimCard is Ready
Message Pin; //Pin Code insertation

//Bringup GRPS

Message CGATT; // GPRS Status
Message CSTT; // APN Insertation
Message CIICR; //Bring up wireless connection
Message CIFSR; //Get Local Ip Address
Message CIPSTART; //StartUp Connection
Message CIPSEND;  // prompt will appear and data insetration phase
Message Msg; // The massage that should be send to server
Message End; // End the message to send

//Bringup GNS

Message CGNSPWR;
Message CGNSINF;


char Rx_indx, Rx_data[2], Rx_Buffer[100];
char Rx2_indx,Rx2_data[2],Rx2_Buffer[100];
uint8_t len;
uint8_t len2;
char buffer[100];
char buffer2[100];
char Res[100];



int Check_One = 0;	// To Check 0x0D  ----> A Responce will start by 0x0D,0x0A
int Check_Two = 0;	// To Check First 0x0D and 0x0A
int Check_Three = 0; // To check Second 0x0D and 0x0A



int Start_Index = 0;
int Stop_Index = 0;

uint8_t j; // To clear Rx2_Buffer


void SystemClock_Config(void);
void Error_Handler(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_IWDG_Init(void);
static void Index_Setter(void);
static int Response_Checker(Message*);
static void Send_Sim(Message*);
static void System_Initializer(void);
static void Command_Changer();
static char* GetLocation(char*);


int main(void)
{
	System_Initializer();
	
	//ModuleStartUp part
	At.Command = AT_Command;
	At.Response = AT_Response;
	At.NoCheck = 0;
	SimReady.Command = SimReady_Command;
	SimReady.Response = SimReady_Response;
	SimReady.Next = &Pin;	
	SimReady.NoCheck = 0;
	Pin.Command = PIN_Command;
	Pin.Response = PIN_Response;
	Pin.NoCheck = 1;
	Pin.Next = &CGATT;
	
	//GPRS PART
	CGATT.Command = Cgatt_Command;
	CGATT.Response = Cgatt_Response;
	CGATT.Next = &CSTT;
	CGATT.NoCheck = 0;
	CSTT.Command = Cstt_Command;
	CSTT.Response = Cstt_Response;
	CSTT.Next = &CIICR;
	CSTT.NoCheck = 0;
	CIICR.Command = Ciicr_Command;
	CIICR.Response = Ciicr_Response;
	CIICR.Next = &CIFSR;
	CIICR.NoCheck = 0;
	CIFSR.Command = Cifsr_Command;
	CIFSR.Response = Cifsr_Response;
	CIFSR.Next =&CIPSTART;
	CIFSR.NoCheck = 1;
	CIPSTART.Command = CipStart_Command;
	CIPSTART.Response = CipStart_Response;
	CIPSTART.Next = &CIPSEND;
	CIPSTART.NoCheck = 0;
	CIPSEND.Command = CipSend_Command;
	CIPSEND.NoCheck = 1;
	CIPSEND.Next = &Msg;
//	Msg.Command = Loc;
	Msg.Next = &End;
	Msg.NoCheck = 1;
	End.Command = End_Data;
	End.NoCheck = 0;
	End.Response = End_Data_Response;
	End.Next = &CIPSEND;
	
	//GNS PART
	CGNSPWR.Command = CGNSPWR_Command;
	CGNSPWR.Response = CGNSPWR_Response;
	CGNSPWR.NoCheck = 0;
	CGNSPWR.Next = &CGNSINF;
	
	CGNSINF.Command = CGNSINF_Command;
	CGNSINF.NoCheck = 0;
	CGNSINF.Next = &Pin;

	
	
	msg = &CGNSPWR;
  while (1){
		while(1){

			if(msg == &Msg){
				//{"id":2,"lat":32.636164,"long":51.700544}
				char Lat[9];
				char Long[9];
				strncpy(Lat,Loc,9);
				strncpy(Long,Loc+10,9);
				char str[80];
				sprintf(str, "{\"id\":2,\"lat\":%s,\"long\":%s}",Lat,Long);
				Msg.Command = str;
				Send_Sim(msg);
				str[0] = '\0';
			}
			
			if(msg == &End)
			{	
				uint8_t x = 0x1a;
				HAL_UART_Transmit(&huart2 ,(uint8_t*) &x,1, 1000);
			}
			else
				Send_Sim(msg);

			HAL_Delay(Delay);
 			if(msg->NoCheck == 1){
				msg = msg->Next;
				for (j=0;j<100;j++) {Rx2_Buffer[j]=0;} //Clear the Buffer
				Rx2_indx = 0;
				break;
			}
			if(Response_Checker(msg)){
				if(msg == &End){
					SEND = 1;
					Delay = 10000;
				}
				msg = msg->Next;
				break;		
			}
		}
	}
}
static int Response_Checker(Message* msg){
//	if(Check_Three == 1){
		if(msg == &CGNSINF){
			if(Rx2_Buffer[25] == '0'){ // No gps info
				for (j=0;j<100;j++) {Rx2_Buffer[j]=0;} //Clear the Buffer
				Rx2_indx = 0;
				Check_Three = 0;
				return 0;
			}
			else if (Rx2_Buffer[25] == '1'){// Data for Gps is valid Here
					strcpy(Loc,GetLocation(Rx2_Buffer));
					return 1;
			}
		}
		else if(msg == &CIPSTART){
			strncpy (Res, Rx2_Buffer+45, 53);
			len2 = strlen(Rx2_Buffer);
			for (j=0;j<100;j++) {Rx2_Buffer[j]=0;} //Clear the Buffer
			Rx2_indx = 0;
			Check_Three = 0;
			if(strncmp( msg->Response,Res,strlen(Res)) == 0){		
				for (j=0;j<100;j++) {Res[j]=0;} //Clear the Res
					return 1; //The Response is ok
			}
			else{
				for (j=0;j<100;j++) {Res[j]=0;} //Clear the Res
				return 0;
			}		
		}
		else if(msg == &End){
			strncpy (Res, Rx2_Buffer+3, 8);
			len2 = strlen(Rx2_Buffer);
			for (j=0;j<100;j++) {Rx2_Buffer[j]=0;} //Clear the Buffer
			Rx2_indx = 0;
			Check_Three = 0;
			if(strncmp( msg->Response,Res,strlen(Res)) == 0){		
				for (j=0;j<100;j++) {Res[j]=0;} //Clear the Res
					return 1; //The Response is ok
			}
			else{
				for (j=0;j<100;j++) {Res[j]=0;} //Clear the Res
				return 0;
			}		
		}
		
		else{
			strncpy (Res, Rx2_Buffer+Start_Index, Stop_Index-Start_Index-1);
			len2 = strlen(Rx2_Buffer);
			for (j=0;j<100;j++) {Rx2_Buffer[j]=0;} //Clear the Buffer
			Rx2_indx = 0;
			Check_Three = 0;
			if(strncmp( msg->Response,Res,strlen(Res)) == 0){		
				for (j=0;j<100;j++) {Res[j]=0;} //Clear the Res
				return 1; //The Response is ok
			}
			else{
				for (j=0;j<100;j++) {Res[j]=0;} //Clear the Res
				return 0;
			}
		}
//	}
		return NULL;
}

static void Send_Sim(Message* msg){
	len = strlen(msg->Command);
	HAL_UART_Transmit(&huart2 ,msg->Command,len, 1000);
}

static char* GetLocation(char* str){
	int start;
	int end;
	int j = 0;
	char value[19];
	int counter = 0;
	while(1){
		if(str[j] == ','){
			counter ++;
			if(counter == 3)
				start = j;	
			if(counter == 5){
				end = j;
				return strncpy(value,str+start+1,end-start);
			}
		}
		j++;
	}
}
static void Index_Setter(void){
	if(Check_Two == 0){
		if(Rx2_data[0] == 0x0D){
			Check_One = 1;
			}
		else if(Check_One == 1){
			if(Rx2_data[0] == 0x0A){
				Check_Two = 1;
				Start_Index = Rx2_indx;
			}
			else{
				Check_One = 0;
			}
		}
	}
		else{
			if(Rx2_data[0] == 0x0D){
				Check_One = 1;
			}
			else if(Check_One == 1){
				if(Rx2_data[0] == 0x0A){
					Check_Two = 0;
					Check_Three = 1;
					Stop_Index = Rx2_indx-1;
				}
				else{
					Check_One = 0;
				}
			}
					
		}
}

static void System_Initializer(void){
	HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_IWDG_Init();
	HAL_UART_Receive_IT(&huart1, Rx_data, 1);
	HAL_UART_Receive_IT(&huart2, Rx2_data, 1);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	uint8_t i; 
	if (huart->Instance == USART1){ //PC to Micro
	if (Rx_indx==0) {for (i=0;i<100;i++) Rx_Buffer[i]=0;}
			if (Rx_data[0]!=13){
				Rx_Buffer[Rx_indx++]=Rx_data[0];
		}
		else { 
			Rx_indx=0; 
			len = strlen(Rx_Buffer);
			HAL_UART_Transmit(&huart1 ,Rx_Buffer,len, 1000); //send to sim808
		}
		HAL_UART_Receive_IT(&huart1, Rx_data, 1);
		}
	 if (huart->Instance == USART2){ //sim808 to Micro
		 
		Rx2_Buffer[Rx2_indx++]=Rx2_data[0];
		if(Check_Three == 0){ // To just get first 0x0D 0x0A
			Index_Setter();
		}
		HAL_UART_Receive_IT(&huart2, Rx2_data,1);
		}
	}


void SystemClock_Config(void)
{

  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = 16;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }

    /**Configure the Systick interrupt time 
    */
  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

    /**Configure the Systick 
    */
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

static void MX_IWDG_Init(void)
{

  hiwdg.Instance = IWDG;
  hiwdg.Init.Prescaler = IWDG_PRESCALER_4;
  hiwdg.Init.Reload = 4095;
  if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
  {
    Error_Handler();
  }

}

static void MX_USART1_UART_Init(void)
{

  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }

}

/* USART2 init function */
static void MX_USART2_UART_Init(void)
{

  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }

}

static void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct;

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);

  /*Configure GPIO pin : PB0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void Error_Handler(void)
{
  while(1) 
  {
  }
}

#ifdef USE_FULL_ASSERT

/**
   * @brief Reports the name of the source file and the source line number
   * where the assert_param error has occurred.
   * @param file: pointer to the source file name
   * @param line: assert_param error line source number
   * @retval None
   */
void assert_failed(uint8_t* file, uint32_t line)
{

}

#endif

/**
  * @}
  */ 

/**
  * @}
*/ 

