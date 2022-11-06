#include <sstream>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include "http_parser.h"
#include "curl/curl.h"
#include "global.h"
#include "utils.h"

#define MAX_REQ_SIZE 10485760

std::string RequestParam::get_param(std::string &name)
{
    std::multimap<std::string, std::string>::iterator i = this->params.find(name);
    if (i == params.end())
    {
        return std::string();
    }
    return i->second;
}

void RequestParam::get_params(std::string &name, std::vector<std::string> &params)
{
    std::pair<std::multimap<std::string, std::string>::iterator, std::multimap<std::string, std::string>::iterator> ret = this->params.equal_range(name);
    for (std::multimap<std::string, std::string>::iterator it = ret.first; it != ret.second; ++it)
    {
        params.push_back(it->second);
    }
}

int RequestParam::parse_query_url(const std::string &query_url)
{
    std::stringstream query_ss(query_url);
    LOG(INFO) << "start parse_query_url: " << query_url.c_str();

    while (query_ss.good())
    {
        std::string key_value;
        std::getline(query_ss, key_value, '&');
        LOG(INFO) << "get key_value: " << key_value.c_str();

        std::stringstream key_value_ss(key_value);
        while (key_value_ss.good())
        {
            std::string key, value;
            std::getline(key_value_ss, key, '=');
            std::getline(key_value_ss, value, '=');
            params.insert(std::pair<std::string, std::string>(key, value));
        }
    }
    return 0;
}

std::string RequestLine::get_request_uri()
{
    std::stringstream ss(this->request_url);
    std::string uri;
    std::getline(ss, uri, '?');
    return uri;
}

int RequestLine::parse_request_line(const char *line, int size)
{
    std::string line_str(line, size);
    std::stringstream ss(std::string(line, size));

    std::getline(ss, method, ' ');
    if (!ss.good())
    {
        LOG(INFO) << "GET method error which line: " << line_str.c_str();
        return -1;
    }
    std::getline(ss, request_url, ' ');
    if (!ss.good())
    {
        LOG(INFO) << "GET request_url error which line: " << line_str.c_str();
        return -1;
    }
    // http://127.0.0.1/hello?name=yuan&age=25
    int ret = parse_request_url_params();
    if (ret != 0)
    {
        LOG(INFO) << "parse_request_url_params fail which request_url: " << request_url.c_str();
        return ret;
    }

    std::getline(ss, http_version, ' ');

    return 0;
}

int RequestLine::parse_request_url_params()
{
    std::stringstream ss(request_url);
    LOG(INFO) << "start parse params which request_url: " << request_url.c_str();

    std::string uri;
    std::getline(ss, uri, '?');
    if (ss.good())
    {
        std::string query_url;
        std::getline(ss, query_url, '?');

        param.parse_query_url(query_url);
    }
    return 0;
}

std::string Request::get_param(std::string name)
{
    if (line.method == "GET")
    {
        return line.get_request_param().get_param(name);
    }
    if (line.method == "POST")
    {
        return body.get_param(name);
    }
    return "";
}

std::string Request::get_unescape_param(std::string name)
{
    std::string param = this->get_param(name);
    if (param.empty())
    {
        return param;
    }
    char *escape_content = curl_unescape(param.c_str(), param.size());
    std::string unescape_param(escape_content);
    curl_free(escape_content);
    return unescape_param;
}

void Request::get_params(std::string &name, std::vector<std::string> &params)
{
    if (line.method == "GET")
    {
        line.get_request_param().get_params(name, params);
    }
    if (line.method == "POST")
    {
        body.get_params(name, params);
    }
}

void Request::add_header(std::string &name, std::string &value)
{
    this->headers[name] = value;
}

std::string Request::get_header(std::string name)
{
    return this->headers[name];
}

std::string Request::get_request_uri()
{
    return line.get_request_uri();
}

Request::Request()
{
    parse_part = PARSE_REQ_LINE;
    req_buf = new std::stringstream();
    body_buf = new std::stringstream();
    total_req_size = 0;

    total_body_size = 0;
    read_body_size = 0;
}

Request::~Request()
{
    if (req_buf != NULL)
    {
        delete req_buf;
        req_buf = NULL;
    }
    if (body_buf != NULL){
        delete body_buf;
        body_buf = NULL;
    }
}

bool Request::check_req_over()
{
    // check last 4 chars
    int check_num = 4;
    req_buf->seekg(-check_num, req_buf->end);
    char check_buf[check_num];
    bzero(check_buf, check_num);

    req_buf->readsome(check_buf, check_num);
    // std::string ss2(check_buf);
    // std::cout << ss2;
    if (strncmp(check_buf, "\r\n\r\n", check_num) != 0)
    {
        return false;
    }
    req_buf->seekg(0);
    return true;
}

int Request::parse_request(const char *read_buffer, int read_size)
{
    total_req_size += read_size;
    if (total_req_size > MAX_REQ_SIZE)
    {
        return -1;
    }
    req_buf->write(read_buffer, read_size);
    LOG(INFO) << "read from client: size: " << read_size;

    if (total_req_size < 4) // 什么都解析不到
    {
        return READ_CONTINUE;
    }
    LOG(INFO) << req_buf->str();
    // bool is_over = this->check_req_over();
    // if (!is_over) {
    //     LOG(ERROR) << "check_req_over err";
    //     return 1; // to be continue
    // }

    std::string line;
    int ret = 0;
    while (req_buf->good())
    {
        // 状态机流转

        std::getline(*req_buf, line, '\n');  // 分割符
        if (line == "\r")  //"\r\n"
        {
            parse_part = PARSE_REQ_OVER;
            if (this->line.method == "POST")
            { 
                parse_part = PARSE_REQ_BODY;
            }
            continue;
        }

        if (parse_part == PARSE_REQ_LINE)
        {   // parse request line like  "GET /index.jsp HTTP/1.1"
            LOG(INFO) << "start parse req_line line: " << line.c_str();
            ret = this->line.parse_request_line(line.c_str(), line.size() - 1);
            if (ret != 0)
            {
                return -1;
            }
            parse_part = PARSE_REQ_HEAD;
            // printf("parse_request_line success which method:%s, url:%s, http_version:%s\n", this->line.method.c_str(), this->line.request_url.c_str(), this->line.http_version.c_str());

            // check method
            // strncasecmp()  TODO
            if (this->line.method != "GET" && this->line.method != "POST")
            {
                LOG(WARNING) << " now unsupported method...";
                return -1;
            }
            continue;
        }

        if (parse_part == PARSE_REQ_HEAD && !line.empty())
        { 
            // read head
            std::vector<std::string> parts;
            SocketUtils::split_str(line, ':', parts); // line like Cache-Control:max-age=0
            if (parts.size() < 2)
            {
                continue;
            }
            // strncasecmp()  TODO
            if(parts[0] == "Content-Length"){ // 大小写
                total_body_size = atoi(parts[1].c_str());
                LOG(INFO) <<  "Content-Length: " << total_body_size;
            }
            add_header(parts[0], parts[1]);
            continue;
        }

        if (parse_part == PARSE_REQ_BODY && !line.empty())
        {
            // printf("start PARSE_REQ_BODY line:%s\n", line.c_str());
            // 这时候请求头已经解析完了，要拿到 Content-Length
            if(total_body_size > 0){
                size_t line_length = line.size();
                read_body_size += line_length;
                read_body_size += 1;
                const char* body_buffer = line.c_str();
                body_buf->write(body_buffer, line_length); 

                if(read_body_size >= total_body_size){
                    // 说明body 读完了
                    if(this->headers["Content-Type"] == "application/x-www-form-urlencoded"){
                        this->body.parse_query_url(this->body_buf->str());
                    } else if (this->headers["Content-Type"] == "application/json"){
                        // 直接操作 body buf
                        LOG(INFO) << "body content: " << body_buf->str();
                    }
                    parse_part = PARSE_REQ_OVER;
                    break;
                } else if(read_body_size < total_body_size){
                    continue;
                } else {
                    return -1;
                } 
            }
        }
        if (parse_part == PARSE_REQ_OVER){
            break;
        }
    }

    if (parse_part != PARSE_REQ_OVER)
    {
        // std::string line_info = "unknown";
        // if (parse_part > PARSE_REQ_LINE)
        // {
        //     line_info = this->line.to_string();
        // }
        // // printf("parse request no over parse_part:%d, line_info:%s\n", parse_part, line_info.c_str());
        return READ_CONTINUE; // to be continue
    }
    return ret;
}

Response::Response(CodeMsg status_code)
{
    this->code_msg = status_code;
    this->is_writed = 0;
}

void Response::set_head(std::string name, std::string &value)
{
    this->headers[name] = value;
}

void Response::set_body(Json::Value &body)
{
    Json::FastWriter writer;
    std::string str_value = writer.write(body);
    this->body = str_value;
}

int Response::gen_response(std::string &http_version, bool is_keepalive)
{
    // printf("START gen_response code:%d, msg:%s\n", code_msg.status_code, code_msg.msg.c_str());
    res_bytes << http_version << " " << code_msg.status_code << " " << code_msg.msg << "\r\n";
    res_bytes << "Server: SimpleServer/0.1"
              << "\r\n";
    if (headers.find("Content-Type") == headers.end())
    {
        res_bytes << "Content-Type: application/json; charset=UTF-8"
                  << "\r\n";
    }
    res_bytes << "Content-Length: " << body.size() << "\r\n";

    std::string con_status = "Connection: close";
    if (is_keepalive)
    {
        con_status = "Connection: Keep-Alive";
    }
    res_bytes << con_status << "\r\n";

    for (std::map<std::string, std::string>::iterator it = headers.begin(); it != headers.end(); ++it)
    {
        res_bytes << it->first << ": " << it->second << "\r\n";
    }
    // header end
    res_bytes << "\r\n";
    res_bytes << body;

    // printf("gen response context:%s\n", res_bytes.str().c_str());
    return 0;
}

int Response::readsome(char *buffer, int buffer_size, int &read_size)
{
    res_bytes.read(buffer, buffer_size);
    read_size = res_bytes.gcount();

    if (!res_bytes.eof())
    {
        return 1;
    }
    return 0;
}

int Response::rollback(int num)
{
    if (res_bytes.eof())
    {
        res_bytes.clear();
    }
    int rb_pos = (int)res_bytes.tellg() - num;
    res_bytes.seekg(rb_pos);
    return res_bytes.good() ? 0 : -1;
}
