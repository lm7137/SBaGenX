# Contributing to SBaGenX

## Where to start

Use GitHub Discussions for:

- open-ended ideas
- presets and example sharing
- roadmap discussion
- questions that are not yet concrete bugs

Use GitHub Issues for:

- reproducible bugs
- documentation gaps
- packaging problems
- concrete feature proposals

## Good first contributions

- improve README clarity and screenshots
- add or refine example `.sbg` / `.sbgf` files
- improve installation or release-channel documentation
- strengthen tests around existing behavior
- polish packaging or runtime-bundle checks

## Engineering rules for this repo

- behavior-affecting changes should land in `sbagenxlib` first when possible
- keep the CLI and GUI as frontends around the shared engine
- do not silently change output semantics without updating docs and examples
- if you change shared Rust/C FFI structs, update the ABI validation path too

## Pull requests

A good pull request should:

- state the user-visible problem clearly
- explain the chosen fix briefly
- mention validation performed
- update docs/examples/tests when the change changes behavior

## Release channels

- Stable releases are for normal users
- Alpha releases are for testers and early adopters

Please keep that distinction clear when proposing release-facing changes.

## Code of conduct

By participating in this project, you agree to follow [`CODE_OF_CONDUCT.md`](CODE_OF_CONDUCT.md).
