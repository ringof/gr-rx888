# librx888 v0.0 staging area

These four files are the librx888 v0.0 drop-in for the `rx888-tools`
repository. They live in this tree only so that:

1. CI can build librx888 + gr-rx888 in a single job without depending
   on librx888 being merged into rx888-tools yet.
2. `docker/Dockerfile` has a stable, in-tree source for the library.
3. Anyone reading gr-rx888 can see the exact ABI it's coded against.

**This is a staging copy, not the canonical source.** Once librx888 is
merged into `rx888-tools` and tagged, this directory gets deleted and
CI / Docker switch to fetching from a `rx888-tools` release. Until
then, keep these files in sync with whatever's installed in production.

## Files

- `librx888.h` — public header (public ABI listed in `../expected-abi.txt`)
- `librx888.c` — implementation (lifted from `rx888-tools/src/rx888_stream.c`)
- `rx888_stream.c` — thin CLI wrapper that links librx888 (proves the
  library works end-to-end)
- `Makefile` — replaces `rx888-tools/Makefile`; builds librx888 +
  binaries together

## How CI uses them

```sh
# Clone rx888-tools for ezusb.c + rx888.h, then overlay our librx888:
git clone --depth 1 https://github.com/ringof/rx888-tools /tmp/rx888-tools
cp tests/librx888/librx888.h /tmp/rx888-tools/include/
cp tests/librx888/librx888.c /tmp/rx888-tools/src/
cp tests/librx888/rx888_stream.c /tmp/rx888-tools/src/
cp tests/librx888/Makefile /tmp/rx888-tools/
(cd /tmp/rx888-tools && make && sudo make install && sudo ldconfig)
```

The same dance is in `.github/workflows/ci.yml` and `docker/Dockerfile`.
