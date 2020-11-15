#include "delay.h"
#include "usart4.h"
#include "stdarg.h"	 	 
#include "stdio.h"	 	 
#include "string.h"	  
#include "timer.h"
//////////////////////////////////////////////////////////////////////////////////	 
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32F4开发板
//串口3驱动代码	   
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//修改日期:2014/8/9
//版本：V1.0
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2009-2019
//All rights reserved									  
////////////////////////////////////////////////////////////////////////////////// 	   

//串口发送缓存区 	
__align(8) u8 USART4_TX_BUF[USART4_MAX_SEND_LEN]; 	//发送缓冲,最大USART3_MAX_SEND_LEN字节
#ifdef USART4_RX_EN   								//如果使能了接收   	  
//串口接收缓存区 	
u8 USART4_RX_BUF[USART4_MAX_RECV_LEN]; 				//接收缓冲,最大USART3_MAX_RECV_LEN个字节.


//通过判断接收连续2个字符之间的时间差不大于100ms来决定是不是一次连续的数据.
//如果2个字符接收间隔超过100ms,则认为不是1次连续数据.也就是超过100ms没有接收到
//任何数据,则表示此次接收完毕.
//接收到的数据状态
//[15]:0,没有接收到数据;1,接收到了一批数据.
//[14:0]:接收到的数据长度
vu16 USART4_RX_STA=0;   	 
void USART4_IRQHandler(void)
{
	u8 res;	    
	if(USART_GetITStatus(UART4, USART_IT_RXNE) != RESET)//接收到数据
	{	 
 
	res =USART_ReceiveData(UART4);		
	if((USART4_RX_STA&(1<<15))==0)//接收完的一批数据,还没有被处理,则不再接收其他数据
	{ 
		if(USART4_RX_STA<USART4_MAX_RECV_LEN)		//还可以接收数据
		{
			TIM_SetCounter(TIM5,0);//计数器清空        				 
			if(USART4_RX_STA==0)		
				TIM_Cmd(TIM5, ENABLE);  //使能定时器7 
			USART4_RX_BUF[USART4_RX_STA++]=res;		//记录接收到的值	 
		}else 
		{
			USART4_RX_STA|=1<<15;					//强制标记接收完成
		} 
	}  	
 }										 
}  
#endif	
//初始化IO 串口3
//bound:波特率	  
void usart4_init(u32 bound)
{  

	NVIC_InitTypeDef NVIC_InitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;

	
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC,ENABLE); //使能GPIOB时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART4,ENABLE);//使能USART3时钟

 	USART_DeInit(UART4);  //复位串口3
	
	GPIO_PinAFConfig(GPIOC,GPIO_PinSource11,GPIO_AF_UART4); //GPIOB11复用为USART3
	GPIO_PinAFConfig(GPIOC,GPIO_PinSource10,GPIO_AF_UART4); //GPIOB10复用为USART3	
	
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11 | GPIO_Pin_10; //GPIOB11和GPIOB10初始化
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;//复用功能
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	//速度50MHz
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP; //推挽复用输出
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP; //上拉
	GPIO_Init(GPIOC,&GPIO_InitStructure); //初始化GPIOB11，和GPIOB10
	
	USART_InitStructure.USART_BaudRate = bound;//波特率 
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;//字长为8位数据格式
	USART_InitStructure.USART_StopBits = USART_StopBits_1;//一个停止位
	USART_InitStructure.USART_Parity = USART_Parity_No;//无奇偶校验位
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;//无硬件数据流控制
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;	//收发模式
  
	USART_Init(UART4, &USART_InitStructure); //初始化串口3
 
	USART_Cmd(UART4, ENABLE);               //使能串口 
	
  USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);//开启中断   
	
	NVIC_InitStructure.NVIC_IRQChannel = UART4_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=0 ;//抢占优先级0
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;		//子优先级0
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			//IRQ通道使能
	NVIC_Init(&NVIC_InitStructure);	//根据指定的参数初始化VIC寄存器
	TIM7_Int_Init(100-1,8400-1);		//100ms中断
	USART4_RX_STA=0;		//清零
	TIM_Cmd(TIM7, DISABLE); //关闭定时器7
  	

}

//串口3,printf 函数
//确保一次发送数据不超过USART3_MAX_SEND_LEN字节
void u4_printf(char* fmt,...)  
{  
	u16 i,j;
	va_list ap;
	va_start(ap,fmt);
	vsprintf((char*)USART4_TX_BUF,fmt,ap);
	va_end(ap);
	i=strlen((const char*)USART4_TX_BUF);//此次发送数据的长度
	for(j=0;j<i;j++)//循环发送数据
	{
		 while(USART_GetFlagStatus(UART4,USART_FLAG_TC)==RESET); //循环发送,直到发送完毕   
		USART_SendData(UART4,USART4_TX_BUF[j]);  
	}
}


 
 











