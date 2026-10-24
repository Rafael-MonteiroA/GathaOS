# Browser GET forms (Stage 13)

Start > Browser > Forms demo opens `about:forms`. Type a short search and press
Enter or Preview. The next page shows the encoded query locally: it does not
contact a search engine or send anything over the network. Back returns to a
fresh form. This demo also works without a network adapter.

On compatible HTTP/HTTPS pages, the same controls submit actual GET requests to
the form action and render the response. The form action is resolved relative to
the final page URL after redirects; omitted/empty actions use the current page.
The new query replaces an existing action query. Fragments stay in navigation
history but are omitted from the request. Submitting the current URL still
fetches it again. HTTPS-to-HTTP form actions are rejected before any request.

## Supported controls

- `<form method="get">`, also the default when method is omitted.
- `<input type="text">` (including omitted type) and `type="search"`.
- Hidden name/value pairs, in document order, including repeated names.
- `<input type="submit">` and `<button type="submit">` (button's default type).
  Only the chosen submit button contributes its name/value. Button captions are
  independent of their submitted values.
- Disabled controls are omitted; readonly text is submitted but cannot be edited.
- Required editable fields cannot submit empty values. The first invalid field
  gets focus. Text `maxlength` limits typing, within the browser's 255-byte cap.
- The hidden `_charset_` field, when present, submits `UTF-8`.

Tab/Shift+Tab move through enabled visible controls and the address field.
Ctrl+A, selection, Backspace/Delete and arrows use the existing text editor.
Enter in text uses the first submit button; if it is disabled, nothing is sent.
Without a submit button, Enter submits only a form with one text/search field.
Enter or Space activates a focused submit button. Ctrl+L selects the address.
Focus scrolls controls into view; normal and maximized windows use the same data.
Back/Forward refetch pages and reset their field values. There is no form cache.

Name/value pairs use application/x-www-form-urlencoded: spaces become `+`,
reserved bytes become `%HH`, and fields are joined with `&`. The renderer keeps
the prior page and edited fields on network/format failure. Existing Stop/Escape
cancellation, redirect and certificate rules continue to apply. Serial diagnostics
include requested URLs and therefore GET query values.

## Deliberate bounds

At most 16 forms and 64 controls per document; 127-byte names, 255-byte values,
63-character button captions. Form names, initial values and actions accept
printable ASCII only. Unsupported Unicode/control bytes or entities are rejected,
never transliterated into different submitted data. Keyboard input remains US
ASCII. The existing 1,024-byte URL and 767-byte path/query limits apply after
encoding; oversized forms send nothing. Captions may be shortened for display.

Only the small form subset above is supported. POST, passwords, uploads,
checkboxes/radios, select/textarea, fieldsets, external `form=` associations,
submit overrides, alternate targets/encodings, pattern/minlength validation,
inline input/submit handlers, template and base elements are not implemented. Their forms
are blocked with an explanation, rather than partially submitted. Nested forms,
exceeded control/form limits and truncated documents are also blocked. The HTML
scanner is still a bounded subset, not a complete
standards parser; arbitrary malformed pages are not guaranteed interoperable.

There is no JavaScript, CSS, cookies, login or full Google Search interface.
This stage does not broaden the existing TLS certificate/cipher profile.

## Verification

`make test` includes `tools/forms_test.py`. A host HTTP server records the actual
guest request bytes and independently parses their query; no guest-side success
message substitutes for the server check. PS/2 events edit fields, use focus and
submit; screenshots/pixel checks verify result text. Tests cover query escaping,
hidden/duplicate names, selected buttons, ownership, required/maxlength,
readonly/disabled controls, repeated submissions, navigation, unsupported forms,
window resizing, the offline demo and memory reclamation. Maximized toolbar
clicks also verify that an obscured desktop icon cannot launch Files behind
the browser; the window manager now checks whether a window covers the icon.

The host test compiles the same guest parser/serializer with ASan/UBSan and
checks form semantics, limits, rejected destinations/data and 6,000 structured
HTML mutations, in addition to the existing 12,000 URL/document mutations.
The desktop soak edits and submits the local demo in every cycle.
Measured release results belong in stage-13-report.md.

Original code was written against the
[HTML form model](https://html.spec.whatwg.org/multipage/form-control-infrastructure.html)
and [URL-encoded serialization](https://url.spec.whatwg.org/#application/x-www-form-urlencoded).
These references do not imply complete standards conformance.
