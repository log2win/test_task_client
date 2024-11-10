# Setup

## Linux

```console
mkdir build
cd build
cmake -DCMAKE_PREFIX_PATH=/path/to/your/qt/version/compiler/ ..
cmake --build .
```

then start client

```console
./client
```

# Settings

Some settings as server address and port can be changed in settings.hpp