from http.server import BaseHTTPRequestHandler, HTTPServer
import logging

class MyRequestHandler(BaseHTTPRequestHandler):
  def do_GET(self):
    #handle get
    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.end_headers()
    self.wfile.write(b"Hello! You made a GET request.")

  def do_POST(self):
    #handle post
    content_length = int(self.headers['Content-Length']) #get size of the content
    post_data = self.rfile.read(content_length) #read post data
    logging.info(f"Received POST data: {post_data.decode('utf-8')}")

    #send response back to client
    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.end_headers()
    self.wfile.write(b"Post request received. Thanks")

def run(server_class=HTTPServer, handler_class=MyRequestHandler, port=8000):
  logging.basicConfig(level=logging.INFO)
  server_address = ('', port)
  httpd = server_class(server_address, handler_class)
  logging.info(f"startng server on port {port}...")
  httpd.serve_forever()

if __name__ == '__main__':
  run()