# Contributing to LoRagotchi-RX

Target this repository, not DX or MeshCore upstream. Use focused branches and
include RX host-test and explicit board-build results. Hardware acceptance and
publication are separate from compilation. No automatic merge or release.

Receiver code belongs under `examples/companion_radio/LoRagotchiRX*` and `ui-rx/`.
Keep sender-side DX changes separate; preserve the Aquarium LRX/1 contract or
version changes deliberately. Follow the inherited two-space C++ style.

Historical MeshCore contribution guidance remains in Git history.
