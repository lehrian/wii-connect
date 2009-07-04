#include <errno.h>
#include <debug.h>
#include <fat.h>
#include <math.h>
#include <network.h>
#include <ogc/lwp_watchdog.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/dir.h>
#include <unistd.h>
#include <wiiuse/wpad.h>


#include "FreeTypeGX.h"
#include "video.h"
#include "audio.h"
#include "menu.h"
#include "input.h"
#include "filelist.h"
#include "main.h"
#include "internet.h"


FreeTypeGX *fontSystem;
int ExitRequested = 0;
extern struct emsg *ms;
bool netHalt = false;
email *eml;
http *inet;
_netaction netaction;
static lwp_t net_t = LWP_THREAD_NULL;


void *networkthread(void *args){
	while(1){
		if(netHalt){
			LWP_SuspendThread(net_t);
		}
		else{
			switch(netaction){
				case SEND_EMAIL:
					eml->sendemail(ms);
					netaction = NONE;
					break;
				case RECV_EMAIL:
					netaction = NONE;
					break;
				case NONE:
					haltnetwork();
					break;
			}
		}
	}
}

void haltnetwork(){
	netHalt = true;

	// wait for thread to finish
	while(!LWP_ThreadIsSuspended(net_t)) usleep(50);
}
void startnetwork(){
	netHalt = false;
	LWP_ResumeThread (net_t);
}


void initall(){
	PAD_Init();
	WPAD_Init();
	InitVideo(); // Initialise video
	InitAudio(); // Initialize audio

	// read wiimote accelerometer and IR data
	WPAD_SetDataFormat(WPAD_CHAN_ALL,WPAD_FMT_BTNS_ACC_IR);
	WPAD_SetVRes(WPAD_CHAN_ALL, screenwidth, screenheight);

	
	
	fatInitDefault();
	
	printf("\x1b[7;7H");
	
	DEBUG_Init(GDBSTUB_DEVICE_USB, 1);
	printf("\x1b[7CInit Wifi...");
	eml = new email();
	printf("success!\n");
	
	mailsettings ssettings;
	mailsettings psettings;
	strcpy(ssettings.server,"smtp-server.tampabay.rr.com");
	strcpy(psettings.server,"pop-server.tampabay.rr.com");
	eml->clearsettings(SMTP);
	eml->clearsettings(POP);
	eml->setsettings(SMTP,&ssettings);
	eml->setsettings(POP,&psettings);
	ms = new emsg;

	fontSystem = new FreeTypeGX();
	fontSystem->loadFont(font_ttf, font_ttf_size, 0);
	fontSystem->setCompatibilityMode(FTGX_COMPATIBILITY_DEFAULT_TEVOP_GX_PASSCLR | FTGX_COMPATIBILITY_DEFAULT_VTXDESC_GX_NONE);

//	InitGUIThreads();
//	LWP_CreateThread (&net_t, networkthread, NULL, NULL, 0, 40);
}

void ExitApp()
{
	ShutoffRumble();
	StopGX();
	exit(0);
}

//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
	
	
	initall();
	
//	MainMenu(MENU_EMAIL);	
	
	mlist *ml = eml->getnewmail();
	printf("%s",ml->ml->body);
	
	exit(0);
}