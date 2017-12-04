#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
/*使用do...while(0)组合是为了连续调用多个函数，在宏展开的时候，不会反生
 * 意向不到的错误，例如只用if且不加括号的时候，即
 * if
 * 		handle_error_en
 * if语句只会执行errno = en而后续语句会跳出if语句*/
#define handle_error_en(en,msg) \
	do{errno = en; perror(msg); exit(EXIT_FAILURE);}while(0);

static void* sig_thread(void* arg)
{
	sigset_t* set = (sigset_t* )arg;
	int s,sig;
	for(;;)
	{
		/*第二个步骤，调用sigwait等待信号*/
		s = sigwait(set, &sig);
		if(s != 0)
			handle_error_en(s,"sigwait");
		printf("Signal handling thread got signal %d\n",sig);
	}
}

int main(int argc,char* argv[])
{
	pthread_t thread;
	sigset_t set;
	int s;

	/*第一个步骤，在主线程中设置信号掩码*/
	sigemptyset(&set);
	sigaddset(&set, SIGQUIT);
	sigaddset(&set, SIGUSR1);
	s = pthread_sigmask(SIG_BLOCK, &set, NULL);
	if(s != 0)
		handle_error_en(s,"pthread_sigmask");
	s = pthread_create(&thread, NULL, &sig_thread, (void*) &set);
	if(s != 0)
		handle_error_en(s, "pthread_careate");
	pause();
}
