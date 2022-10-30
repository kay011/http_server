#include <sstream>
#include <cstdlib>
#include "http_server.h"
#include "easylogging++.h"

INITIALIZE_EASYLOGGINGPP
void login(Request &request, Json::Value &root)
{
    std::string name = request.get_param("name");
    std::string pwd = request.get_param("pwd");

    printf("login user which name:%s, pwd:%s\n", name.c_str(), pwd.c_str());

    root["code"] = 0;
    root["msg"] = "login success!";
}

void hello(Request &request, Json::Value &root)
{
    LOG(INFO) << "request_in hello";
    root["code"] = 0;
    root["msg"] = "hello world!";
}

void hello2(Request &request, Json::Value &root)
{
    LOG(INFO) << request.body_buf->str();
    root["code"] = 0;
    root["msg"] = "hello world!";
}

int main(int argc, char *argv[])
{
    el::Configurations conf("/root/git_root/http_server_demo/conf/log.conf");
    el::Loggers::reconfigureAllLoggers(conf);
    HttpServer http_server;

    http_server.post("/login", login);
    http_server.get("/hello", hello);
    http_server.post("/hello2", hello2);
    http_server.start(3490);
    return 0;
}