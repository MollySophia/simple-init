#include<time.h>
#include"hardware.h"
#include"lvgl.h"
#include"gui.h"
#include"sysbar.h"
struct sysbar sysbar;

static void sysbar_thread(struct sysbar*b){
	int bats[64]={0},bat,lvl;
	bat=pwr_scan_device(bats,63,true);
	char timestr[64]={0},*sym=NULL;
	time_t t=time(NULL);
	struct tm*tt=localtime(&t);
	if(tt&&strftime(timestr,63,"%H:%M",tt)>0)
		lv_label_set_text(b->top.content.time,timestr);
	if(bat>=0&&(lvl=pwr_multi_get_capacity(bats))>=0){
		if(lvl<10)sym=LV_SYMBOL_BATTERY_EMPTY;
		else if(lvl<25)sym=LV_SYMBOL_BATTERY_1;
		else if(lvl<45)sym=LV_SYMBOL_BATTERY_2;
		else if(lvl<65)sym=LV_SYMBOL_BATTERY_3;
		else if(lvl>85)sym=LV_SYMBOL_BATTERY_FULL;
		lv_label_set_text_fmt(b->top.content.level,"%d%%",lvl);
		lv_label_set_text(b->top.content.battery,sym);
	}
}

static void sysbar_thread_cb(lv_task_t*a){
	sysbar_thread((struct sysbar*)a->user_data);
}

static void set_bar_style(lv_obj_t*obj,uint8_t part){
	static lv_style_t bar;
	static bool initialized;
	if(!initialized){
		lv_style_init(&bar);
		lv_style_set_bg_color(&bar,LV_STATE_DEFAULT,LV_COLOR_SILVER);
	}
	lv_theme_apply(obj,LV_THEME_SCR);
	lv_obj_add_style(obj,part,&bar);
}

static void keyboard_toggle(lv_obj_t*obj,lv_event_t e){
	if(obj==sysbar.bottom.content.keyboard&&e!=LV_EVENT_CLICKED)return;
	if(obj==sysbar.keyboard&&e!=LV_EVENT_CANCEL){
		lv_keyboard_def_event_cb(obj,e);
		return;
	}
	if(sysbar.keyboard){
		lv_obj_del(sysbar.keyboard);
		sysbar.keyboard=NULL;
		return;
	}
	int w=gui_w,h=gui_h/3;
	sysbar.keyboard=lv_keyboard_create(sysbar.screen,NULL);
	lv_obj_set_size(sysbar.keyboard,w,h);
	lv_obj_set_y(sysbar.keyboard,gui_h-sysbar.size-h);
	lv_obj_set_event_cb(sysbar.keyboard,keyboard_toggle);
}

static lv_obj_t*draw_bottom_button(char*symbol,lv_coord_t x,lv_event_cb_t cb){
	int bi=gui_dpi/200;
	if(!sysbar.bottom.style_inited){
		sysbar.bottom.style_inited=true;
		lv_style_init(&sysbar.bottom.btn_style);
		lv_style_set_outline_width(&sysbar.bottom.btn_style,LV_STATE_DEFAULT,bi);
		lv_style_set_border_width(&sysbar.bottom.btn_style,LV_STATE_DEFAULT,0);
		lv_style_set_margin_all(&sysbar.bottom.btn_style,LV_STATE_DEFAULT,0);
		lv_style_set_pad_all(&sysbar.bottom.btn_style,LV_STATE_DEFAULT,0);
		lv_style_set_radius(&sysbar.bottom.btn_style,LV_STATE_DEFAULT,8);
		lv_style_set_bg_color(&sysbar.bottom.btn_style,LV_STATE_DEFAULT,LV_COLOR_SILVER);
	}
	lv_obj_t*btn=lv_btn_create(sysbar.bottom.bar,NULL);
	lv_obj_t*text=lv_label_create(btn,NULL);
	lv_label_set_text(text,symbol);
	lv_obj_add_style(btn,LV_BTN_PART_MAIN,&sysbar.bottom.btn_style);
	lv_obj_set_size(btn,sysbar.size*2,sysbar.size-bi);
	lv_obj_align(btn,NULL,LV_ALIGN_IN_TOP_MID,x,bi);
	lv_group_add_obj(gui_grp,btn);
	if(cb)lv_obj_set_event_cb(btn,cb);
	return btn;
}

static void sysbar_draw_bottom(){
	sysbar.bottom.bar=lv_obj_create(sysbar.screen,NULL);
	lv_obj_set_size(sysbar.bottom.bar,gui_w,sysbar.size);
	lv_obj_align(sysbar.bottom.bar,NULL,LV_ALIGN_IN_BOTTOM_LEFT,0,0);
	set_bar_style(sysbar.bottom.bar,LV_OBJ_PART_MAIN);

	int bm=gui_dpi/20;
	sysbar.bottom.content.back=draw_bottom_button(LV_SYMBOL_LEFT,-(sysbar.size*3+(bm*2)),NULL);
	sysbar.bottom.content.home=draw_bottom_button(LV_SYMBOL_HOME,-(sysbar.size+bm),NULL);
	sysbar.bottom.content.keyboard=draw_bottom_button(LV_SYMBOL_KEYBOARD,sysbar.size+bm,keyboard_toggle);
	sysbar.bottom.content.power=draw_bottom_button(LV_SYMBOL_POWER,sysbar.size*3+(bm*2),NULL);
}

static void sysbar_draw_top(){
	int x=gui_dpi/10,y=gui_dpi/20;
	sysbar.top.bar=lv_obj_create(sysbar.screen,NULL);
	lv_obj_set_size(sysbar.top.bar,gui_w,sysbar.size);
	lv_obj_align(sysbar.top.bar,NULL,LV_ALIGN_IN_TOP_LEFT,0,0);
	set_bar_style(sysbar.top.bar,LV_OBJ_PART_MAIN);

	sysbar.top.content.time=lv_label_create(sysbar.top.bar,NULL);
	lv_label_set_text(sysbar.top.content.time,"00:00");
	lv_obj_align(sysbar.top.content.time,NULL,LV_ALIGN_IN_TOP_LEFT,x,y);

	sysbar.top.content.level=lv_label_create(sysbar.top.bar,NULL);
	lv_label_set_text(sysbar.top.content.level,"000%");
	lv_obj_align(sysbar.top.content.level,NULL,LV_ALIGN_IN_TOP_RIGHT,-x,y);

	sysbar.top.content.battery=lv_label_create(sysbar.top.bar,NULL);
	lv_label_set_text(sysbar.top.content.battery,LV_SYMBOL_BATTERY_EMPTY);
	lv_obj_align(
		sysbar.top.content.battery,NULL,LV_ALIGN_IN_TOP_RIGHT,
		-gui_dpi/5-lv_obj_get_width(sysbar.top.content.level),
		y
	);
}

int sysbar_draw(lv_obj_t*scr){
	sysbar.screen=scr;
	sysbar.size=24+(gui_dpi/10);
	gui_sh=gui_h-(sysbar.size*2);
	sysbar_draw_top();
	sysbar_draw_bottom();
	sysbar_thread(&sysbar);

	sysbar.content=lv_obj_create(scr,NULL);
	lv_obj_set_size(sysbar.content,gui_sw,gui_sh);
	lv_obj_set_pos(sysbar.content,0,sysbar.size);
	lv_theme_apply(sysbar.content,LV_THEME_SCR);

	lv_task_create(sysbar_thread_cb,5000,LV_TASK_PRIO_LOW,&sysbar);
	return 0;
}
