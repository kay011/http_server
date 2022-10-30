#ifndef HTTP_PARSER_H_
#define HTTP_PARSER_H_

#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <functional>
#include <sys/time.h>
#include "json/json.h"
#include "easylogging++.h"

#define RequestBody RequestParam

class Request;
class Response;

typedef std::function<void(Request &request, Response &response)> method_handler_callback;
typedef std::function<void(Request &request, Json::Value &response)> json_handler_callback;

struct HttpMethod
{
	int code;
	std::string name;
};

struct Resource
{
	HttpMethod method;
	method_handler_callback method_handler_cb;
	json_handler_callback json_handler_cb;
};

struct CodeMsg {
	int status_code;
	std::string msg;
};

const  HttpMethod GET_METHOD = {1, "GET"};
const  HttpMethod POST_METHOD = {2, "POST"};

const CodeMsg STATUS_OK = {200, "OK"};
const CodeMsg STATUS_NOT_FOUND = {404, "Not Found"};
const CodeMsg STATUS_METHOD_NOT_ALLOWED = {405, "Method Not Allowed"};

const int PARSE_REQ_LINE = 0;
const int PARSE_REQ_HEAD = 1;
const int PARSE_REQ_BODY = 2;
const int PARSE_REQ_OVER = 3;

class RequestParam {
private:
    std::multimap<std::string, std::string> params;
public:

    std::string get_param(std::string &name);
    void get_params(std::string &name, std::vector<std::string> &params);
    int parse_query_url(const std::string &query_url);
};

class RequestLine {

public:
	std::string method;       // like GET/POST
	std::string request_url;  // like /hello?name=aaa
	std::string http_version; // like HTTP/1.1

	std::string get_request_uri();

    int parse_request_line(const char *line, int size);

    RequestParam &get_request_param() {
        return param;
    }

    std::string to_string() {
        std::string ret = "method:";
        ret += method;
        ret += ",";
        ret += "request_url:";
        ret += request_url;
        ret += ",";
        ret += "http_version:";
        ret += http_version;
        return ret;
    }
private:
    RequestParam param;
    /**
     * request_url : /sayhello?name=tom&age=3
     */
    int parse_request_url_params();
};

class Request {
private:
	std::map<std::string, std::string> headers;
	std::stringstream *req_buf;
	
	int total_req_size;

	int total_body_size;
	int read_body_size;
public:
	int parse_part;
	RequestLine line;
	RequestBody body;
	std::stringstream *body_buf;
	Request();

	~Request();

	std::string get_param(std::string name);

	std::string get_unescape_param(std::string name);

	void get_params(std::string &name, std::vector<std::string> &params);

	void add_header(std::string &name, std::string &value);

	std::string get_header(std::string name);

	std::string get_request_uri();

	inline bool check_req_over();

	int parse_request(const char *read_buffer, int read_size);

	int clear();
};

class Response {
private:
	std::map<std::string, std::string> headers;
	std::stringstream res_bytes;

public:
	bool is_writed;
	CodeMsg code_msg;
	std::string body;

	Response(CodeMsg status_code = STATUS_OK);
	Response(CodeMsg status_code, Json::Value &body);

	void set_head(std::string name, std::string &value);

	void set_body(Json::Value &body);

	int gen_response(std::string &http_version, bool is_keepalive);

	/**
	 * return 0: read part, 1: read over, -1:read error
	 */
	int readsome(char *buffer, int buffer_size, int &read_size);

	/**
	 * rollback num bytes in response bytes
	 */
	int rollback(int num);

};

class HttpContext {
private:
	Response *res;
	Request *req;
public:
	int fd;
	timeval start;

	HttpContext(int fd) {
		this->fd = fd;
		req = new Request();
		res = new Response();
	}

	~HttpContext() {
	    delete_req_res();
	}

	int record_start_time() {
		gettimeofday(&start, NULL);
		return 0;
	}

	int get_cost_time() {
		timeval end;
		gettimeofday(&end, NULL);
		int cost_time = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
		return cost_time;
	}

	void print_access_log(std::string &client_ip) {
		std::string http_method = this->req->line.method;
		std::string request_url = this->req->line.request_url;
		int cost_time = get_cost_time();
		LOG(INFO)<< "access_log " << http_method.c_str() << " " << request_url.c_str() << " "
				 << "status_code:" << res->code_msg.status_code << " "
				 << "cost_time:" << cost_time << "us, "
				 << "body_size:" << res->body.size() << ", "
				 << "client_ip:" << client_ip.c_str();
		// printf("access_log %s %s status_code:%d cost_time:%d us, body_size:%d, client_ip:%s\n",
		//         http_method.c_str(), request_url.c_str(), res->code_msg.status_code,
		//         cost_time, res->body.size(), client_ip.c_str());
	}

	inline void delete_req_res() {
	    if (req != NULL) {
            delete req;
            req = NULL;
        }
        if (res != NULL) {
            delete res;
            res = NULL;
        }
	}

	void clear() {
	    delete_req_res();
	    req = new Request();
        res = new Response();
	}

	Response &get_res() {
	    return *res;
	}

	Request &get_requset() {
	    return *req;
	}

};

#endif /* HTTP_PARSER_H_ */
