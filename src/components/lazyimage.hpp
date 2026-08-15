#pragma once
#include <string>
#include <gtkmm/image.h>
#include <gdkmm/pixbufanimation.h>
#include <sigc++/connection.h>

// loads an image only when the widget is drawn for the first time
class LazyImage : public Gtk::Image {
public:
    LazyImage(int w, int h, bool use_placeholder = true);
    LazyImage(std::string url, int w, int h, bool use_placeholder = true);
    ~LazyImage() override;

    void SetAnimated(bool is_animated);
    void SetURL(const std::string &url);

private:
    bool OnDraw(const Cairo::RefPtr<Cairo::Context> &context);
    void OnWindowFocusChange(bool focused);
    void ApplyAnimation(const Glib::RefPtr<Gdk::PixbufAnimation> &pb);
    void FreezeAnimation();
    void ThawAnimation();

    bool m_animated = false;
    bool m_needs_request = true;
    bool m_frozen = false;
    std::string m_url;
    int m_width;
    int m_height;
    Glib::RefPtr<Gdk::PixbufAnimation> m_animation;
    sigc::connection m_focus_conn;
};
