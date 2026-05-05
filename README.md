# OpenterfaceCore

Shared C library for Openterface KeyMod — keyboard, mouse, and macro packet building for the CH9329 HID-over-serial protocol.

## What's here

| Path | Purpose |
|------|---------|
| `include/keymod.h` | Public API header |
| `src/keymod.c` | Implementation (HID codes, packet builders, token parser) |
| `cli/keymod_cli.c` | CLI demo / debugging tool |
| `tests/keymod_test.c` | Unit tests |
| `ios/KeyModWrapper/` | Swift wrapper for iOS integration |
| `android/` | JNI wrapper + Java helper for Android integration |

## API

### Packet builders

```c
uint8_t pkt[14];
km_build_keyboard(pkt, KM_MOD_CTRL, (uint8_t[]){0x06}, 1);  // Ctrl+C

uint8_t mpkt[11];
km_build_mouse_rel(mpkt, KM_MS_BTN_LEFT, 0, 0, 0);          // left click

uint8_t pr[28];
km_build_press_release(pr, KM_MOD_NONE, 0x28);              // Enter press+release
```

### HID code lookup

```c
int code = km_hid_code("Enter");           // 0x28
int code = km_hid_code_for_char('!', &s);  // 0x1E, s=1 (needs shift)
```

### Macro parsing

```c
km_parsed_token_t tokens[32];
int n = km_parse_macro("<CMD>V</CMD>", tokens, 32);
// tokens[0].hid_code = 0x19 (V), tokens[0].modifiers = KM_MOD_GUI
```

## Build

```bash
mkdir build && cd build
cmake ..
make
./keymod_test      # unit tests
./keymod_cli       # demo output
./keymod_cli Enter # single key packet
./keymod_cli "<CTRL>C</CTRL>"  # macro packets
```

## iOS integration

Add all `.c` files and `include/keymod.h` to your Xcode target.
Create a bridging header that imports `OpenterfaceCore.h`.
Use `KeyModPacker` from Swift:

```swift
let packet = KeyModPacker.keyboard(modifiers: KeyModModifier.ctrl, keys: [0x06])
bleManager.sendTouchData(data: packet)
```

## Android integration

Add the JNI CMake to your `app/build.gradle`:

```groovy
android {
    externalNativeBuild {
        cmake {
            path "../../Openterface_Core/android/CMakeLists.txt"
        }
    }
}
```

Copy `KeyModLib.java` into your package. Call from Java:

```java
byte[] packet = KeyModLib.buildKeyboardPacket(KeyModLib.MOD_CTRL, new byte[]{0x06});
bluetoothService.sendData(packet);
```

## Protocol

All packets use the CH9329 format:

- **Keyboard** (14 bytes): `57 AB 00 02 08 | mod | 00 | k1..k6 | checksum`
- **Mouse rel** (11 bytes): `57 AB 00 05 05 | 01 | btn | dx | dy | wheel | checksum`
- **Checksum**: sum of all bytes except last, masked to 8 bits
