#include "giffavoritespicker.hpp"

#include "abaddon.hpp"
#include "util.hpp"

namespace {
constexpr int ThumbMax = 140;
constexpr int PickerWidth = 380;
constexpr int PickerHeight = 420;

void ClampThumbSize(int inw, int inh, int &outw, int &outh) {
    if (inw <= 0 || inh <= 0) {
        outw = ThumbMax;
        outh = ThumbMax;
        return;
    }
    GetImageDimensions(inw, inh, outw, outh, ThumbMax, ThumbMax);
}
} // namespace

GifPickerItem::GifPickerItem(const FavoriteGIF &gif, int thumb_w, int thumb_h)
    : m_gif(gif) {
    get_style_context()->add_class("gif-picker-item");
    set_can_focus(true);

    m_img = Gtk::manage(new LazyImage(thumb_w, thumb_h, true));
    // Static thumbs only — animating dozens of GIFs at once is too heavy
    m_img->SetAnimated(false);
    m_img->SetURL(gif.GetThumbnailURL());
    m_img->set_size_request(thumb_w, thumb_h);
    m_img->show();

    m_ev.add(*m_img);
    m_ev.set_events(m_ev.get_events() | Gdk::BUTTON_PRESS_MASK);
    m_ev.signal_button_press_event().connect([this](GdkEventButton *ev) -> bool {
        if (ev->type == GDK_BUTTON_PRESS && ev->button == GDK_BUTTON_PRIMARY) {
            activate();
            return true;
        }
        return false;
    });
    m_ev.show();
    add(m_ev);
    show();
}

const FavoriteGIF &GifPickerItem::GetGIF() const {
    return m_gif;
}

GifFavoritesPicker::GifFavoritesPicker()
    : m_main(Gtk::ORIENTATION_VERTICAL)
    , m_header(Gtk::ORIENTATION_VERTICAL)
    , m_status_box(Gtk::ORIENTATION_HORIZONTAL) {
    get_style_context()->add_class("gif-favorites-picker");
    set_modal(false);
    set_position(Gtk::POS_TOP);

    m_title.set_text("Favorites");
    m_title.set_halign(Gtk::ALIGN_START);
    m_title.get_style_context()->add_class("gif-picker-title");
    m_title.show();

    m_search.set_placeholder_text("Search Favorites");
    m_search.signal_changed().connect(sigc::mem_fun(*this, &GifFavoritesPicker::OnSearchChanged));
    m_search.show();

    m_header.set_spacing(6);
    m_header.set_margin_bottom(6);
    m_header.pack_start(m_title, false, false);
    m_header.pack_start(m_search, false, false);
    m_header.show();

    m_flow.set_homogeneous(false);
    m_flow.set_row_spacing(4);
    m_flow.set_column_spacing(4);
    m_flow.set_max_children_per_line(3);
    m_flow.set_min_children_per_line(2);
    m_flow.set_selection_mode(Gtk::SELECTION_NONE);
    m_flow.set_activate_on_single_click(true);
    m_flow.signal_child_activated().connect(sigc::mem_fun(*this, &GifFavoritesPicker::OnChildActivated));
    m_flow.show();

    m_scroll.set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
    m_scroll.set_size_request(PickerWidth, PickerHeight);
    m_scroll.add(m_flow);
    m_scroll.show();

    m_spinner.set_size_request(16, 16);
    m_status.set_text("Loading…");
    m_status_box.set_spacing(8);
    m_status_box.set_halign(Gtk::ALIGN_CENTER);
    m_status_box.pack_start(m_spinner, false, false);
    m_status_box.pack_start(m_status, false, false);

    m_main.set_spacing(4);
    m_main.set_margin_start(8);
    m_main.set_margin_end(8);
    m_main.set_margin_top(8);
    m_main.set_margin_bottom(8);
    m_main.pack_start(m_header, false, false);
    m_main.pack_start(m_scroll, true, true);
    m_main.pack_start(m_status_box, false, false);
    m_main.show();

    add(m_main);
}

void GifFavoritesPicker::Popup(Gtk::Widget &relative_to) {
    set_relative_to(relative_to);
    popup();
    m_search.grab_focus();
    LoadFavorites(false);
}

void GifFavoritesPicker::LoadFavorites(bool force_refresh) {
    if (m_loading) return;
    m_loading = true;

    m_status_box.show();
    m_spinner.start();
    m_status.set_text("Loading…");
    ClearFlow();

    Abaddon::Get().GetDiscordClient().FetchFavoriteGIFs(
        sigc::track_obj([this](std::vector<FavoriteGIF> gifs) {
            m_loading = false;
            m_spinner.stop();
            m_gifs = std::move(gifs);
            if (m_gifs.empty()) {
                m_status.set_text("No favorite GIFs");
                m_status_box.show();
            } else {
                m_status_box.hide();
            }
            RebuildGrid();
        },
                        *this),
        force_refresh);
}

void GifFavoritesPicker::ClearFlow() {
    std::vector<Gtk::Widget *> children;
    m_flow.foreach ([&children](Gtk::Widget &w) {
        children.push_back(&w);
    });
    for (auto *w : children)
        m_flow.remove(*w);
}

void GifFavoritesPicker::RebuildGrid() {
    ClearFlow();

    const Glib::ustring term = m_search.get_text().lowercase();

    for (const auto &gif : m_gifs) {
        if (!term.empty()) {
            const auto key_l = Glib::ustring(gif.Key).lowercase();
            if (key_l.find(term) == Glib::ustring::npos)
                continue;
        }

        int tw, th;
        ClampThumbSize(gif.Width, gif.Height, tw, th);
        auto *item = Gtk::manage(new GifPickerItem(gif, tw, th));
        m_flow.add(*item);
        item->show();
    }
}

void GifFavoritesPicker::OnSearchChanged() {
    RebuildGrid();
}

void GifFavoritesPicker::OnChildActivated(Gtk::FlowBoxChild *child) {
    auto *item = dynamic_cast<GifPickerItem *>(child);
    if (item == nullptr) return;

    const auto &gif = item->GetGIF();
    if (gif.Key.empty()) return;

    m_signal_gif_chosen.emit(gif.Key);
    popdown();
}

GifFavoritesPicker::type_signal_gif_chosen GifFavoritesPicker::signal_gif_chosen() {
    return m_signal_gif_chosen;
}
