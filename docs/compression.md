# Compressed HTTP responses (Stage 14)

Browser and Terminal automatically request `Accept-Encoding: gzip, deflate` and
decode supported response bodies before parsing HTML or saving a file. Plain
responses remain supported. The User-Agent is GathaOS/14. This improves transfer
compatibility; it does not change the HTML renderer, form subset or TLS profile.

## Original decoder

libgatha/inflate.c is an original freestanding implementation of DEFLATE stored,
fixed-Huffman and dynamic-Huffman blocks. It validates canonical code lengths,
repeat counts, end markers, reserved symbols, stored-length complements and
back-reference distances. Overlapping matches are copied one byte at a time.
The advertised zlib window and each gzip member's separate history are enforced.
No zlib or other third-party decoder is linked into GathaOS.

Gzip accepts optional extra/name/comment fields, verifies an optional header CRC,
checks each member's CRC32 and ISIZE, and supports at most 32 concatenated members.
Optional header data is bounded to an 8 KiB header prefix per member (plus FHCRC).
Reserved flags, trailing garbage and incomplete members fail. Zlib-wrapped
deflate checks CMF/FLG, window size and Adler-32; preset dictionaries, raw-deflate
fallbacks and concatenated zlib streams are unsupported. The HTTP `x-gzip` alias
and coding names with different ASCII case are accepted.

The pure decoder takes caller-owned input/output buffers and allocates nothing.
It returns output length only after all integrity checks succeed. Host tests
compile this exact source under ASan/UBSan. Independent host Python zlib/gzip
produce stored/fixed/dynamic, empty, multi-block, optional-header and boundary
vectors, plus 20,000 deterministic mutations. Cancellation/deadline callbacks
and guard bytes are checked as well. Host libraries are test tools only.

## HTTP integration and limits

Content-Length, chunked and connection-close framing are processed before
decompression. The existing HTTP/TLS checks remain in effect. Duplicate
Content-Encoding fields, stacked codings, Brotli and other unknown codings are
rejected. Trailers cannot introduce/change framing, coding, content type or
redirect destination. Identity and absent Content-Encoding remain valid.
Bodyless 204 and redirect responses keep the existing no-body handling.

| Resource | Browser | Terminal wget |
|---|---:|---:|
| Decoded body limit | 512 KiB | 4 MiB |
| Encoded body limit | 576 KiB | 4 MiB + 64 KiB |

The extra 64 KiB allows compression overhead; both caps are enforced independently.
An expanding body stops at the decoded cap, regardless of its compressed size or
declared gzip length. The whole encoded body and a separate decoded buffer are
held in the user heap. Output is allocated at the caller's cap; the existing
geometric input buffer may reserve more than the encoded byte count. Memory is
bounded and released on success, failure, cancellation or process exit.

The same 120-second post-DNS deadline covers network and decoder work. A poll
runs initially and at most every 16,384 input/output byte operations, including
empty-block/header input. Escape/Stop can cancel; closing releases process-owned
buffers and sockets. A failed decode never replaces the displayed page or an
existing downloaded file. Serial output reports successful encoded/decoded byte
counts for diagnosis. There is no streaming display or partial file save.

## Real-boot checks

`tools/compression_test.py` serves independently compressed pages to the actual
QEMU guest. PS/2 events open pages, edit and submit a GET form; framebuffer pixels
check the decoded result. Binary Terminal downloads are compared byte-for-byte
with independent expected files after shutdown. Cases include every HTTP framing
mode, gzip members/optional headers, zlib, empty bodies, corrupt checksums,
truncation, unsupported coding, output expansion limits and forbidden trailers.
Escape and window close are injected during decoding, followed by a successful
request and baseline memory checks. Logs and request headers are saved as evidence.
The full desktop/network/TLS/forms regressions still run; external HTTP/HTTPS
tests remain separate from deterministic `make test`.

The full regression exposed an existing ATA flush timeout before the desktop
started in a 64 MiB guest. The ATA wait previously used CPU iterations as its
deadline. It now reserves PIT channel 2 for polled ~55 ms one-shots, allowing
600 intervals (about 33 seconds) independently of CPU speed and IRQ delivery.
Channel 0 remains the scheduler clock; the speaker is disabled during the wait
and its control bits are restored. A large iteration ceiling remains only as a
fallback for a broken PIT. Disk operations are still synchronous, and a genuine
timeout still disables filesystem writes; no flush error is ignored or retried.
Host I/O injection checks delayed readiness beyond the old ceiling, expiration,
DRQ/error/absent-device handling and cleanup. QEMU regression checks actual disk
writes/persistence and the scheduler after this change. PIT port behavior was
checked against the primary [QEMU PC hardware model](https://github.com/qemu/qemu/blob/master/hw/audio/pcspk.c).

## Remaining work and references

No Brotli, HTTP/2/3, informational responses, cookies, new TLS algorithms, CSS,
images or JavaScript are added. Google Search and modern web applications remain
unsupported. The loader's remaining kernel payload space must be considered
before larger browser features. See stage-14-report.md for measured results.

Written from format specifications, without copying reference implementations:
[DEFLATE RFC 1951](https://www.rfc-editor.org/rfc/rfc1951),
[gzip RFC 1952](https://www.rfc-editor.org/rfc/rfc1952), and
[zlib RFC 1950](https://www.rfc-editor.org/rfc/rfc1950).
