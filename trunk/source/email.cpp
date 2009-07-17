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

#include "b64/encode.h"
#include "internet.h"


email::email(){
	response = new char [512];
	line = new char [100];
	smtpsettings = new mailsettings;
	popsettings = new mailsettings;
	smtpsettings->port = 0;
	popsettings->port = 0;
}

email::~email(){
	
}

void email::setsettings(settype st , mailsettings *s){
	if(st == SMTP) memcpy(smtpsettings,s,sizeof(mailsettings));
	if(st == POP) memcpy(popsettings,s,sizeof(mailsettings));
	
}
void email::getsettings(settype st,mailsettings *s){
	if(st == SMTP) memcpy(s,smtpsettings,sizeof(mailsettings));
	if(st == POP) memcpy(s,popsettings,sizeof(mailsettings));
}
void email::clearsettings(settype st){
	if(st == SMTP) memset(smtpsettings,0,sizeof(mailsettings));
	if(st == POP) memset(popsettings,0,sizeof(mailsettings));
}
void email::parseesmtp(int v,char *in){
	if((esmp = strstr(in,"AUTH"))){
		if((esmp = strstr(in,"LOGIN"))){
			v ^= ESMTP_AUTH_LOGIN;
		}
	}
}
void email::sendemail(struct emsg *mess){
	if(smtpsettings->port == 0){
		smtpport = 25;
	}
	else{
		smtpport = smtpsettings->port;
	}
	connect(smtpsettings->server,smtpport,TCP);
	host = strchr(mess->from,'@')+sizeof(char);
	response = read(200);
	sprintf(line,"EHLO %s\r\n",localip);
	writetosocket(line);
	response = read(512);
	if(response[0] == '5'){
		sprintf(line,"HELO %s\r\n", localip);
		writetosocket(line);
		response = read(200);
	}
	else{
		parseesmtp(esmtp,response);
		if(esmtp & ESMTP_AUTH_LOGIN){
			user64 = new char [50];
			pass64 = new char [50];
			base64::encoder E;
			E.encode(smtpsettings->user,strlen(smtpsettings->user), user64);
			E.encode(smtpsettings->password,strlen(smtpsettings->password), pass64);
			writetosocket("AUTH LOGIN\r\n");
			response = read(200);
			sprintf(line,"%s\r\n",user64);
			writetosocket(line);
			response = read(200);
			sprintf(line,"%s\r\n",pass64);
			writetosocket(line);
			response = read(200);
		}
	}
	sprintf(line,"MAIL FROM:<%s>\r\n",mess->from);
	writetosocket(line);
	response = read(200);
	sprintf(line,"RCPT TO:<%s>\r\n",mess->to);
	writetosocket(line);
	response = read(200);
	writetosocket("DATA\r\n");
	response = read(200);
	sprintf(line,"Subject: %s\r\n",mess->subject);
	writetosocket(line);
	sprintf(line,"\r\n%s\r\n",mess->message);
	writetosocket(line);
	writetosocket(".\r\n");
	response = read(200);
	writetosocket("QUIT\r\n");
	response = read(200);
}

void email::parsemessage(messlist *ml,char *mess){
	bl = 0;
	numlines = 0;
	if(strncmp(messline,"+OK",strlen("+OK"))){
		//error
	}
	for(int x = 0;x<(int)strlen(mess);x++){
		if((messline = strstr(mess,"\r\n\r\n"))){
			messline += 4;
			bl = strchr(messline,'\r')-messline;
			ml->body = new char [bl+1];
			ml->body[bl] = '\0';
			strncpy(ml->body,messline,bl);
		}
		if((messline = strstr(mess,"Subject:"))){
			messline += strlen("Subject:");
			bl = strchr(messline,'\r')-messline;
			ml->subject = new char [bl+1];
			ml->subject[bl] = '\0';
			strncpy(ml->subject,messline,bl);
		}
		
	}
}

void email::getsize(char *response, int *sizes,int numdata){
	b = new char [10];
	p = strchr(response,'\n')+1;
	for(int x = 0;x<numdata;x++){
		p = strchr(p,' ')+1;
		for(bl = 0;p[bl] != '\r';bl++);
		strncpy(b,p,bl);
		sizes[x] = atoi(b);
		p = strchr(p,'\n')+1;
	}
}

int email::renderpopresponse(const char *resp){
	pos = 0;
	if(strncmp(resp,"+OK",3)){
		return 0;
	}
	pos = (int)(strchr(resp,' ')-resp);
	pos++;
	mlen = (int)(strchr(resp+pos,' ')-(resp+pos));
	mtmp = new char [mlen];
	strncpy(mtmp,resp+pos,mlen);
	nummessages = atoi(mtmp);			//ASCII number offset
	return nummessages;
}

messlist *email::getnewmail(){
	if(!popsettings->port){
		popport = 110;
	}
	else{
		popport = popsettings->port;
	}
	connect(popsettings->server,popport,TCP);
	response = read(200);
	sprintf(line,"USER %s\r\n",popsettings->user);
	writetosocket(line);
	response = read(200);
	sprintf(line,"PASS %s\r\n",popsettings->password);
	writetosocket(line);
	response = read(200);
	writetosocket("STAT\r\n");
	response = read(200);
	rendered = renderpopresponse(response);
	messsize = new int [rendered];
	writetosocket("LIST\r\n");
	response = read(200);
	
	messsize = new int [rendered];
	getsize(response, messsize, rendered);

	response = read(200);
	newmailroot = new messlist;
	newmail = newmailroot;
	newmail->next = 0;
	for(int x = 0;x<rendered;x++){
		mailbuffer = new char [messsize[x]+30];
		sprintf(line,"RETR %d\r\n",x+1);
		writetosocket(line);
		mailbuffer = read(messsize[x]+30);
		response = read(200);
		
		parsemessage(newmail,mailbuffer);
		
		if(x != rendered-1){
			newmail->next = new messlist;
			newmail = newmail->next;
			newmail->next = 0;
		}
		
	}
	//for(int y = 0;y<rendered;y++){
	//	sprintf(line,"DELE %d\r\n",y+1);
	//	writetosocket(line);
	//	response = read(200);
	//}
	writetosocket("QUIT\r\n");
	response = read(200);
	return newmailroot;
}