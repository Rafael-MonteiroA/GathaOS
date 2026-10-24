# Gatha Browser (Stages 12-13)

Start > Browser launches the original ring-3 browser. Its blue/silver controls
use the existing Gatha toolkit. It is a bounded text-oriented browser, not a
modern web engine. No external guest libraries, DOM engine or JavaScript runtime
are included.

## Using it

- Enter an HTTP or HTTPS address and press Enter or Go. A bare domain defaults
  to HTTPS. Ctrl+L selects the address; Ctrl+A selects text in the address field.
- Blue underlined links open in the same window. Hover shows their destination.
- Back/Forward (also Alt+Left/Right) keep up to 16 locations and scroll positions
  for this session. Navigation refetches pages; there is no cache. Reload/F5
  refetches the current page. Home opens the built-in help/start page.
- Click the page to leave the address field, then use arrows, Page Up/Down,
  Home/End or the right-hand scrollbar. Resizing reflows the text.
- Stop or Escape cancels a request. Closing during a request releases the task,
  window, heap, socket and DNS query. Other applications remain schedulable.
- The previous page remains visible while loading. On failure it and its
  effective address are restored; the bottom bar shows the error.

The Home page links to Example Domain and Google's public robots.txt. Reading
that Google file over authenticated HTTPS does not provide Google's search
interface. Stage 13 adds limited GET forms; JavaScript remains unsupported.
Use Forms demo or `about:forms` for an offline example; see [forms.md](forms.md).

## Renderer and resource bounds

The original scanner handles basic headings, paragraphs, divisions, lists,
line breaks, preformatted text, links, image alternative text and literal ASCII
ID fragment targets. Tables are flattened into text. Script/style/iframe bodies,
comments and head metadata are skipped; the title is shown above the page.
HTML whitespace collapses outside pre; text/plain preserves line breaks.
Common entities and numeric references decode. UTF-8 Latin accents and several
punctuation marks become ASCII approximations; other characters display as '?'.
This is not Unicode typography or an implementation of the full HTML parsing
algorithm. Malformed markup uses a bounded best-effort scan, without execution.

| Resource | Limit |
|---|---:|
| HTTP response body per browser request | 512 KiB |
| Visible document text | 65,535 ASCII characters |
| Layout rows | 8,192 |
| Links / ID targets | 128 / 128 |
| URL including terminator | 1,024 bytes |
| Request path including query | 767 bytes |
| History entries per window | 16 |
| Automatic redirects | 5 |

The display reports truncation when the text/row cap is reached. Links beyond
the link cap remain plain text. The document and replacement are separate heap
allocations, so a failed fetch/parse allocation leaves the old page intact.
Only visible rows are drawn, using the original 8x16 font.

Only Content-Type text/html and text/plain are displayed; missing/other types
are rejected, as are explicit charsets other than UTF-8 or US-ASCII. HTTP 204 is
empty. HTTP 4xx/5xx bodies with a supported type can be displayed with an error
status. No MIME sniffing, downloads, local-file browsing, file association,
images, audio/video, CSS, JavaScript, POST forms, cookies, credentials, clipboard,
tabs, bookmarks, persistent history, printing or password management.

## URLs, HTTP and TLS

libgatha/url.c resolves relative paths, query-only references, network paths and
fragments against the final response URL. Literal dot segments are removed.
Scheme/host case is normalized. Fragments are never sent to the server; a
same-document fragment jump does not refetch. Percent escapes are validated but
not decoded. Literal ASCII IDs are supported; percent-encoded IDs are not.
Only HTTP(S) with a DNS host or dotted IPv4 and optional numeric port is accepted.
No userinfo, IPv6 literals, IDNs, whitespace/control characters or backslashes.
Redirects use status 301/302/303/307/308 and a bounded Location value. Automatic
HTTPS-to-HTTP downgrade is blocked. An explicitly entered HTTP URL remains
available and is labeled unencrypted. There is no HSTS database.

The shared g_http_fetch API exposes status, Content-Type and Location and accepts
a caller body limit. It owns no navigation policy. g_http_get remains Terminal's
200-only, 4 MiB downloader. Framing checks and cancellation are shared. Each
request has the existing 120-second post-DNS deadline; a redirect sequence can
therefore take longer. Stop/close remains available during each network wait.
There is no proxy, compression, HTTP/2, HTTP/3, authentication or refresh header.
HTML base tags and meta refresh are ignored.

HTTPS uses the exact Stage 11 TLS 1.2 and trust profile described in
[https.md](https.md). Certificate failure has no bypass. Roots compiled into
Browser are the same two public roots as Terminal; disk files cannot replace
them. RDRAND and a correct guest UTC RTC are required. TLS 1.3/ECDSA-only sites,
many certificate chains and modern websites remain unsupported. Certificate
revocation checks, root updates and a security audit remain absent.

## Verification and references

tools/browser_native_test.py compiles the actual URL/document sources with
AddressSanitizer/UndefinedBehaviorSanitizer. It checks RFC URI vectors,
renderer/charset bounds and 12,000 deterministic mutation inputs. The host-only
test executable uses a fixed base to avoid older ASan/WSL PIE startup collisions.
Guest memory layouts are unchanged by that host compiler option.

tools/browser_test.py drives real PS/2 events in QEMU, checks pixels, redirects,
links, history, scrolling, resize, failures, cancellation, offline operation and
certificate rejection. Its separate live mode opens Example Domain and Google's
HTTPS robots.txt. Release measurements and evidence belong in stage-12-report.md.
Private fixture roots never enter the production browser.

Specifications consulted for the original subset:
[RFC 3986, URI resolution](https://www.rfc-editor.org/rfc/rfc3986.html) and
[HTML syntax](https://html.spec.whatwg.org/multipage/syntax.html).
These references do not imply full standards conformance.
