# S001 — CLI update checker and MSI installer

- **Status:** In review · **Order:** 2 · **Depends on:** S000

## Story

As a Windows user of `hitokoto`, I want a CLI updater (`hitokoto-win-updater-cli`) that checks my
installed `hitokoto` version against GitHub Releases and downloads and installs the newer MSI
package when available, so that I can keep `hitokoto` up to date from the command line.

## Acceptance scenarios

**Scenario: detect installed version and scope**
- Given `hitokoto` is installed via its MSI (per-user or per-machine)
- When the updater queries the Windows Uninstall registry entries (or fallback `hitokoto.exe --version`)
- Then it reports the installed `MAJOR.MINOR.PATCH` version and install scope (`per-user` or `per-machine`)
- And when `hitokoto` is not installed, it reports `not installed`

**Scenario: parse and compare GitHub release versions**
- Given the GitHub Releases API JSON for `Daniel-Fuerst/hitokoto` (`/releases/latest`)
- When `src/core` parses the release tag (e.g. `v0.6.2`) and assets
- Then it extracts the SemVer version, locates `hitokoto-<version>-x64.msi` and `SHA256SUMS` (and optional asset `digest`), and determines whether an update is available

**Scenario: up-to-date installation**
- Given installed version `0.6.2` and latest GitHub release `v0.6.2`
- When `hitokoto-win-updater-cli` runs
- Then it prints the installed and latest versions, reports that `hitokoto` is up to date, downloads nothing, and exits `0`

**Scenario: check-only mode**
- Given installed version `0.6.1` and latest GitHub release `v0.6.2`
- When `hitokoto-win-updater-cli --check` runs
- Then it prints the installed and latest versions, reports that an update is available, downloads nothing, and exits `0`

**Scenario: download, verify SHA-256, and install newer MSI**
- Given installed version `0.6.1` and latest GitHub release `v0.6.2`
- When `hitokoto-win-updater-cli` runs (without `--check`)
- Then it downloads `hitokoto-0.6.2-x64.msi` and `SHA256SUMS` to a temporary directory
- And verifies the downloaded MSI's SHA-256 hash matches `SHA256SUMS` (and the GitHub asset digest if present) before running `msiexec.exe`
- And refuses to run the installer if verification fails
- And invokes `msiexec.exe /i <msi> <scope-property> /norestart` (preserving `MSIINSTALLPERUSER=1` or `ALLUSERS=1`, with `/qb` by default or `/qn` with `--silent`) and removes temporary files afterwards
