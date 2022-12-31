(work in progress)

## Releasing

When releasing a new version of `ted`:

- Update `TED_VERSION` in `ted.h`.
- Update `Version` in `control` for the `.deb` file.
- Run `make ted.deb` on Debian/Ubuntu.
- Run `make.bat release` on Windows.
- Open installer project, and increment version number.
- Build `ted.msi`.
- Create a new release on GitHub with `ted.deb` and `ted.msi`.
