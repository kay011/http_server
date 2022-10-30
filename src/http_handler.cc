#include <string>
#include "http_handler.h"



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