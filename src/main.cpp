#include <sstream>
#include <cstdlib>
#include "http_server.h"
#include "easylogging++.h"
#include "http_handler.h"
#include "stacktrace.h"
#include <signal.h>

INITIALIZE_EASYLOGGINGPP

void init(){
    // el::Configurations conf("/root/git_root/http_server_demo/conf/log.conf");
    // el::Loggers::reconfigureAllLoggers(conf);

	// el::Logger* defaultLogger = el::Loggers::getLogger("default");
    el::Configurations conf;
	conf.setToDefault();
	conf.setGlobally(el::ConfigurationType::Format, "[%datetime{%H:%m:%s} | %level] %msg");
	conf.setGlobally(el::ConfigurationType::Filename, "log_%datetime{%Y%M%d}.log");
	conf.setGlobally(el::ConfigurationType::Enabled, "true");
	conf.setGlobally(el::ConfigurationType::ToFile, "true");
	el::Loggers::reconfigureAllLoggers(conf);
	el::Loggers::reconfigureAllLoggers(el::ConfigurationType::ToStandardOutput, "false");
}

void register_router(HttpServer& http_server){
    http_server.get("/hello", hello);
    http_server.post("/login", login);
    http_server.post("/hello2", hello2);
}

void DumpTraceback(int Signal){
    print_stacktrace(Signal);
    _exit(1);
}
int SigHandle()
{
	//signal(SIGINT, SIG_IGN);
//	signal(SIGHUP, SIG_IGN);
//	signal(SIGQUIT, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);

	// Dump traceback when crash.
	// Core signal's default action is to terminate the process and dump core.
	signal(SIGBUS, DumpTraceback);  // 10 Core  Bus error (bad memory access)
	signal(SIGSEGV, DumpTraceback); // 11 Core  Invalid memory reference
	signal(SIGABRT, DumpTraceback); // 6  Core  Abort signal from abort(3)
	signal(SIGILL, DumpTraceback);  // 4  Core  Illegal Instruction
	signal(SIGFPE, DumpTraceback);  // 8  Core  Floating point exception

	return 0;
}
/*****   main ****/
int main(int argc, char *argv[])
{
    init();
	SigHandle();
    HttpServer http_server(3490);
    register_router(http_server);
    http_server.start();
    return 0;
}