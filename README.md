# KalaFont

**KalaFont** compiles .ttf and .otf fonts into .kfd (kalafontdata) - a runtime-ready bitmap or per-glyph binary of vertices, indices, uvs and other relevant data related to the font that was passed. Locks to one base height and strips out all metadata of the original font file.

**KalaFont** uses FreeType to help with font parsing but the runtime-ready binary or its importer header no longer requires FreeType. Use the [importer](https://github.com/KalaKit/KalaHeaders/blob/main/import_kfd.hpp) to import the binary to your target executable.

This cli was built with the help of the [KalaCLI](https://github.com/kalakit/kalacli) library.

---

## Links

[Donate on PayPal](https://www.paypal.com/donate/?hosted_button_id=QWG8SAYX5TTP6)

[Official Discord server](https://discord.gg/jkvasmTND5)

[Official Youtube channel](https://youtube.com/greenlaser)

---

## Docs

[How to build from source](docs/build_from_source.md)

[External libraries](docs/external_libraries.md)

[Lost Empire Entertainment and KalaKit ecosystem](docs/ecosystem.md)