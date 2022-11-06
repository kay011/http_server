#ifndef HTTP_HANDLER_H
#define HTTP_HANDLER_H
#include "http_parser.h"
#include "json/json.h"
void login(Request &request, Json::Value &root);
void hello(Request &request, Json::Value &root);
void hello2(Request &request, Json::Value &root);
#endif