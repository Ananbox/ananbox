# Ananbox

**Another rootless Android container on android**

Ananbox is a fork version of Anbox, with some modifications to get it run on Android rootlessly. And proot is now used to enforce the storage isolation.

## Status
WIP. The container can boot, but still buggy.

Most of the Android security features are missing becuase the current implementation of binder inside the container.

### Supported System Component
- Binder
- Graphics (forked from anbox)
- Wifi Simulation (still buggy)

### Supported Host Android version
Android 11, 12

(More version is coming soon.., theoretically it can support android 7.0 and newer)

### Supported Architecture
- x86_64
- arm64

## Feature
- FOSS (Both the app & the internal ROM), you can build everything from source, and everything is under your control.
- Customizable, you can customize both the app and the internal ROM. 

## How to use

Build or Download the app and the rootfs.7z of corresponding architecture. The app provides the option to import the ROM the first time you boot. 

Click the bottom-right button to launch the Settings Activity, where you can shutdown the container gracefully.

## Debug

System.log is under `<internal storage>/rootfs/data`, proot.log is under `<internal storage>`

## Preview

![demo](https://github.com/Ananbox/ananbox/assets/6512977/2c63d517-5bf2-48bb-ac71-42aa809cffed)

