#!/usr/bin/env python3
"""
This script is meant to serve source code (C/C++) to the browser for WASM source maps.
The only reason it's a special script is that it needs to return CORS headers.
"""
from http.server import HTTPServer, SimpleHTTPRequestHandler, test
import sys
import socket


class CORSRequestHandler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Access-Control-Allow-Origin', '*')
        SimpleHTTPRequestHandler.end_headers(self)


class IPv4HTTPServer(HTTPServer):
    address_family = socket.AF_INET


if __name__ == '__main__':
    test(CORSRequestHandler, IPv4HTTPServer, port=int(sys.argv[1]) if len(sys.argv) > 1 else 8000, bind='0.0.0.0')
