#pragma once
#include <string>
#include <gtkmm/eventbox.h>
#include <gst/gst.h>
#include <sigc++/connection.h>

class VideoWidget : public Gtk::EventBox {
public:
    VideoWidget(const std::string &url, const std::string &click_url, int w, int h);
    VideoWidget(const VideoWidget &) = delete;
    VideoWidget &operator=(const VideoWidget &) = delete;
    ~VideoWidget() override;

protected:
    void on_realize() override;
    void on_unmap() override;
    void on_map() override;

private:
    static gboolean OnGstMessage(GstBus *bus, GstMessage *msg, gpointer user_data);
    void StartPlayback();
    void PausePlayback();
    void ResumePlayback();
    void OnWindowFocusChange(bool focused);

    GstElement *m_pipeline = nullptr;
    GstElement *m_sink = nullptr;
    std::string m_click_url;
    guint m_bus_watch_id = 0;
    bool m_started = false;
    bool m_paused_for_focus = false;
    sigc::connection m_focus_conn;
};
