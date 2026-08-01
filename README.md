# moglang/fs

Filesystem utilities for Mog. The canonical import is `github.com/moglang/fs`.
This native package supports ABI 3 and Mog runtime `^0.1.4` on Linux x86_64,
Linux ARM64, and macOS ARM64.

Install from a Mog project directory. Git dependencies build from source and
therefore require CMake and a C++17 compiler:

```bash
mog add github.com/moglang/fs@v0.1.2
```

```mog
const fs = @import("github.com/moglang/fs")

fs.mkdir("output")
fs.writeText("output/message.txt", "hello")
fs.copyFile("output/message.txt", "output/backup.txt", false)
print(fs.readText("output/backup.txt"))
print(fs.fileSize("output/backup.txt"))
```

## API notes

- `readText` and `writeText` preserve bytes without newline translation. They are
  intended for text; the current native ABI has no byte-array value type.
- `copyFile(from, to, overwrite)` copies one regular file. Passing `false` fails
  if the destination exists; passing `true` replaces it.
- `mkdir` creates parent directories as needed.
- `rename` uses the operating system's filesystem rename operation.
- **`remove` recursively removes a path and all descendants.** A missing path is
  accepted. Do not pass an untrusted or insufficiently scoped path.
- Directory listing is not exposed because ABI 3 cannot return collections.

Build with CMake. The complete public contract is in `package.api.mog`. The
package is licensed under GPL-3.0-only; see `LICENSE`.
