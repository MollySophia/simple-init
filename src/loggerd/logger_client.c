#include<ctype.h>
#include<errno.h>
#include<stdio.h>
#include<stdarg.h>
#include<stdlib.h>
#include<string.h>
#include<sys/un.h>
#include<sys/socket.h>
#include"str.h"
#include"defines.h"
#include"system.h"
#include"logger_internal.h"

int logfd=-1;

int set_logfd(int fd){
	return logfd=fd<0?logfd:fd;
}

void close_logfd(){
	if(logfd<0)return;
	close(logfd);
	logfd=-1;
}

int open_file_logfd(char*path){
	if(!path)ERET(EINVAL);
	int fd=open(path,O_WRONLY|O_SYNC);
	if(fd<0)fprintf(stderr,"cannot open log pipe %s: %m\n",path);
	return fd<0?fd:set_logfd(fd);
}

int open_socket_logfd(char*path){
	if(!path)ERET(EINVAL);
	struct sockaddr_un addr;
	int sock;
	if((sock=socket(AF_UNIX,SOCK_STREAM,0))<0){
		fprintf(stderr,"cannot create socket: %m\n");
		return -1;
	}
	memset(&addr,0,sizeof(addr));
	addr.sun_family=AF_UNIX;
	strncpy(addr.sun_path,path,sizeof(addr.sun_path)-1);
	if(connect(sock,(struct sockaddr*)&addr,sizeof(addr))<0){
		fprintf(stderr,"cannot connect socket %s: %m\n",path);
		close(sock);
		return -1;
	}
	return set_logfd(sock);
}

static int log_msg_send(enum log_oper oper,void*data,size_t size){
	struct log_msg msg;
	int x=logger_internal_send_msg(logfd,oper,data,size);
	if(x<0)return -1;
	do{if(logger_internal_read_msg(logfd,&msg)<0)return -1;}
	while(msg.oper!=LOG_OK&&msg.oper!=LOG_FAIL);
	if(msg.size>0){
		void*d=malloc(msg.size);
		if(!d)return -1;
		memset(d,0,msg.size);
		ssize_t r=read(logfd,d,msg.size);
		if(r<0||(size_t)r!=msg.size){
			free(d);
			return -1;
		}
		errno=parse_int((char*)d,0);
		free(d);
	}else errno=0;
	return x;
}

static inline int log_msg_send_string(enum log_oper oper,char*data){
	return data?log_msg_send(oper,data,strlen(data)):-EINVAL;
}

int logger_listen(char*file){
	return file?log_msg_send_string(LOG_LISTEN,file):-EINVAL;
}

int logger_open(char*file){
	return file?log_msg_send_string(LOG_OPEN,file):-EINVAL;
}

int logger_exit(){
	int r;
	if((r=log_msg_send(LOG_QUIT,NULL,0))<0)return r;
	close_logfd();
	return 0;
}

int logger_klog(){
	return log_msg_send(LOG_KLOG,NULL,0);
}

int logger_write(struct log_item*log){
	if(!log)ERET(EINVAL);
	ssize_t s=strlen(log->content)-1;
	while(s>=0)
		if(!isspace(log->content[s]))break;
		else log->content[s]=0,s--;
	if(logfd<0)return fprintf(stderr,"%s: %s\n",log->tag,log->content);
	return log_msg_send(LOG_ADD,(void*)log,sizeof(struct log_item));
}

int logger_print(enum log_level level,char*tag,char*content){
	if(!tag||!content)ERET(EINVAL);
	struct log_item log;
	memset(&log,0,sizeof(struct log_item));
	log.level=level;
	strncpy(log.tag,tag,sizeof(log.tag)-1);
	strncpy(log.content,content,sizeof(log.content)-1);
	time(&log.time);
	log.pid=getpid();
	return logger_write(&log);
}

static int logger_printf_x(enum log_level level,char*tag,const char*fmt,va_list ap){
	if(!tag||!fmt)ERET(EINVAL);
	size_t size=sizeof(char)*BUFFER_SIZE;
	char*content=malloc(size+1);
	if(!content)return -errno;
	memset(content,0,size+1);
	if(!vsnprintf(content,size,fmt,ap)){
		free(content);
		return -errno;
	}
	int r=logger_print(level,tag,content);
	free(content);
	return r;
}

static int logger_perror_x(enum log_level level,char*tag,const char*fmt,va_list ap){
	if(!fmt)ERET(EINVAL);
	char*buff=NULL;
	if(errno>0){
		char*er=strerror(errno);
		size_t s=strlen(fmt)+strlen(er)+4;
		if(!(buff=malloc(s)))ERET(ENOMEM);
		memset(buff,0,s);
		snprintf(buff,s,"%s: %s",fmt,er);
	};
	int e=logger_printf_x(level,tag,buff?buff:fmt,ap);
	if(buff)free(buff);
	return e;
}

int logger_printf(enum log_level level,char*tag,const char*fmt,...){
	if(!tag||!fmt)ERET(EINVAL);
	int err=errno;
	va_list ap;
	va_start(ap,fmt);
	errno=err;
	int r=logger_printf_x(level,tag,fmt,ap);
	va_end(ap);
	errno=err;
	return r;
}

int logger_perror(enum log_level level,char*tag,const char*fmt,...){
	if(!tag||!fmt)ERET(EINVAL);
	int err=errno;
	va_list ap;
	va_start(ap,fmt);
	errno=err;
	int r=logger_perror_x(level,tag,fmt,ap);
	va_end(ap);
	errno=err;
	return r;
}

int return_logger_printf(enum log_level level,int e,char*tag,const char*fmt,...){
	if(!tag||!fmt)ERET(EINVAL);
	int err=errno;
	va_list ap;
	va_start(ap,fmt);
	errno=err;
	logger_printf_x(level,tag,fmt,ap);
	va_end(ap);
	errno=err;
	return e;
}

int return_logger_perror(enum log_level level,int e,char*tag,const char*fmt,...){
	if(!tag||!fmt)ERET(EINVAL);
	int ee=errno;
	va_list ap;
	va_start(ap,fmt);
	errno=ee;
	logger_perror_x(level,tag,fmt,ap);
	va_end(ap);
	return e;
}

int start_loggerd(pid_t*p){
	int fds[2],r;
	if(logfd>=0)ERET(EEXIST);
	if(socketpair(AF_UNIX,SOCK_STREAM,0,fds)<0)return -errno;
	pid_t pid=fork();
	switch(pid){
		case -1:return -errno;
		case 0:
			close_all_fd((int[]){fds[0]},1);
			r=loggerd_thread(fds[0]);
			exit(r);
			_exit(r);
			return r;
	}
	close(fds[0]);
	struct log_msg msg;
	do{if(logger_internal_read_msg(fds[1],&msg)<0)ERET(EIO);}
	while(msg.oper!=LOG_OK);
	if(p)*p=pid;
	return set_logfd(fds[1]);
}

char*level2string(enum log_level level){
	switch(level){
		case LEVEL_DEBUG:return   "DEBUG";
		case LEVEL_INFO:return    "INFO";
		case LEVEL_NOTICE:return  "NOTICE";
		case LEVEL_WARNING:return "WARN";
		case LEVEL_ERROR:return   "ERROR";
		case LEVEL_CRIT:return    "CRIT";
		case LEVEL_ALERT:return   "ALERT";
		case LEVEL_EMERG:return   "EMGCY";
		default:return            "?????";
	}
}

enum log_level parse_level(const char*v){
	#define CS (const char*[])
	if(!v)return 0;
	if(     fuzzy_cmps(v,CS{"7","debug","dbg"      ,NULL}))return LEVEL_DEBUG;
	else if(fuzzy_cmps(v,CS{"6","info","inf"       ,NULL}))return LEVEL_INFO;
	else if(fuzzy_cmps(v,CS{"5","notice"           ,NULL}))return LEVEL_NOTICE;
	else if(fuzzy_cmps(v,CS{"4","warning"          ,NULL}))return LEVEL_WARNING;
	else if(fuzzy_cmps(v,CS{"3","error"            ,NULL}))return LEVEL_ERROR;
	else if(fuzzy_cmps(v,CS{"2","critical"         ,NULL}))return LEVEL_CRIT;
	else if(fuzzy_cmps(v,CS{"1","alert","alrt"     ,NULL}))return LEVEL_ALERT;
	else if(fuzzy_cmps(v,CS{"0","emgcy","emergency",NULL}))return LEVEL_EMERG;
	else return 0;
}
