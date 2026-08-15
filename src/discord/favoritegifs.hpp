#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct FavoriteGIF {
    enum class Format : int {
        None = 0,
        Image = 1,
        Video = 2,
    };

    std::string Key;   // URL posted to the channel (tenor/klipy/cdn)
    std::string Src;   // preview / media URL
    Format Format_ = Format::None;
    int Width = 0;
    int Height = 0;
    int Order = 0;

    // Best-effort URL GdkPixbuf can load (gif/webp/png/jpeg), not mp4
    [[nodiscard]] std::string GetThumbnailURL() const;
};

// Decode FrecencyUserSettings (settings-proto type 2) and extract favorite_gifs.
// Returns empty on parse failure.
std::vector<FavoriteGIF> DecodeFavoriteGIFsFromSettingsProto(const std::string &base64_settings);
