Editing the embedded web UI (SPA)

The single-page app that the firmware serves lives in plain HTML at:

- `components/main/web/index.html`

During CMake configure the project automatically converts this HTML into a C
header (so the firmware can serve it from flash). The converter is
`tools/embed_web.py` and the generated header is written into the build tree
under the component's generated directory, for example:

```
build/esp-idf/main/generated/web_index.h
```

Quick workflow

- Edit the SPA in `components/main/web/index.html`.
- Re-generate the header and build the firmware (the build runs the generator
  automatically):

```bash
idf.py build
```

- Run the tests (unit tests use the generated header too):

```bash
idf.py -DTEST=true build
```

- Flash + monitor (device required):

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

Manual regeneration

If you prefer to regenerate the header manually (without a full CMake run),
you can run the converter script directly:

```bash
python3 tools/embed_web.py components/main/web/index.html build/esp-idf/main/generated/web_index.h
```

Notes

- Do not commit generated header files into the repository. The project
  intentionally keeps the editable `index.html` under `components/main/web/`
  and generates the C header into the build tree during configure/build.
- If you want CI to validate changes to the SPA, have your CI run:
  `idf.py -DTEST=true build` so the generated header is created and unit tests run.
