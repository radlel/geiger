#include "Arduino.h"
#include "Nextion.h"
#include "SoftwareSerial.h"


#define DET 2
#define BUT 3
#define bluetoothP 5
#define PIR 6
#define POW 10
#define ALARM_PIN 9
#define CONV_GND 18
#define CONV_VCC 19
#define CEL A0

#define TB_S 6
#define CPMtouSvpH 0.0057
#define COUNTtouSv (CPMtouSvpH / 60)
#define TON 1150
#define TOF 3000


enum alarm_source {
  BT_CON_BROKEN,
  LOW_BATTERY,
  HIGH_CURR_RADIATION
  HIGH_TOTAL_RADIATION
};


volatile unsigned short ssc = 0;
unsigned short tab[TB_S] = {0};
unsigned short t_i = 0;
unsigned long tt;
unsigned long t_sec = 0;
unsigned long CPM = 0;
unsigned long uSv = 0;
unsigned short plot_rg = 200;
unsigned long background_val = 0;
unsigned long source_val = 0;
long measure_result = -1;
String flag_name = "";
SoftwareSerial bluetoothS (4, 7);

NexProgressBar ncl (0, 8, "j2");
NexVariable r1 (0, 22, "R1");
NexVariable r2 (0, 23, "R2");
NexVariable r3 (0, 24, "R3");
NexVariable r4 (0, 25, "R4");
NexVariable r5 (0, 26, "R5");
NexVariable r6 (0, 27, "R6");
NexVariable y1 (0, 36, "y1");
NexVariable y2 (0, 37, "y2");
NexVariable t4 (0, 54, "t4");
NexVariable t5 (0, 54, "t5");
NexVariable nex_flag (0, 56, "flag_str");
NexVariable touch (0, 38, "touch");
NexVariable help (0, 49, "help");

boolean start_patr_2 = true;
boolean bluetooth = false;
boolean pair = false;
boolean bt_finish = false;
boolean rem = false;
boolean background = false;
boolean measure = false;
boolean reset = false;
boolean alarm = false;
boolean usv_range_end = false;

volatile boolean button = false;


void setup() {
	pinMode(BUT, INPUT);
	pinMode(POW, OUTPUT);
	pinMode(bluetoothP, OUTPUT);
	pinMode(PIR, INPUT);
	pinMode(ALARM_PIN, OUTPUT);

	pinMode(CONV_GND, OUTPUT);
	pinMode(CONV_VCC, OUTPUT);

	attachInterrupt(0, excDet, RISING);
	attachInterrupt(1, excBut, RISING);
	nexInit();
	bluetoothS.begin(9600);

	delay(TON);
	digitalWrite(POW, HIGH);
	digitalWrite(CONV_GND, LOW);
	digitalWrite(CONV_VCC, HIGH);
	start_p1();
}

void loop() {
	check_nex_touch();
	if(!reset) proc_and_print_results();
	if(bluetooth) bluetooth_send();
	if(bluetooth) bluetooth_read();
	if(pair) if_paired();
	if(measure) measure();
	if(button) check_button();
	set_cell_level();
	if(alarm) alarm_sound();
	if(reset) reset();
}


void excDet() { ssc ++; }

void excBut() { if(t_sec > 0 && !reset) button = true; }


void start_p1() {
	tt = millis();
	while(t_i < TB_S - 1) {
		if(millis() - tt >= 1000) {
			tab[t_i] = ssc;
			ssc = 0;
			tt = millis();
			t_i ++;
			if(t_i == 1) ncl.setValue(map(no_bigger_than(analogRead(CEL), 860), 512, 860, 0, 100));
		}
	}
	tt = millis();
}

void start_p2() {
	sendCommand("sys0=1");
	sendCommand("tsw 255,1");
	sendCommand("plot.en=1");
	sendCommand("p2.pic=56");
	start_patr_2 = false;
}

void reset() {
	for(int i = 0; i < TB_S; i++) tab[i] = 0;
	ssc = 0;
	t_i = 0;
	t_sec = 0;
	CPM = 0;
	uSv = 0;
	plot_rg = 200;
	background_val = 0;
	source_val = 0;
	measure_result = -1;
	start_patr_2 = true;
	bluetooth = false;
	digitalWrite(bluetoothP, LOW);
	pair = false;
	rem = false;
	background = false;
	measure = false;

	start_p1();
	reset = false;
}

void check_nex_touch() {
	static unsigned long ttl = millis();
	char flag_buf[31] = {0};

	if(millis() - ttl >= 100) {
		ttl = millis();
		uint32_t n;
		touch.getValue(&n);

		switch(n) {
		case 0:		
			if(measure_result != -1) measure_result = -1;
			break;
		case 1:		
			rem = true;
			if(background_val > 0) proc_and_set(background_val, t4, "t4");
			if(source_val > 0 && source_val > background_val && background) proc_and_set(source_val - background_val, t5, "t5");
			else if(source_val > 0 && source_val <= background_val && background) t5.setText("0.00");
			else if(source_val > 0 && !background) proc_and_set(source_val, t5, "t5");
			sendCommand("click func,1");
			break;
		case 2:		
			rem = false;
			if(background_val > 0) proc_and_set(background_val, t4, "t4");
			if(source_val > 0 && source_val > background_val && background) proc_and_set(source_val - background_val, t5, "t5");
			else if(source_val > 0 && source_val <= background_val && background) t5.setText("0.00");
			else if(source_val > 0 && !background) proc_and_set(source_val, t5, "t5");
			sendCommand("click func,1");
			break;
		case 9:		
			reset = true;
			sendCommand("rest");
			break;
		case 11:	
			measurement(&background_val, n);
			break;
		case 12:	
			measurement(&source_val, n);
			break;
		case 18:	
			background = true;
			if(source_val > 0 && source_val > background_val) proc_and_set(source_val - background_val, t5, "t5");
			else if(source_val > 0 && source_val <= background_val) t5.setText("0.00");
			sendCommand("click func,1");
			break;
		case 19:	
			background = false;
			if(source_val > 0) proc_and_set(source_val, t5, "t5");
			sendCommand("click func,1");
			break;
		case 31:	
			digitalWrite(bluetoothP, HIGH);
			pair = true;
			sendCommand("click func,1");
			break;
		case 32:	
			bt_finish = true;
			digitalWrite(bluetoothP, LOW);
			bluetooth = false;
			sendCommand("click func,1");
			break;
		case 33:	
			bluetooth = false;
			pair = false;
			digitalWrite(bluetoothP, LOW);
			sendCommand("help.val=32");
			sendCommand("click func,1");
			break;
		case 41:	
			memset(flag_buf, 0, sizeof(flag_buf));
			nex_flag.getText(flag_buf, sizeof(flag_buf));
			flag_name = String(flag_buf);
			if(flag_name != "") sendCommand("click func,1");
			break;
		case 100:	
			plot_rg = 100;
			sendCommand("click func,1");
			break;
		case 200:
			plot_rg = 200;
			sendCommand("click func,1");
			break;
		case 500:
			plot_rg = 500;
			sendCommand("click func,1");
			break;
		case 1000:
			plot_rg = 1000;
			sendCommand("click func,1");
			break;
		case 2000:
			plot_rg = 2000;
			sendCommand("click func,1");
			break;
		case 5000:
			plot_rg = 5000;
			sendCommand("click func,1");
			break;
		case 10000:
			plot_rg = 10000;
			sendCommand("click func,1");
			break;
		}
	}
}

void proc_and_print_results() {
	if(millis() - tt >= 1000) {
		unsigned short ssc_t;

		tab[t_i] = ssc_t = ssc;
		ssc = 0;
		tt = millis();
		if(t_i < TB_S - 1) t_i ++;
		else t_i = 0;

		CPM = 0;
		for(int i = 0; i < TB_S; i++) CPM += tab[i];
		CPM *= (60 / TB_S);
		long dif = 0;
		if(background) {
			dif = CPM - background_val;
			if(dif < 0) dif = 0;
			r2.setValue(dif);
		}
		else r2.setValue(CPM);

		if(background) proc_and_set(background_val >= CPM ? 0 : CPM - background_val, r1, "r1");
		else proc_and_set(CPM, r1, "r1");

		static boolean too_big_uSvph = false;
		if(CPM * CPMtouSvpH >= 1.17 && !too_big_uSvph) {
			alarm_show(3);
			too_big_uSvph = true;
		}

		if(too_big_uSvph && CPM * CPMtouSvpH < 1.17) {
			sendCommand("A3.val=2");
			too_big_uSvph = false;
		}

		if(!usv_range_end) {
			uSv += ssc_t;
			proc_and_set(uSv, r3, "r3");
		}

		static boolean too_big_uSv = false;
		if(uSv * COUNTtouSv >= 9.32  && !too_big_uSv) {
			too_big_uSv = true;
			alarm_show(4);
		}

		r4.setValue(t_sec);
		t_sec ++;

		if(CPM * CPMtouSvpH <= 0.39) r5.setValue(int(map_f(CPM * CPMtouSvpH, 0.0, 0.39, 0, 44)));
		else 			r5.setValue(int(map_f(no_bigger_than(CPM * CPMtouSvpH, 5.0), 0.39, 5.0, 44, 100)));

		if(uSv * COUNTtouSv <= 9.32) r6.setValue(int(map_f(uSv * COUNTtouSv, 0, 9.32, 0, 61)));
		else 		    r6.setValue(int(map_f(no_bigger_than_f(uSv * COUNTtouSv, 27.96), 9.32, 27.96, 61, 100)));

		static boolean ch = true;
		if(ch) y1.setValue(map(no_bigger_than(CPM, plot_rg), 0, plot_rg, 1, 80));
		else   y2.setValue(map(no_bigger_than(CPM, plot_rg), 0, plot_rg, 1, 80));
		ch = !ch;

		if(start_patr_2) start_p2();
	}
}

void proc_and_set(unsigned long value, NexVariable nt, String nex_name) {
	char buf[10] = {0};
	String str = "";

	if(nex_name == "r1") {
		if (!rem || (rem && value * CPMtouSvpH < 100.0)) str = String(double(value * CPMtouSvpH * (rem ? 100 : 1)));
		else str = String(long(value * CPMtouSvpH * 100 + 0.5));
	} else if(nex_name == "r3") {
		if(uSv * COUNTtouSv >= 100000.0) {
			str = ">100mSv";
			usv_range_end = true;
		} else if(rem && value * COUNTtouSv >= 1000.0) str = String(long(value * COUNTtouSv * 100 + 0.5));
		else str = String(double(value * COUNTtouSv * (rem ? 100 : 1)));
	} else if(nex_name == "t4" || nex_name == "t5") {
		str = String(double(value * CPMtouSvpH * (rem ? 100 : 1)));
	}

	str.toCharArray(buf, sizeof(buf), 0);
	nt.setText(buf);
}

void measure() {
	static boolean start = true;
	static unsigned long temp = 0;
	static unsigned long ttl1, ttl2;
	if(start) {
		ttl1 = ttl2 = t_sec;
		start = false;
		temp = 0;
	}

	if(t_sec - ttl1 == TB_S ) {
		for(int i = 0; i < TB_S; i++) temp += tab[i];
		ttl1 = t_sec;
	}

	if(t_sec - ttl2 >= 600) {
		measure_result = temp / 10;
		measure = false;
		start = true;
	}
}

void measurement(unsigned long *v, uint32_t n) {
	if(measure == false && measure_result == -1) measure = true;
	if(measure_result > -1) {
		*v = measure_result;
		sendCommand("click func,1");
		if	    (n == 12 && source_val > background_val && background) proc_and_set(source_val - background_val, t5, "t5");
		else if (n == 12 && source_val <= background_val && background) t5.setText("0.00");
		else if (n == 12 && !background) proc_and_set(source_val, t5, "t5");
		else if (n == 11) {
			proc_and_set(background_val, t4, "t4");
			if(background && source_val > 0 && source_val > background_val) proc_and_set(source_val - background_val, t5, "t5");
			else if(background && source_val > 0 && source_val <= background_val) t5.setText("0.00");
		}
		measure_result = -2;
	}
}

void bluetooth_send() {
	static unsigned long temp = 0;
	if(temp != t_sec) {
		bluetoothS.println(String(t_sec - 1) + "," + String(t_i > 0 ? tab[t_i - 1] : tab[TB_S - 1]) + "," + flag_name + ",");
		temp = t_sec;
		if(flag_name != "") flag_name = "";
	}
}

void if_paired() {
	static boolean paired = false;
	if(digitalRead(PIR) && !paired) {
		sendCommand("help.val=5555");
		sendCommand("click func,1");
		bluetooth = true;
		paired = true;
	}
	if(!digitalRead(PIR) && paired) {
		sendCommand("help.val=32");
		sendCommand("click func,1");
		digitalWrite(bluetoothP, LOW);
		bluetooth = false;
		pair = false;

		if(!bt_finish) alarm_show(1);
		else bt_finish = false;

		paired = false;
	}
}

void bluetooth_read() { if (bluetoothS.available() > 0 && bluetoothS.read() == 'x') bt_finish = true; }

void set_cell_level() {
	static boolean setCL = true;
	if(t_sec % 60 == 0 && setCL) {
		short temp = map(no_bigger_than(analogRead(CEL), 860), 512, 860, 0, 100);
		ncl.setValue(temp);
		setCL = false;

		static bool under_3V = false;
		if(temp <= 30 && !under_3V) {
			alarm_show(2);
			under_3V = true;
		}
		if(under_3V && temp > 30) {
			sendCommand("j2.ppic=12");
			under_3V = false;
		}
	}
	if(t_sec % 60 != 0) setCL = true;
}

void check_button() {
	static unsigned long ttl = 0;
	if(!digitalRead(BUT)) {
		sendCommand("help.val=4321");
		sendCommand("click func,1");
		button = false;
		ttl = 0;
	} else {
		if(millis() - ttl >= TOF + 2000) ttl = millis();
		if(millis() - ttl >= TOF) {
			digitalWrite(POW, LOW);
			digitalWrite(CONV_VCC, LOW);
		}
	}
}

void alarm_sound() {
	static unsigned long ttl = 0;
	if(millis() - ttl >= 3500) ttl = millis();

	if(millis() - ttl <= 800) digitalWrite(ALARM_PIN, HIGH);
	else if(millis() - ttl > 800 && millis() - ttl <= 1600) digitalWrite(ALARM_PIN, LOW);
	else if(millis() - ttl > 1600 && millis() - ttl <= 2400) digitalWrite(ALARM_PIN, HIGH);

	if(millis() - ttl >= 2400) {
		digitalWrite(ALARM_PIN, LOW);
		alarm = false;
	}
}

void alarm_show(short n) {
	switch(n) {
	case BT_CON_BROKEN:
		sendCommand("A1.val=1");
		break;
	case LOW_BATTERY:
		sendCommand("j2.ppic=71");
		break;
	case HIGH_CURR_RADIATION:
		sendCommand("A3.val=1");
		break;
	case HIGH_TOTAL_RADIATION:
		sendCommand("A4.val=1");
		break;
	}
	alarm = true;
}

double map_f(double v, double in_min, double in_max, double out_min, double out_max) { return (v - in_min) * (out_max - out_min) / (in_max - in_min) + out_min; }

unsigned short no_bigger_than(unsigned short val, unsigned short max) { return ((val > max) ? max : val); }

double no_bigger_than_f(double val, double max) { return ((val > max) ? max : val); }
