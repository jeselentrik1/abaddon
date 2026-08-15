#include "favoritegifs.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

#include <glibmm/base64.h>

namespace {

class ProtoReader {
public:
    ProtoReader(const uint8_t *data, size_t len)
        : m_p(data)
        , m_end(data + len) {}

    bool Done() const { return m_p >= m_end; }

    bool ReadTag(uint32_t &field, uint32_t &wire) {
        uint64_t tag = 0;
        if (!ReadVarint(tag)) return false;
        field = static_cast<uint32_t>(tag >> 3);
        wire = static_cast<uint32_t>(tag & 7);
        return true;
    }

    bool ReadVarint(uint64_t &out) {
        out = 0;
        int shift = 0;
        while (m_p < m_end && shift < 64) {
            const uint8_t b = *m_p++;
            out |= static_cast<uint64_t>(b & 0x7F) << shift;
            if ((b & 0x80) == 0) return true;
            shift += 7;
        }
        return false;
    }

    bool ReadBytes(std::string &out) {
        uint64_t len = 0;
        if (!ReadVarint(len)) return false;
        if (m_p + len > m_end) return false;
        out.assign(reinterpret_cast<const char *>(m_p), static_cast<size_t>(len));
        m_p += len;
        return true;
    }

    bool Skip(uint32_t wire) {
        switch (wire) {
            case 0: { // varint
                uint64_t v;
                return ReadVarint(v);
            }
            case 1: // 64-bit
                if (m_p + 8 > m_end) return false;
                m_p += 8;
                return true;
            case 2: { // length-delimited
                std::string unused;
                return ReadBytes(unused);
            }
            case 5: // 32-bit
                if (m_p + 4 > m_end) return false;
                m_p += 4;
                return true;
            default:
                return false;
        }
    }

private:
    const uint8_t *m_p;
    const uint8_t *m_end;
};

bool ParseFavoriteGIF(const std::string &bytes, FavoriteGIF &gif) {
    ProtoReader r(reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size());
    while (!r.Done()) {
        uint32_t field = 0, wire = 0;
        if (!r.ReadTag(field, wire)) return false;
        if (field == 1 && wire == 0) {
            uint64_t v = 0;
            if (!r.ReadVarint(v)) return false;
            gif.Format_ = static_cast<FavoriteGIF::Format>(v);
        } else if (field == 2 && wire == 2) {
            if (!r.ReadBytes(gif.Src)) return false;
        } else if (field == 3 && wire == 0) {
            uint64_t v = 0;
            if (!r.ReadVarint(v)) return false;
            gif.Width = static_cast<int>(v);
        } else if (field == 4 && wire == 0) {
            uint64_t v = 0;
            if (!r.ReadVarint(v)) return false;
            gif.Height = static_cast<int>(v);
        } else if (field == 5 && wire == 0) {
            uint64_t v = 0;
            if (!r.ReadVarint(v)) return false;
            gif.Order = static_cast<int>(v);
        } else if (!r.Skip(wire)) {
            return false;
        }
    }
    return true;
}

bool ParseGifsEntry(const std::string &bytes, FavoriteGIF &gif) {
    ProtoReader r(reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size());
    while (!r.Done()) {
        uint32_t field = 0, wire = 0;
        if (!r.ReadTag(field, wire)) return false;
        if (field == 1 && wire == 2) {
            if (!r.ReadBytes(gif.Key)) return false;
        } else if (field == 2 && wire == 2) {
            std::string value;
            if (!r.ReadBytes(value)) return false;
            if (!ParseFavoriteGIF(value, gif)) return false;
        } else if (!r.Skip(wire)) {
            return false;
        }
    }
    return !gif.Key.empty();
}

bool ParseFavoriteGIFs(const std::string &bytes, std::vector<FavoriteGIF> &out) {
    ProtoReader r(reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size());
    while (!r.Done()) {
        uint32_t field = 0, wire = 0;
        if (!r.ReadTag(field, wire)) return false;
        if (field == 1 && wire == 2) {
            std::string entry;
            if (!r.ReadBytes(entry)) return false;
            FavoriteGIF gif;
            if (ParseGifsEntry(entry, gif))
                out.push_back(std::move(gif));
        } else if (!r.Skip(wire)) {
            return false;
        }
    }
    return true;
}

bool ParseFrecencyUserSettings(const std::string &bytes, std::vector<FavoriteGIF> &out) {
    ProtoReader r(reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size());
    while (!r.Done()) {
        uint32_t field = 0, wire = 0;
        if (!r.ReadTag(field, wire)) return false;
        if (field == 2 && wire == 2) {
            std::string favs;
            if (!r.ReadBytes(favs)) return false;
            if (!ParseFavoriteGIFs(favs, out)) return false;
        } else if (!r.Skip(wire)) {
            return false;
        }
    }
    return true;
}

void ReplaceAll(std::string &s, const std::string &from, const std::string &to) {
    if (from.empty()) return;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
}

} // namespace

std::string FavoriteGIF::GetThumbnailURL() const {
    std::string url = Src.empty() ? Key : Src;

    // Prefer direct Tenor/Klipy media so we can swap mp4→gif without
    // invalidating Discord's hashed external-proxy URLs.
    const auto extract_after = [](const std::string &s, const char *marker) -> std::string {
        auto pos = s.find(marker);
        if (pos == std::string::npos) return {};
        return "https://" + s.substr(pos);
    };

    if (auto direct = extract_after(url, "media.tenor.com/"); !direct.empty()) {
        url = std::move(direct);
        ReplaceAll(url, "AAAPo", "AAAAC");
        ReplaceAll(url, "AAAPs", "AAAAC");
        ReplaceAll(url, ".mp4", ".gif");
        ReplaceAll(url, ".webm", ".gif");
        return url;
    }

    if (auto direct = extract_after(url, "static.klipy.com/"); !direct.empty())
        return direct;
    if (auto direct = extract_after(url, "media.klipy.com/"); !direct.empty())
        return direct;

    // Prefer unsigned Discord attachment/emoji keys over expired signed src
    if (Format_ == Format::Image && !Key.empty() &&
        Key.find("ex=") == std::string::npos &&
        (Key.find(".gif") != std::string::npos || Key.find(".webp") != std::string::npos ||
         Key.find(".png") != std::string::npos || Key.find(".jpg") != std::string::npos ||
         Key.find(".jpeg") != std::string::npos)) {
        return Key;
    }

    // Last resort: try rewriting in place (may fail for hashed proxy URLs)
    if (url.find(".mp4") != std::string::npos || url.find(".webm") != std::string::npos) {
        ReplaceAll(url, "AAAPo", "AAAAC");
        ReplaceAll(url, "AAAPs", "AAAAC");
        ReplaceAll(url, ".mp4", ".gif");
        ReplaceAll(url, ".webm", ".gif");
    }

    return url;
}

std::vector<FavoriteGIF> DecodeFavoriteGIFsFromSettingsProto(const std::string &base64_settings) {
    std::vector<FavoriteGIF> result;
    if (base64_settings.empty()) return result;

    gsize len = 0;
    guchar *decoded = g_base64_decode(base64_settings.c_str(), &len);
    if (decoded == nullptr || len == 0) {
        g_free(decoded);
        return result;
    }

    std::string bytes(reinterpret_cast<char *>(decoded), len);
    g_free(decoded);

    if (!ParseFrecencyUserSettings(bytes, result)) {
        result.clear();
        return result;
    }

    std::sort(result.begin(), result.end(), [](const FavoriteGIF &a, const FavoriteGIF &b) {
        return a.Order < b.Order;
    });
    return result;
}
