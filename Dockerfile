FROM ghcr.io/wiiu-env/devkitppc:20260225

COPY --from=ghcr.io/wiiu-env/wiiupluginsystem:20260418 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libmocha:20260331 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libnotifications:20260404 /artifacts $DEVKITPRO

# SDL2, SDL2_ttf and SDL2_image (image viewer) for the app's GUI
RUN dkp-pacman -Syu --noconfirm wiiu-sdl2 wiiu-sdl2_ttf wiiu-sdl2_image && yes | dkp-pacman -Scc

WORKDIR /project
