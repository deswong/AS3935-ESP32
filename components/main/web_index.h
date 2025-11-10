/* Auto-generated simple embedded SPA header (atomic replace)
 * This file contains a minimal single-page app used by the firmware's HTTP
 * server. It intentionally keeps the HTML small for clarity during recovery.
 */
#ifndef WEB_INDEX_H
#define WEB_INDEX_H

#include <stddef.h>

const char index_html[] =
"<!doctype html>\n"
"<html lang=\"en\">\n"
"<head>\n"
"  <meta charset=\"utf-8\">\n"
"  <meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
"  <title>AS3935</title>\n"
"  <style>body{font-family:Arial,Helvetica,sans-serif;margin:12px}</style>\n"
"</head>\n"
"<body>\n"
"<h1>AS3935 Config (recovery)</h1>\n"
"<p>The full UI was temporarily removed to fix a corrupted header file.\n"
"Use the REST API endpoints to configure Wi-Fi, MQTT and AS3935 settings.</p>\n"
"</body>\n"
"</html>\n";

const size_t index_html_len = sizeof(index_html) - 1;

#endif // WEB_INDEX_H
