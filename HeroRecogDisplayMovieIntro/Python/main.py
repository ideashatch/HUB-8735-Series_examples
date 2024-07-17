from http.server import BaseHTTPRequestHandler, HTTPServer
import webbrowser


class RequestHandler(BaseHTTPRequestHandler):

    def do_GET(self):
        if self.path == '/open_browser1':
            webbrowser.open('file:///C:/Users/phone/OneDrive/%E6%A1%8C%E9%9D%A2/%E8%B3%BC%E7%A5%A8%E4%BB%8B%E9%9D%A2.html')
        elif self.path == '/open_browser2':
            webbrowser.open('file:///C:/Users/phone/OneDrive/%E6%A1%8C%E9%9D%A2/%E8%B3%BC%E7%A5%A8%E4%BB%8B%E9%9D%A2%20-2.html')
        self.send_response(200)
        self.send_header('Content-type', 'text/html')
        self.end_headers()
        self.wfile.write(b'Browser opened')


def run(server_class=HTTPServer, handler_class=RequestHandler, port=8888):
    server_address = ('', port)
    httpd = server_class(server_address, handler_class)
    print(f'Starting httpd server on port {port}')
    httpd.serve_forever()


if __name__ == '__main__':
    run()
