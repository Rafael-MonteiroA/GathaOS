"""Boot TLS 1.2 PSS-only servers and adversarial signatures in the actual OS."""
import contextlib
import json
from pathlib import Path
import shutil
import struct
import sys
import time
from apps_test import launch,field,click
from mkgathafs import create
from network_test import Guest
from pss_fixtures import make
from pss_peer import Server,host_check
from tls_peer import Proxy


def main(iso,seed):
    folder=make("build/pss-tls-pki")
    host_check(folder)
    specs=[]
    for suffix,code in (("sha256",4),("sha384",5),("sha512",6)):
        specs.append((suffix,"rsa_pss_rsae_"+suffix,0x0800+code,"inspect",0))
        specs.append(("pss-leaf","rsa_pss_pss_"+suffix,0x0805+code,"inspect",0))
    specs += [("restricted-leaf","rsa_pss_pss_sha256",0x0809,"inspect",0),
              ("good","rsa_pkcs1_sha256",0x0401,"inspect",0)]
    for fault,result in (("pss-signature",-129),("pss-key-kind",-129),("pss-unoffered",-74),
                         ("pss-hash",-129),("pss-length",-74)):
        specs.append(("sha256","rsa_pss_rsae_sha256",0x0804,fault,result))
    specs += [("restricted-leaf","rsa_pss_pss_sha256",0x0809,"pss-key-kind",-129),
              ("restricted-leaf","rsa_pss_pss_sha256",0x0809,"pss-pkcs-kind",-129),
              ("pss-wrong-name","rsa_pss_rsae_sha256",0x0804,"inspect",-129),
              ("pss-expired","rsa_pss_rsae_sha256",0x0804,"inspect",-129),
              ("sha256","rsa_pss_rsae_sha256",0x0804,"inspect",0)]
    with contextlib.ExitStack() as stack:
        plan=[];proxies=[]
        for name,scheme,code,fault,result in specs:
            server=stack.enter_context(contextlib.closing(Server(folder,name,scheme)))
            proxy=stack.enter_context(contextlib.closing(Proxy(server,fault)))
            proxies.append(proxy);plan.append((proxy.port,result,0,len(server.payload)))
        root=Path("build/pss-tls-rootfs")
        shutil.copytree("build/none/rootfs",root,dirs_exist_ok=True)
        (root/"Documents/TLS-root.der").write_bytes((folder/"root.der").read_bytes())
        (root/"Documents/TLS-plan.bin").write_bytes(struct.pack("<I",len(plan))+b"".join(struct.pack("<IiII",*p) for p in plan))
        private=Path("build/pss-tls-private.img");private.write_bytes(create(root))
        with contextlib.closing(Guest(iso,private,"tls-pss",["-cpu","max","-rtc","base=2026-10-24T12:00:00","-nic","user,model=pcnet"])) as guest:
            guest.boot()
            guest.d.close(guest.window);window=launch(guest.d,1)
            field(guest.d,window,150,52,"/System/Tests");click(guest.d,window,550,54)
            time.sleep(2);baseline=guest.health();before=len(guest.d.log())
            click(guest.d,window,100,166);click(guest.d,window,220,14)
            for i,(_,result,_,_) in enumerate(plan):
                text=guest.wait(lambda s:f"TLS TEST: case {i} result=" in s[before:],150)
                assert f"TLS TEST: case {i} result={result}\n" in text,(i,text[-3000:])
                print("PSS TLS QEMU PASS:",specs[i],flush=True)
            guest.wait(lambda s:"TLS TEST: all protocol, certificate and integrity cases PASS" in s[before:])
            time.sleep(3);assert guest.health()==baseline,(baseline,guest.health())
            for spec,proxy in zip(specs,proxies):
                name,scheme,code,fault,result=spec
                assert ("signature-scheme",code) in proxy.events,(spec,proxy.events)
                if fault!="inspect":assert fault in proxy.events,(spec,proxy.events)
            text=guest.d.log()[before:]
            assert "TLS: RSA-PSS restricted key verified" in text
            assert "TLS: RSA-PSS RSAE verified" in text and "TLS: RSA PKCS1 verified" in text
            (guest.root/"signature-cases.json").write_text(json.dumps([
                {"certificate":s[0],"scheme":s[1],"fault":s[3],"expected":s[4],"events":p.events}
                for s,p in zip(specs,proxies)],indent=2)+"\n")
            guest.d.close(window);guest.window=launch(guest.d,0)
            guest.command_line("echo PSS verification passed","PSS verification passed")
            guest.finish(["six forced PSS schemes","restricted/unrestricted keys","PSS-signed chains",
                          "signature tampering and algorithm confusion rejected","PKCS1 regression",
                          "domain/date policy retained","repeat success and memory reclamation"])


if __name__=="__main__":
    main(Path(sys.argv[1]).resolve(),Path(sys.argv[2]).resolve())
