#include "fuc.h"
#include "rtc.h"
#include "usart.h"
#include "gpio.h"
#include <lcd.h>

extern const unsigned char _imgArr[28800];
uint8_t img_number=0;
uint8_t img_count=0;
_Bool isImgUpdated =0;
char lcd_buffer[20]={0};
uint8_t key_down,key_up,key_value,key_old;
RTC_TimeTypeDef sTime;
RTC_DateTypeDef sDate;
RTC_AlarmTypeDef sAlarm={0};
uint8_t curr_page=0;uint8_t old_page=0;
uint8_t img_direction=0;
_Bool img_direct_old=0;
_Bool waitForDMA=0;
uint8_t alarmming = 0;
uint16_t btn_uwtick=0;
_Bool rxFinished =1;

uint8_t btn_index=0;//测试用
RTC_TimeTypeDef  H_S_M_Time;//不用这个的话会有bug
RTC_DateTypeDef  Y_M_D_Data;


uint8_t alarm_h = 23,alarm_m = 30,alarm_s = 20,cursor = 0;//cursor设置闹钟用的下标 0:h; 1:min; 2:sec;
uint8_t setAlarmed = 0;

#define Scheduled ;

void main_proc(){
	key_proc();
	rtc_proc();
	lcd_proc();
	led_proc();
}


void led_show(uint8_t index,int opeator){//1开0关
  HAL_GPIO_WritePin(GPIOD,GPIO_PIN_2,GPIO_PIN_SET);
  if(opeator==1){
    HAL_GPIO_WritePin(GPIOC,GPIO_PIN_8<<(index-1),GPIO_PIN_RESET);
  }else{
    HAL_GPIO_WritePin(GPIOC,GPIO_PIN_8<<(index-1),GPIO_PIN_SET);
  }
  HAL_GPIO_WritePin(GPIOD,GPIO_PIN_2,GPIO_PIN_RESET);
}

void fleshLed(){
	HAL_GPIO_WritePin(GPIOD,GPIO_PIN_2,GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOC,GPIO_PIN_All,GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOD,GPIO_PIN_2,GPIO_PIN_RESET);
}
uint8_t key_scan()
{
	if(HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_0)==GPIO_PIN_RESET)
		return 1;
	else if(HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_1)==GPIO_PIN_RESET)
		return 2;
	else if(HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_2)==GPIO_PIN_RESET)
		return 3;
	else if(HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_0)==GPIO_PIN_RESET)
		return 4;
	return 0;
}


void dma_proc(){
	Scheduled//基于串口的背景图片更换功能
}

	
void setALarm(){
	sAlarm.AlarmTime.Hours = alarm_h;
  sAlarm.AlarmTime.Minutes = alarm_m;
  sAlarm.AlarmTime.Seconds = alarm_s;
  sAlarm.AlarmTime.SubSeconds = 0;
  sAlarm.AlarmMask = RTC_ALARMMASK_DATEWEEKDAY;
  sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;
  sAlarm.AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_DATE;
  sAlarm.AlarmDateWeekDay = 1;
  sAlarm.Alarm = RTC_ALARM_A;
  if (HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, RTC_FORMAT_BIN) != HAL_OK)
  {
    Error_Handler();
  }
}

_Bool key4_pressed=0;
uint16_t key4_tick=0;

void key_proc(){
	if(uwTick - btn_uwtick<=100) return ;
	btn_uwtick = uwTick;
	key_value = key_scan();
	key_down = key_value&(key_value^key_old);
	key_up = ~key_value&(key_value^key_old);
	key_old = key_value;
	switch(key_down){
		case 1:
			btn_index=1;
			if(curr_page==0){//控制游标
				cursor++;
				cursor%=3;
			}
			break;
		case 2://设置闹钟时间
			btn_index=2;
			if(curr_page==0){
				switch(cursor){
					case 0:alarm_h++;alarm_h%=24;break;
					case 1:alarm_m++;alarm_m%=60;break;
					case 2:alarm_s++;;alarm_s%=60;break;
				}
			}
			break;
		case 3:
			btn_index=3;
			if(curr_page==1){//控制图片显示方向
				img_direction++;
				isImgUpdated = 0;
				img_direction%=2;//0=Horizontal,1=Vertical
			}else{
				setAlarmed++;//启用/关闭闹钟功能
				setAlarmed%=2;
				setALarm();
			}
			break;
		case 4:
			alarmming = 0;//关闭闹钟
			key4_pressed=1;
			key4_tick=uwTick;//长按相关
			led_show(8,0);
			break;
	}
	switch(key_up){//检测长按
		case 4:
			if(key4_pressed&&uwTick - key4_tick>1000){
				btn_index=4;
				curr_page++;
				curr_page%=2;//0为起始页，1为展示页	
				break;
			}else key4_pressed=0;
	}
}


void rtc_proc(){//更新rtc时钟
	HAL_RTC_GetTime(&hrtc,&H_S_M_Time,RTC_FORMAT_BIN);
	HAL_RTC_GetDate(&hrtc,&Y_M_D_Data,RTC_FORMAT_BIN);
}
	

void LCD_showHPic(const uint8_t* pic_arr, uint8_t pic_x, uint8_t pic_y) {//水平显示图片
  LCD_SetCursor((240-pic_y)/2,(320-pic_x)/2); 
  LCD_WriteRAM_Prepare();
  
  for(int i=0;i<(pic_x*pic_y*2);i+=2){
    if(i%(pic_x*2)==0){
      for(uint8_t flesh=0;flesh<(320-pic_x);flesh++)
        LCD_WriteRAM((uint16_t)0x00000);
    }
    LCD_WriteRAM(pic_arr[i+1]<<8|pic_arr[i]);
  }
  isImgUpdated=1;
}


void LCD_showVPic(const uint8_t* pic_arr, uint8_t pic_x, uint8_t pic_y) {//垂直显示图片
  uint16_t start_x=(240-pic_x)/2,start_y=0;
  LCD_SetCursor(start_x,start_y); 
  LCD_WriteRAM_Prepare();
  
  for(uint16_t y=0;y<pic_x;y++){
    for(uint16_t x=0;x<(320-pic_y)/2;x++){
      LCD_WriteRAM(0x000000);
    }//填充左半边
    for(uint16_t x=0;x<pic_y;x++){
      uint16_t offset =(pic_x*x+y) * 2;
      LCD_WriteRAM(pic_arr[offset+1]<<8|pic_arr[offset]);
    }
    for(uint16_t x=0;x<(320-pic_y)/2;x++){
      LCD_WriteRAM(0x000000);
    }//填充右半边
  }
  isImgUpdated=1;
}
void alarm_page1_fuc(){//倒计时计算
	uint16_t CurrSec = sTime.Hours * 3600 + sTime.Minutes * 60 + sTime.Seconds;
	uint16_t alarmSec =  sAlarm.AlarmTime.Hours * 3600 + sAlarm.AlarmTime.Minutes * 60 + sAlarm.AlarmTime.Seconds;
	
	if(alarmSec < CurrSec){
		alarmSec += 24*3600;
	}
	uint16_t leftSec = alarmSec - CurrSec;
	if(alarmming){
		sprintf(lcd_buffer,"      TimeOut!                ");
	}else{
		sprintf(lcd_buffer,"Hour:%dMin:%dSec:%d     ",leftSec/3600,(leftSec%3600)/60,leftSec%60);
	}
}
void lcd_proc(){
	if(old_page!=curr_page||((img_direct_old != img_direction) && curr_page == 1)){//切屏时刷新屏幕缓冲区
		LCD_Clear(Black);
	}
	old_page = curr_page;
	img_direct_old=img_direction;
	
	HAL_RTC_GetTime(&hrtc,&sTime,RTC_FORMAT_BIN);
	HAL_RTC_GetDate(&hrtc,&sDate,RTC_FORMAT_BIN);
	if(curr_page){
		if(!isImgUpdated){//只渲染一次图片以减少性能开销
			if(img_direction){
				LCD_showHPic(_imgArr,120,120);
			}else{
				LCD_showVPic(_imgArr,120,120);
			}
		}
		if(setAlarmed||alarmming){
			alarm_page1_fuc();
		}else
			sprintf(lcd_buffer,"   Plz set alarm      ");
		LCD_DisplayStringLine(Line9,(u8*)lcd_buffer);
		sprintf(lcd_buffer,"       H/V:%d                ",img_direction);
		LCD_DisplayStringLine(Line0,(u8*)lcd_buffer);
		fleshLed();//修复清屏造成的led引脚冲突
	}else{
		sprintf(lcd_buffer,"      HomePage               ");
		LCD_DisplayStringLine(Line0,(uint8_t *)lcd_buffer);
		sprintf(lcd_buffer,"    Date:%d-%d-%d              ",sDate.Year+2000,sDate.Month,sDate.Date);
		LCD_DisplayStringLine(Line1,(u8*)lcd_buffer);
		sprintf(lcd_buffer,"    Time:%2d:%2d:%2d            ",sTime.Hours,sTime.Minutes,sTime.Seconds);
		LCD_DisplayStringLine(Line2,(u8*)lcd_buffer);
		sprintf(lcd_buffer,"    Alarm:%d:%d:%d            ",sAlarm.AlarmTime.Hours,sAlarm.AlarmTime.Minutes,sAlarm.AlarmTime.Seconds);
		LCD_DisplayStringLine(Line3,(u8*)lcd_buffer);
		sprintf(lcd_buffer,"    Set:%d:%d:%d            ",alarm_h,alarm_m,alarm_s);
		LCD_DisplayStringLine(Line4,(u8*)lcd_buffer);
		sprintf(lcd_buffer,"    Cursor:%d            ",cursor+1);
		LCD_DisplayStringLine(Line5,(u8*)lcd_buffer);
	}
}

uint16_t blink_tick=0;
void led_proc(){
	if(alarmming){
		if(uwTick-blink_tick>=500){
			blink_tick = uwTick;
			led_show(8,0);
		}else
			led_show(8,1);
	}
	if(setAlarmed){
		led_show(1,1);
		HAL_NVIC_SetPriority(RTC_Alarm_IRQn, 0, 0);
		HAL_NVIC_EnableIRQ(RTC_Alarm_IRQn);
	}else {
		HAL_NVIC_DisableIRQ(RTC_Alarm_IRQn);
		led_show(1,0);
	}
}


void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc){
	alarmming = 1;
	setAlarmed = 0;
}