#include<reg52.h> 
#include<intrins.h>
sbit SCK=P1^4; //DS1302 sclk
sbit SDA=P1^5; //Ds1302 data
sbit RST=P1^6; //Ds1302 reset
sbit fontcs = P2^2; //字型
sbit digitcs = P2^3; //位數
sbit SPK = P1^2; //speaker
#define RST_CLR RST=0 //reset 0
#define RST_SET RST=1 //reset 1
#define IO_CLR SDA=0 //data 0
#define IO_SET SDA=1 //data 1
#define IO_R SDA 
#define SCK_CLR SCK=0 //sclk 0
#define SCK_SET SCK=1 //sclk 1
#define ds1302_sec_add 0x80 //sec
#define ds1302_min_add 0x82 //min
#define ds1302_hr_add 0x84 //hour
#define ds1302_date_add 0x86 //date
#define ds1302_month_add 0x88 //month
#define ds1302_day_add 0x8a //weekday
#define ds1302_year_add 0x8c //year
#define ds1302_control_add 0x8e //write protect
#define ds1302_charger_add 0x90 //charger
#define ds1302_clkburst_add 0xbe
#define ms50 65536-46080 // 11.0592Mhz
#define DataPort P0 //data port
#define KeyPort P3 //keyport
#define MAX 20
#define nxt_head head++; if (head == MAX) head = 0;
typedef unsigned char byte;
typedef unsigned int word;
unsigned int t1_cnt; //timer1 counter
unsigned char t_buf1[8],t_buf2[8]={20,22,6,16,16,59,50,2},t_buf3[16];//2022/6/16 16:59:50 星期二
unsigned char alarm_time[8], hour, min, sec;
unsigned char code test_output[5] = {"HELLO"};
unsigned char code font[10]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};//共陰字型
unsigned char code digit[8]={0xfe,0xfd,0xfb,0xf7,0xef,0xdf,0xbf,0x7f};//位數
unsigned char TempData[8]; //七段顯示器
unsigned long High,Low,Time;//freq & len
unsigned int song_cnt, song_acc;
//音階頻率表的高 8bit
unsigned char code FREQH[]={
 	0xF2,0xF3,0xF5,0xF5,0xF6,0xF7,0xF8, 
	0xF9,0xF9,0xFA,0xFA,0xFB,0xFB,0xFC,
	0xFC,0xFC,0xFD,0xFD,0xFD,0xFD,0xFE,
	0xFE,0xFE,0xFE,0xFE,0xFE,0xFE,0xFF,
};
//音階頻率表的低 8bit 
unsigned char code FREQL[]={
 	0x42,0xC1,0x17,0xB6,0xD0,0xD1,0xB6,
 	0x21,0xE1,0x8C,0xD8,0x68,0xE9,0x5B,
 	0x8F,0xEE,0x44,0x6B,0xB4,0xF4,0x2D, 
	0x47,0x77,0xA2,0xB6,0xDA,0xFA,0x16,
};
code unsigned char MUSIC[]={ 6,2,3, 5,2,1, 3,2,2, 5,2,2, 
	1,3,2, 6,2,1, 5,2,1,
 	6,2,4, 3,2,2, 5,2,1, 6,2,1, 
	5,2,2, 3,2,2, 1,2,1,
 	6,1,1, 5,2,1, 3,2,1, 2,2,4, 
	2,2,3, 3,2,1, 5,2,2,
 	5,2,1, 6,2,1, 3,2,2, 2,2,2, 
	1,2,4, 5,2,3, 3,2,1,
 	2,2,1, 1,2,1, 6,1,1, 1,2,1, 
	5,1,6, 0,0,0 
};
byte input_buf[MAX];
byte tail,head; //queue
bit ReadTimeFlag;
bit SetTimeFlag;
bit set_Alarm;
bit enti;
bit en_sing;
void DelayUs2x(unsigned int t){ 
	while(--t);
}
void DelayMs(unsigned int t){
	while(t--)DelayUs2x(490);//delay 1mS
}
void Display(unsigned char FirstBit,unsigned char Num){
 	static unsigned char i=0; 
 	DataPort=0; //clear
 	fontcs=1; 
 	fontcs=0;
 	DataPort=digit[i+FirstBit]; //write digit
 	digitcs = 1; 
 	digitcs = 0;
 	DataPort=TempData[i]; //write font
 	fontcs=1; 
 	fontcs=0; 
 	if(++i==Num)i=0;
}
void Ds1302_Write_Byte(unsigned char addr, unsigned char d){// addr=IS,d=data
	unsigned char i;
	RST_SET; //reset=1
	addr = addr & 0xFE; //start write
	for (i = 0; i < 8; ++i) { 
		if (addr & 0x01)IO_SET; // data = addr & 0x01
		else IO_CLR; 
		SCK_SET; //re
		SCK_CLR;
		addr = addr >> 1; //nxt
	}
	for (i = 0; i < 8; ++i){ //input data
		if (d & 0x01)IO_SET; // data = d & 0x01
		else IO_CLR;
		SCK_SET; //re
		SCK_CLR;
		d = d >> 1; //nxt
	}
	RST_CLR; //reset=0,stop
}
unsigned char Ds1302_Read_Byte(unsigned char addr) {
	unsigned char i,tmp;
	RST_SET; //reset=1
	addr = addr | 0x01; //start read
	for (i = 0; i < 8; ++i){
		if (addr & 0x01)IO_SET;  // data = addr & 0x01
		else IO_CLR;
		SCK_SET; //re
		SCK_CLR;
		addr = addr >> 1; //nxt
	}
	for (i = 0; i < 8; ++i){
		tmp = tmp >> 1; //nxt
		if (IO_R)tmp |= 0x80; //read->tmp
		else tmp &= 0x7F; 
		SCK_SET; //fe
		SCK_CLR;
	}
	RST_CLR; //reset=0,stop
	return tmp;
}
void Ds1302_Write_Time(void) {
	unsigned char i,tmp;
	for(i=0;i<8;++i){ //DEC->BCD
		tmp=t_buf2[i]/10;
		t_buf1[i]=t_buf2[i]%10;
		t_buf1[i]=t_buf1[i]+tmp*16;
	}
	Ds1302_Write_Byte(ds1302_control_add,0x00); //write protect=0 ,start write
	Ds1302_Write_Byte(ds1302_sec_add,0x80); 
	Ds1302_Write_Byte(ds1302_year_add,t_buf1[1]); 
	Ds1302_Write_Byte(ds1302_month_add,t_buf1[2]);
	Ds1302_Write_Byte(ds1302_date_add,t_buf1[3]); 
	Ds1302_Write_Byte(ds1302_hr_add,t_buf1[4]);
	Ds1302_Write_Byte(ds1302_min_add,t_buf1[5]);
	Ds1302_Write_Byte(ds1302_sec_add,t_buf1[6]);
	Ds1302_Write_Byte(ds1302_day_add,t_buf1[7]);
	Ds1302_Write_Byte(ds1302_control_add,0x80); //write protect=1,stop
}
void Ds1302_Read_Time(void) { 
	unsigned char i;
	t_buf1[1]=Ds1302_Read_Byte(ds1302_year_add); 
	t_buf1[2]=Ds1302_Read_Byte(ds1302_month_add);
	t_buf1[3]=Ds1302_Read_Byte(ds1302_date_add); 
	t_buf1[4]=Ds1302_Read_Byte(ds1302_hr_add); 
	t_buf1[5]=Ds1302_Read_Byte(ds1302_min_add);
	t_buf1[6]=(Ds1302_Read_Byte(ds1302_sec_add))&0x7F;
	t_buf1[7]=Ds1302_Read_Byte(ds1302_day_add);
	for(i=0;i<8;++i){ //BCD->DEC
		t_buf2[i]=t_buf1[i]%16;
		t_buf2[i]=t_buf2[i]+t_buf1[i]/16*10;
	}
}
void Ds1302_Init(void){
	RST_CLR; //RST=0
	SCK_CLR; //SCK=0
 	Ds1302_Write_Byte(ds1302_sec_add,0x00);//sec=0
}
void Init_Timer0(void){
	TMOD |= 0x01; //timer0 mod1
	EA=1;
	ET0=1;
	TR0=1;
}
void uart_init(){
	SCON = 0x50; //mod = 1
	PCON = 0x0; //SMOD = 0;
	TMOD = TMOD | 0x20; //mode 2, timer1 auto reload
	TH1 = 0xfd; //9600, 11.0592MHz
	TR1 = 1; //start count
	RI = 0;
	TI = 0;
	EA = 1; //打開中斷
	ES = 1; //打開 uart 中斷
}
void test_send(){ //test UART
	byte i;
	for(i=0;i<5;++i){
		SBUF = test_output[i];
	while(TI != 1);
		TI = 0;
		enti = 1;
	}
}
unsigned char KeyScan(void){
	unsigned char keyvalue,i;
	if(KeyPort!=0xff){
		DelayMs(10); //解彈跳
		if(KeyPort!=0xff){
		 	keyvalue=KeyPort;
		 	while(KeyPort==0xff);//連續按住判斷放掉
	 		for(i=0;i<8;++i)
	 			if(digit[i] == keyvalue)return i+1;
			return 0;
		}
	}
	return 0;
}
void delay(unsigned char t){
 	unsigned char i;
	for(i=0;i<t;++i)
 		DelayMs(250);
}
void sing_song(unsigned char tp){ //(1) = MOM ;(2) = O1O
	unsigned int i,j,k;
	if(tp == 1){
		for(i=0;i<100;i=i+3){ 
			k=MUSIC[i]+7*MUSIC[i+1]-1;
			High=FREQH[k];
			Low=FREQL[k];
			Time=MUSIC[i+2]*20000; 
			song_acc = 0;
			while(song_acc<Time){
				song_cnt = ((255-High)*256+(255-Low));
				DelayUs2x(song_cnt/2);
				song_acc += song_cnt;
				SPK=!SPK;
			} 
		}
	}
	else if(tp == 2){
		for(j=0;j<5;++j){
			for(i=0;i<100;++i){
				DelayUs2x(400); 
				SPK=!SPK;
			}
			for(i=0;i<100;++i){
				DelayMs(1);
				SPK=!SPK;
			}
		}
		SPK = 0;
	}
} 
void Init_ALL(){//init func
	Init_Timer0(); 
	uart_init(); 
	Ds1302_Init(); 
	test_send();
	SPK = 0; //turn off speaker
}
void main (){
	unsigned char num, idx; 
	Init_ALL();
	while (1){
		num=KeyScan();
		if(ReadTimeFlag==1){ //1 sec/time
			Ds1302_Read_Time();
			ReadTimeFlag=0;
			if(num==0){ //顯示時分秒
				TempData[0]=font[t_buf2[4]/10];
				TempData[1]=font[t_buf2[4]%10];
				TempData[3]=font[t_buf2[5]/10];
				TempData[4]=font[t_buf2[5]%10];
				TempData[6]=font[t_buf2[6]/10];
				TempData[7]=font[t_buf2[6]%10];
			}else if(num==1){ //顯示年月日
				TempData[0]=font[t_buf2[1]/10];
				TempData[1]=font[t_buf2[1]%10];
				TempData[3]=font[t_buf2[2]/10];
				TempData[4]=font[t_buf2[2]%10];
				TempData[6]=font[t_buf2[3]/10];
				TempData[7]=font[t_buf2[3]%10];
			}
			if(num==0||num==1)TempData[2]=TempData[5]=0x40;
		}
		if(head!=tail){ //read data
			if(input_buf[head] == 'X'){//X_16_ , set date
				nxt_head;
				for(idx = 0;idx<16;){
					t_buf3[idx++]=input_buf[head]&0x0F;
					nxt_head;
				}
				for(idx=0;idx<8;++idx)
					t_buf2[idx]=t_buf3[2*idx]*10+t_buf3[2*idx+1];
				Ds1302_Write_Time();
			}
			else if(input_buf[head] == 'Y'){//Y_6_ , set alarm
				nxt_head;
				for(idx = 0;idx<6;){
					alarm_time[idx++]=input_buf[head]&0x0F;
					nxt_head;
				}
				hour = alarm_time[0]*10+alarm_time[1];
				min = alarm_time[2]*10+alarm_time[3];
				sec = alarm_time[4]*10+alarm_time[5];
				set_Alarm = 1;
			}
			nxt_head;//EOF
		}
		if(t_buf2[5]==0&&t_buf2[6]==0)sing_song(1);//o'clock
		if(set_Alarm && hour==t_buf2[4] && min==t_buf2[5] && sec==t_buf2[6]){//alarm
			sing_song(2);
			set_Alarm = 0;
		}
	}
}
void Timer0_isr(void) interrupt 1 {
	static unsigned int time_cnt;
	TH0=(65536-1000)/256; //1ms
	TL0=(65536-1000)%256;
	Display(0,8);
	if(++time_cnt>=100){ //100ms/read time
	 	time_cnt=0;
	 	ReadTimeFlag=1; 
	}
}
void uart_isr() interrupt 4 { //uart interrupt
	byte i;
	if (RI == 1){ //UART -> queue
		RI = 0;
		i = SBUF;
		input_buf[tail++] = i;
		if (tail == MAX) tail = 0;
	}
	if(TI&&enti)TI = 0;
}
