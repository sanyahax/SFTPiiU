#pragma once

// Known-answer tests for every primitive. Run once at startup on the console:
// a failure (e.g. an endianness bug) must stop the server rather than produce
// weak or broken crypto. Returns the name of the first failing test, or
// nullptr when everything passes.
const char *crypto_selftest();
