#include <sstream>
#include <cstdlib>
#include "http_server.h"
#include "easylogging++.h"
#include "http_handler.h"

INITIALIZE_EASYLOGGINGPP

void init(){
    el::Configurations conf("/root/git_root/http_server_demo/conf/log.conf");
    el::Loggers::reconfigureAllLoggers(conf);
}

void register_router(HttpServer& http_server){
    http_server.get("/hello", hello);
    http_server.post("/login", login);
    http_server.post("/hello2", hello2);
}

/*****   main ****/
int main(int argc, char *argv[])
{
    init();
    HttpServer http_server(3490);
    register_router(http_server);
    http_server.start();
    return 0;
}