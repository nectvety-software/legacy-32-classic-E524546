from http.server import HTTPServer, BaseHTTPRequestHandler
import ssl, threading, time, sys
class H(BaseHTTPRequestHandler):
    protocol_version='HTTP/1.1'
    def log_message(self, fmt,*args):
        sys.stdout.write('[server %s] '%self.server.server_port + (fmt%args)+'\n'); sys.stdout.flush()
    def body(self, code, html, headers=None):
        b=html.encode('utf-8')
        self.send_response(code)
        self.send_header('Content-Type','text/html; charset=utf-8')
        self.send_header('Content-Length',str(len(b)))
        if headers:
            for k,v in headers:self.send_header(k,v)
        self.send_header('Connection','close')
        self.end_headers(); self.wfile.write(b)
    def do_GET(self):
        if self.path=='/redirect':
            self.send_response(302); self.send_header('Location','/home'); self.send_header('Set-Cookie','sid=dmax123; Path=/'); self.send_header('Content-Length','0'); self.send_header('Connection','close'); self.end_headers(); return
        if self.path=='/home':
            ck=self.headers.get('Cookie','(none)')
            self.body(200, f'''<!doctype html><html><head><title>DMAX Test Portal</title><style>.x{{color:red}}</style><script>BAD_SCRIPT_TEXT</script></head><body><a href="/article">Open Article</a><br><b>HTTP redirect OK</b><br>Cookie: {ck}<br><img alt="DMAX logo" src="/logo.png"><br><a href="relative/info.html">Relative Info</a></body></html>'''); return
        if self.path=='/article':
            self.body(200,'''<html><head><title>Article One</title></head><body><h2>Article One</h2><p>This page opened by selecting a link in the browser.</p><a href="../home">Back home</a></body></html>'''); return
        if self.path=='/relative/info.html':
            self.body(200,'''<html><head><title>Relative Info</title></head><body>Relative URL resolver works.</body></html>'''); return
        if self.path=='/':
            self.body(200,'''<html><head><title>HTTPS Secure Test</title></head><body><a href="/chunked">Open Chunked</a><br>HTTPS TLS connection OK<br>Encrypted local test page<br><noscript>HIDDEN_NOSCRIPT</noscript></body></html>''', [('Set-Cookie','tlsid=secure42; Path=/')]); return
        if self.path=='/chunked':
            self.send_response(200); self.send_header('Content-Type','text/html; charset=utf-8'); self.send_header('Transfer-Encoding','chunked'); self.send_header('Connection','close'); self.end_headers()
            parts=[b'<html><head><title>Chunked Page</title></head><body>',b'Chunked transfer decoded OK<br>',b'<a href="/">HTTPS Home</a></body></html>']
            for p in parts:
                self.wfile.write(('%X\r\n'%len(p)).encode()+p+b'\r\n'); self.wfile.flush(); time.sleep(.03)
            self.wfile.write(b'0\r\n\r\n'); self.wfile.flush(); return
        self.body(404,'<html><head><title>Not Found</title></head><body>404</body></html>')

def run_http(): HTTPServer(('127.0.0.1',18080),H).serve_forever()
def run_https():
    s=HTTPServer(('127.0.0.1',18443),H)
    ctx=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);ctx.load_cert_chain('cert.pem','key.pem');s.socket=ctx.wrap_socket(s.socket,server_side=True);s.serve_forever()
threading.Thread(target=run_http,daemon=True).start();threading.Thread(target=run_https,daemon=True).start();print('SERVERS_READY',flush=True)
while True: time.sleep(1)
