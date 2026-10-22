# Stage 10 networking

This is an original, bounded IPv4 client stack for a virtual Ethernet adapter.
All guest code is freestanding C. QEMU's NAT and the host test servers are tools,
not code incorporated into GathaOS. The desktop still boots without a NIC.

## Use

Attach a PCnet adapter in NAT mode. `make run` selects it explicitly. In Terminal:

```text
net
nslookup example.com
wget http://example.com/ /Documents/Internet.html
cat /Documents/Internet.html
```

`net` shows DHCP state, IP/mask/gateway/DNS and packet counters. DHCP runs in the
background. Wait until it reports connected. `nslookup` requests an IPv4 A record;
numeric addresses are accepted directly. `wget` accepts HTTP URLs, an optional
numeric port and a required destination path. Escape cancels a transfer; closing
Terminal also cancels and reclaims its memory and network resources. Other apps
continue running while network operations wait. Saving the complete result uses
the existing synchronous, staged GathaFS save, which can briefly pause input.

Downloads are limited to 4 MiB. A failed request does not replace the destination.
The filesystem can impose a lower limit, including 192 KiB on legacy v1 disks.
The downloader buffers the response before saving; it needs RAM and enough disk
space for the staged replacement. An incomplete length/chunked body is an error.

## Driver and execution

PCI configuration mechanism 1 discovers AMD 1022:2000 and enables I/O and bus
mastering. The original PCnet driver uses 32-bit software style 2, 32 receive
descriptors with 2 KiB buffers and eight transmit descriptors with 1536-byte
buffers. Storage is aligned and pinned in the kernel image; DMA uses physical
addresses, never user or heap virtual pointers. Ownership transfers use volatile
descriptors and compiler barriers on this coherent x86 uniprocessor. Frame sizes,
completion flags and receive CRC errors are checked. A DMA memory error stops
the interface. Padding is explicit; the controller produces the Ethernet FCS.

The driver polls and never enables the PCI interrupt. Each compositor iteration
handles at most 32 completed packets and bounded timer work with IRQs disabled.
Network syscalls also run with IRQs disabled, so the shared state cannot be
reentered. No network operation busy-waits for a reply inside a syscall. User
wrappers sleep/yield and poll events between attempts. PCnet initialization has
a finite register-read budget. Incoming traffic cannot create tasks or allocate
unbounded kernel memory.

## Protocols and limits

- Ethernet II, ARP with eight expiring entries, IPv4 with a 1500-byte MTU and
  header checksums. IP options and fragments are dropped; IPv6, VLANs, multicast,
  Wi-Fi, discovery of other NICs and path-MTU discovery are not implemented.
- UDP length/checksum validation. DHCP DISCOVER/OFFER/REQUEST/ACK assigns the
  address, mask, first router and first DNS server. Transaction and client MAC
  must match. Retry backs off to 32 seconds. Leases renew by broadcast REQUEST
  halfway through the lease and expire/reacquire if renewal fails. Server lease
  times are capped at one day for bounded tick arithmetic. There is no static-IP
  UI, address-conflict detection, multiple-interface support or DHCP option
  overload processing.
- Four process-owned DNS queries, 253-byte names, A and ordered CNAME answers,
  bounded backward compression pointers, response ID/port/server/question
  matching, retries and a ten-second timeout. NXDOMAIN is reported. UDP replies
  over 512 bytes or truncated replies are unsupported; there is no DNS-over-TCP,
  EDNS, DNSSEC, AAAA, recursive alias follow-up or DNS cache.
- Four process-owned outbound TCP connections. Each has a 16 KiB receive ring
  and one outstanding transmit segment, respecting the peer window/MSS. Receive
  checks cover the pseudoheader checksum, header sizes, tuple, sequence and ACK
  bounds. Duplicates and overlapping prefixes are trimmed; out-of-order data is
  dropped and acknowledged for retransmission. SYN/data retransmit with bounded
  exponential backoff. Connecting times out after 15 seconds, idle connections
  after 30 seconds. No listening sockets, SACK, window scaling, timestamps,
  general congestion control, zero-window persistence probes or full close
  handshake/TIME_WAIT. Close uses an explicit TCP abort (RST), including after
  a complete HTTP body. This is a limited client transport, not a claim of full
  TCP standards compliance or readiness for arbitrary hostile networks.
- ICMP echo replies are implemented; no shell ping command or ICMP error/PMTU
  processing. DHCP, DNS IDs, local ports and initial sequence numbers are
  deterministic, not cryptographically unpredictable. NAT is the intended
  environment; there is no firewall, transport encryption or authentication.
- HTTP/1.1 GET in user mode, with HTTP/1.0 response support. Status 200 only;
  Content-Length, chunked and connection-close bodies. Header/trailer sections
  are bounded to 8 KiB, lines to 2047 bytes. Conflicting framing, duplicate
  Content-Length, truncation, size overflow and unsupported encodings are errors.
  The total post-DNS operation is bounded to 120 seconds. No redirects, cookies,
  proxies, compressed content, authentication, TLS, HTML rendering or JavaScript.
  HTTPS URLs fail clearly. Google browsing is not provided by this stage.

## Syscall 31

EAX=31, EBX=operation, ECX=user buffer, EDX=buffer byte count, ESI=handle.
EAX returns a signed result. All buffers are checked against the current user
page tables; receive/info/results require writable pages. Kernel threads cannot
use this API. Handles are scoped to the PID; task exit/fault releases them all.

| Operation | Value | Buffer/result |
|---|---:|---|
| INFO | 0 | Exactly 40 writable bytes, `g_network`; returns 0 |
| DNS_START | 1 | NUL-terminated name including NUL, 1..254 bytes; returns handle |
| DNS_RESULT | 2 | Four writable bytes for IPv4; 0 success, -11 pending |
| TCP_CONNECT | 3 | Eight readable bytes: IPv4 and port as uint32; returns handle |
| TCP_STATUS | 4 | 0 connected, -11 connecting, negative terminal error |
| TCP_SEND | 5 | Up to 16384 readable bytes; returns accepted bytes or -11 |
| TCP_RECEIVE | 6 | Up to 16384 writable bytes; bytes, -11 pending or 0 EOF |
| TCP_CLOSE | 7 | Abort and release the connection |
| DNS_CLOSE | 8 | Cancel/release a query |

An IPv4 integer is in dotted-octet order (`10.0.2.15` is `0x0a00020f`).
INFO has eight uint32 fields (state, address, mask, gateway, dns, received, sent,
dropped), six MAC bytes and two reserved zero bytes. State: 0 offline, 1 DHCP,
2 configured. Errors include -9 stale/foreign handle, -14 buffer, -22 argument,
-24 slot limit, -90 message limit, -100 offline, -104 reset, -110 timeout,
-111 refused and -113 route/server unavailable. User helpers add -71 HTTP parse
or status error, -95 unsupported protocol/encoding and -125 cancellation.

## Verification and references

`make test` includes private-disk QEMU boots with PCnet/NAT, a controlled Ethernet
peer and no NIC. `tools/network_test.py ISO DISK live` separately exercises a real
external domain/HTTP server and compares the saved response with a host fetch.
The live test requires internet access and is deliberately excluded from the
deterministic regression target. Reports contain the exact tested ISO hash.

Primary specifications used to write the original implementation:

- [AMD Am79C970A datasheet, publication 19436](https://www.amd.com/content/dam/amd/en/documents/archived-tech-docs/datasheets/19436.pdf)
- [ARP, RFC 826](https://www.rfc-editor.org/rfc/rfc826.html), [IPv4, RFC 791](https://www.rfc-editor.org/rfc/rfc791.html), [UDP, RFC 768](https://www.rfc-editor.org/rfc/rfc768.html)
- [DHCP, RFC 2131](https://www.rfc-editor.org/rfc/rfc2131.html), [options, RFC 2132](https://www.rfc-editor.org/rfc/rfc2132.html)
- [DNS, RFC 1035](https://www.rfc-editor.org/rfc/rfc1035.html)
- [TCP, RFC 9293](https://www.rfc-editor.org/rfc/rfc9293.html), [HTTP/1.1, RFC 9112](https://www.rfc-editor.org/rfc/rfc9112.html)
- [QEMU networking options](https://www.qemu.org/docs/master/system/invocation.html)
- [VirtualBox 7.2 adapter and NAT configuration](https://docs.oracle.com/en/virtualization/virtualbox/7.2/user/networkingdetails.html)
