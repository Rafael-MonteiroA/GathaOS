"""OpenSSL process fixture with explicit TLS signature-scheme selection."""
import socket
import ssl
import subprocess
import threading
import time
from pathlib import Path


class Server:
    def __init__(self, folder, name="sha256", scheme="rsa_pss_rsae_sha256", payload=32781):
        folder=Path(folder).resolve()
        self.payload=bytes((i*37+11)&255 for i in range(payload))
        self.events=[]
        with socket.socket() as reserve:
            reserve.bind(("127.0.0.1",0))
            self.port=reserve.getsockname()[1]
        key=name+".key" if name in ("pss-leaf","restricted-leaf") else "leaf.key"
        self.log=(folder/f"server-{self.port}.log").open("wb")
        self.process=subprocess.Popen(
            ["openssl","s_server","-accept",f"127.0.0.1:{self.port}","-cert",str(folder/(name+".pem")),
             "-key",str(folder/key),"-cert_chain",str(folder/"pss-intermediate.pem"),
             "-tls1_2","-cipher","ECDHE-RSA-AES128-GCM-SHA256","-groups","X25519",
             "-sigalgs",scheme,"-alpn","http/1.1","-no_ticket","-naccept","1",
             "-quiet","-no_ign_eof"],
            stdin=subprocess.PIPE,stdout=self.log,stderr=self.log)
        def feed():
            try:
                self.process.stdin.write(self.payload)
                self.process.stdin.close()
            except (BrokenPipeError,OSError):
                pass
        self.thread=threading.Thread(target=feed,daemon=True)
        self.thread.start()
        # Do not consume the one allowed connection with a readiness probe.
        time.sleep(.1)
        assert self.process.poll() is None,(folder/f"server-{self.port}.log").read_text()
    def close(self):
        if self.process.poll() is None:
            self.process.terminate()
            try:self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:self.process.kill();self.process.wait()
        self.thread.join(timeout=2)
        self.log.close()


def host_check(folder):
    for name,scheme in (("sha256","rsa_pss_rsae_sha256"),("pss-leaf","rsa_pss_pss_sha384"),
                        ("restricted-leaf","rsa_pss_pss_sha256")):
        server=Server(folder,name,scheme,payload=12345)
        try:
            ctx=ssl.create_default_context(cafile=str(Path(folder)/"root.pem"))
            ctx.maximum_version=ssl.TLSVersion.TLSv1_2
            ctx.set_ciphers("ECDHE-RSA-AES128-GCM-SHA256")
            with socket.create_connection(("127.0.0.1",server.port),timeout=5) as tcp:
                with ctx.wrap_socket(tcp,server_hostname="localhost") as stream:
                    stream.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n")
                    received=b""
                    while part:=stream.recv(16384):received+=part
                    assert received==server.payload,(name,len(received))
        finally:server.close()
    print("PSS OPENSSL PEER PASS: RSAE, unrestricted and restricted PSS keys",flush=True)


if __name__=="__main__":
    import sys
    host_check(Path(sys.argv[1]))
