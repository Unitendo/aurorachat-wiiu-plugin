# AuroraChat Wii U Plugin

The AuroraChat Wii U Plugin lets you use AuroraChat on Wii U outside of the app! It shows you AuroraChat messages with the use of [libnotifications](https://github.com/wiiu-env/libnotifications), though it lacks features compared to the Wii U app.
For more clients and stuff, see the [main repo](https://github.com/Unitendo/aurorachat).  
The license, code of conduct, and security/contributing guidelines in the main repo also apply here.

This repository is **open** for contributions! If you'd like to, you may open a PR or an issue, contributing helps us as we develop AuroraChat!

## Installation

(`[ENVIRONMENT]` is a placeholder for the actual environment name.)

1. Copy the file  `AuroraChatPlugin.wps` into `sd:/wiiu/environments/[ENVIRONMENT]/plugins`.
2. Requires the [WiiUPluginLoaderBackend](https://github.com/wiiu-env/WiiUPluginLoaderBackend) in `sd:/wiiu/environments/[ENVIRONMENT]/modules`.
3. Requires the [NotificationModule](https://github.com/wiiu-env/NotificationModule) in `sd:/wiiu/environments/[ENVIRONMENT]/modules`.

## Building

For building you need:

* [wut](https://github.com/devkitpro/wut)
* [wups](https://github.com/Maschell/WiiUPluginSystem)

## Building Using Docker

It's possible to use a docker image for building. This way you don't need anything installed on your host system.

Build the Docker image (only required once):

```bash
docker build . -t aurorachat-wiiu-plugin-builder
```

Build with logging enabled:

```bash
docker run -it --rm -v ${PWD}:/project aurorachat-wiiu-plugin-builder make DEBUG=1
```

Clean the build files:

```bash
docker run -it --rm -v ${PWD}:/project aurorachat-wiiu-plugin-builder make clean
```

## Formatting the Code Using Docker

The following command formats all `.cpp` and `.h` files in `src`:

```bash
find src -type f \( -name '*.cpp' -o -name '*.h' \) ! -print0 |
xargs -0 docker run --rm \
    --user "$(id -u):$(id -g)" \
    -v "$PWD:/src" \
    -w /src \
    ghcr.io/wiiu-env/clang-format:13.0.0-2 \
    -i
```
