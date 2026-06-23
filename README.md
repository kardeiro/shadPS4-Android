# shadPS4 for Android

<div align="center">

**Material You 3 Expressive UI prototype for a PS4 emulator on Android**

Based on the desktop [shadPS4](https://github.com/shadps4-emu/shadPS4) project.

</div>

> ⚠️ **This is a UI-only prototype.** No PS4 game execution is implemented yet.
> The native emulator core (C++) is not yet wired up to this Android shell.
> This build only produces an APK that renders the emulator's mobile interface.

## Status

| Feature | Status |
|---|---|
| Home / Library / Compatibility / Settings screens | ✅ Implemented |
| Material You 3 Expressive theme (dynamic + fallback) | ✅ Implemented |
| DataStore-backed settings persistence | ✅ Implemented |
| Game detail screen with compatibility info | ✅ Implemented |
| About screen with attribution to upstream | ✅ Implemented |
| Native emulator core integration | ❌ Not started |
| PKG installation | ❌ Not started |
| Firmware loading | ❌ Not started |

## Tech stack

- Kotlin 2.1
- Jetpack Compose (BOM `2024.12.01`)
- Material 3 `1.4.0-alpha05` (Expressive)
- AndroidX DataStore (preferences)
- Coil for image loading
- Kotlinx Serialization
- Navigation Compose 2.8 (type-safe)
- Min SDK 26 (Android 8.0) — Target SDK 35
- Java 17 / AGP 8.7.3

## Build

The project ships with the Gradle wrapper, so you only need JDK 17.

```bash
./gradlew assembleDebug
# APK: app/build/outputs/apk/debug/app-debug.apk
```

Or build a release APK:

```bash
./gradlew assembleRelease
# APK: app/build/outputs/apk/release/app-release.apk
```

### GitHub Actions

This repo includes `.github/workflows/build.yml`. On every push or pull request
the workflow:

1. Sets up JDK 17
2. Configures the Android SDK
3. Caches Gradle dependencies
4. Builds `assembleDebug` and `assembleRelease`
5. Uploads both APKs as artifacts
6. On tag pushes (`v*`), creates a GitHub Release with the APK attached

You can also trigger it manually from the **Actions** tab → **Build APK** →
**Run workflow**.

## Project structure

```
app/src/main/java/com/shadps4/emulator/
├── MainActivity.kt            # Entry point, applies theme
├── ShadPs4App.kt              # Application class
├── data/
│   ├── model/                 # GameInfo, CompatibilityStatus, sample data
│   └── repository/            # SettingsRepository (DataStore), GameRepository
└── ui/
    ├── ShadPs4App.kt          # NavHost + Scaffold + bottom bar
    ├── theme/                 # Material 3 Expressive color/type/shape
    ├── navigation/            # Destinations + top-level nav graph
    ├── components/            # GameCard, CompatibilityBadge, ShadNavigationBar
    └── screens/
        ├── home/              # Welcome + recent + quick actions
        ├── library/           # Grid of installed games
        ├── gamedetail/        # Hero + info + compatibility
        ├── compatibility/     # Filterable list
        ├── settings/          # General / Graphics / Audio / Controls / Advanced
        └── about/             # Attribution + license
```

## Roadmap

1. ✅ UI prototype (this commit)
2. ⏳ Native core integration (NDK + CMake, Vulkan renderer surface)
3. ⏳ PKG file installation & parsing
4. ⏳ PS4 firmware loading & HLE modules
5. ⏳ On-screen touch controls + gamepad support
6. ⏳ Save states & per-game settings

## License

GPL-2.0-or-later, same as upstream shadPS4. See [LICENSE](LICENSE).

## Credits

- [shadPS4 Emulator Project](https://github.com/shadps4-emu/shadPS4) — original desktop emulator
- Material 3 Expressive design language by Google

## Disclaimer

PlayStation 4 and PS4 are trademarks of Sony Interactive Entertainment.
shadPS4 is not affiliated with or endorsed by Sony. You must dump your own
games from your own PS4.
