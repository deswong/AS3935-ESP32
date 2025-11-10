#include "http_helpers.h"
#include "esp_http_server.h"

esp_err_t http_helpers_send_400(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_sendstr(req, "Bad Request\n");
}

esp_err_t http_helpers_send_500(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_sendstr(req, "Internal Server Error\n");
}
