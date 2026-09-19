#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "config.h"

void web_server_init(void);
void web_server_handle_client(void);
void tcp_send_feedback(const char* json_str);

#endif // WEB_SERVER_H
