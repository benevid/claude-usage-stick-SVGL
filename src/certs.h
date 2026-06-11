#pragma once

// Root CA bundle for all HTTPS endpoints (api.anthropic.com, status.claude.com).
// Multiple roots so the device survives a server-side CA rotation:
//   GlobalSign Root CA      — current api.anthropic.com anchor (expires 2028-01-28)
//   ISRG Root X1            — Let's Encrypt, current status.claude.com anchor (expires 2035-06-04)
//   DigiCert Global Root G2 — common rotation target (expires 2038-01-15)
extern const char CA_BUNDLE[];
