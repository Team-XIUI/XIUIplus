# Zone Name

Announces each zone the player enters: the whole name fades in while sliding up, holds, then fades out while sliding down. The text is a module text profile.

**Start in:** [zone_name_module.cpp](../../src/runtime/modules/zonename/zone_name_module.cpp), [zone_name_module.hpp](../../src/runtime/modules/zonename/zone_name_module.hpp).

**Keep intact:** The name comes from the party source's local member, which clears while zoning, so the animation starts once the new zone is ready. The default face is the `Highwind` family, saved by name like an installed font. Its embedded copy ([assets/fonts](../../assets/fonts/README.md)) is registered privately when the family is first drawn, so it works without the font installed. The profile does not inherit or follow global HUD typography, which would replace the banner face and size. Fading scales the alpha of the text colors and every effect color together. Edit mode and the open settings page hold the finished name; changing an animation setting or `/xiuiplus zonename preview` replays it.

**Focused check:** `./scripts/check.ps1 -Name new-features` (frame timing cases are in `tests/modules/zone_name_checks.cpp`).

[Shared foundations](../foundations.md) · [Check commands](../../tests/README.md)
