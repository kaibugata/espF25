from http.server import BaseHTTPRequestHandler, HTTPServer
import logging
import json
import urllib.request


# Server's own location (hardcode OR set dynamically)

def get_server_city():
    try:
        with urllib.request.urlopen("https://ipinfo.io/json") as response:
            data = response.read()
            info = json.loads(data.decode())
            return info.get("city", "Unknown")
    except Exception as e:
        print("Failed to get server city:", e)
        return "Unknown"

# Detect city once when server starts
SERVER_CITY = get_server_city()
print("Server city:", SERVER_CITY)

class MyRequestHandler(BaseHTTPRequestHandler):

    # ------------------------
    #   HANDLE GET REQUESTS
    # ------------------------
    def do_GET(self):
        logging.info(f"GET request: {self.path}")

        if self.path == "/location":
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps({"city": SERVER_CITY}).encode())
            return



        # Default GET response
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.end_headers()
        self.wfile.write(b"Hello, GET request received.\n")

    # ------------------------
    #   HANDLE POST REQUESTS
    # ------------------------
    def do_POST(self):
        content_length = int(self.headers.get("Content-Length", 0))
        data_bytes = self.rfile.read(content_length)
        data_str = data_bytes.decode("utf-8")

        logging.info(f"Raw POST data: {data_str}")

        # Attempt to parse JSON from ESP32
        try:
            data = json.loads(data_str)
            city = data.get("city")
            outdoor_temp = data.get("outdoor_temp_C")
            sensor_temp = data.get("sensor_temp_C")

            print("\n=========== ESP32 Report ===========")
            print(f"Server location:  {city}")
            print(f"Outdoor temp:     {outdoor_temp} C")
            print(f"ESP32 temperature:{sensor_temp} C")
            print("====================================\n")

        except Exception as e:
            logging.error(f"Failed to parse JSON: {e}")

        # Respond OK
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.end_headers()
        self.wfile.write(b"POST received.\n")


def run(server_class=HTTPServer, handler_class=MyRequestHandler, port=8000):
    logging.basicConfig(level=logging.INFO)
    server_address = ("", port)
    httpd = server_class(server_address, handler_class)
    logging.info(f"Starting server on port {port}...")
    httpd.serve_forever()


if __name__ == '__main__':
    run()
