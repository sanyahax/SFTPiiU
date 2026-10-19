An SFTP server (SSH) for the Wii U: an Aroma plugin that runs in the background and an app with a live view, file manager and editor.

![Dashboard](https://github.com/sanyahax/SFTPiiU/raw/main/docs/screenshots/dashboard.png)

## Install

1. Download the **SFTPiiU zip** below.
2. Copy the `wiiu` folder from the zip to the root of your SD card (merge it with the existing one).
3. Restart the console and open the **SFTPiiU** app to see the address and password.
4. Connect with any SFTP client (WinSCP, FileZilla, Cyberduck, `sftp`), port 22, any user name.

Requires Aroma. The zip contains:

| File | Purpose |
|---|---|
| `wiiu/apps/SFTPiiU.wuhb` | the app |
| `wiiu/environments/aroma/plugins/SFTPiiU.wps` | the plugin that keeps the server running in the Wii U Menu and in games |

Only want the server while the app is open? Leave out the `.wps`, or switch off **Keep running in the background** in the app's settings.

## Features

- Encrypted SFTP with password or `ssh-ed25519` key login: `curve25519-sha256`, `chacha20-poly1305`, `aes-ctr`, strict KEX
- Access to the SD card, internal storage (MLC), USB and SLC (SLC read-only by default)
- App with dashboard (speed, live transfers), credentials, drives, file browser and settings, in all twelve Wii U languages
- File actions on the console: rename, copy, move, delete, new folder or file (off by default)
- Text editor for config files, image viewer, hex view
- **Keep running in the background:** one switch in the app installs and enables the plugin, with the same password and host key

made by sanyahax (github @sanyahax) - ajk.dev
