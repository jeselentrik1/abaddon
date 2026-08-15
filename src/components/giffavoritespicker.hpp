#pragma once
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/entry.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/flowbox.h>
#include <gtkmm/popover.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/spinner.h>
#include <gtkmm/label.h>
#include "discord/favoritegifs.hpp"
#include "lazyimage.hpp"

class GifPickerItem : public Gtk::FlowBoxChild {
public:
    GifPickerItem(const FavoriteGIF &gif, int thumb_w, int thumb_h);

    [[nodiscard]] const FavoriteGIF &GetGIF() const;

private:
    FavoriteGIF m_gif;
    Gtk::EventBox m_ev;
    LazyImage *m_img = nullptr;
};

class GifFavoritesPicker : public Gtk::Popover {
public:
    GifFavoritesPicker();

    void Popup(Gtk::Widget &relative_to);

private:
    void LoadFavorites(bool force_refresh);
    void ClearFlow();
    void RebuildGrid();
    void OnSearchChanged();
    void OnChildActivated(Gtk::FlowBoxChild *child);

    Gtk::Box m_main;
    Gtk::Box m_header;
    Gtk::Label m_title;
    Gtk::Entry m_search;
    Gtk::ScrolledWindow m_scroll;
    Gtk::FlowBox m_flow;
    Gtk::Box m_status_box;
    Gtk::Spinner m_spinner;
    Gtk::Label m_status;

    std::vector<FavoriteGIF> m_gifs;
    bool m_loading = false;

public:
    using type_signal_gif_chosen = sigc::signal<void, Glib::ustring>;
    type_signal_gif_chosen signal_gif_chosen();

private:
    type_signal_gif_chosen m_signal_gif_chosen;
};
