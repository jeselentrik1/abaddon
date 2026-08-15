#include "videowidget.hpp"
#include "abaddon.hpp"
#include "util.hpp"

#include <gst/gst.h>
#include <gtk/gtk.h>

VideoWidget::VideoWidget(const std::string &url, const std::string &click_url, int w, int h)
    : m_click_url(click_url) {
    set_size_request(w, h);
    set_halign(Gtk::ALIGN_START);
    set_above_child(true);
    get_style_context()->add_class("video-widget");

    m_focus_conn = Abaddon::Get().signal_main_window_focus_change().connect(
        sigc::mem_fun(*this, &VideoWidget::OnWindowFocusChange));

    // playbin -> gtksink; gtksink exposes a GtkGstWidget (DrawingArea) via "widget"
    m_sink = gst_element_factory_make("gtksink", "sink");
    GstElement *playbin = gst_element_factory_make("playbin", "play");

    if (!playbin || !m_sink) {
        if (playbin) gst_object_unref(playbin);
        if (m_sink) gst_object_unref(m_sink);
        m_sink = nullptr;
        m_pipeline = nullptr;
        auto *label = Gtk::manage(new Gtk::Label);
        label->set_markup("<a href=\"" + Glib::Markup::escape_text(url) + "\">video</a>");
        label->set_halign(Gtk::ALIGN_START);
        add(*label);
        show_all();
        return;
    }

    g_object_set(G_OBJECT(playbin), "uri", url.c_str(), "mute", TRUE, nullptr);
    g_object_set(G_OBJECT(playbin), "video-sink", m_sink, nullptr);
    m_pipeline = playbin;

    // Pack the sink widget into this EventBox. Do NOT start playback yet —
    // gtksink creates a toplevel titled "Gtk+ Cairo renderer" if the widget
    // is not inside a real GtkWindow when it goes to PLAYING.
    GtkWidget *video_widget = nullptr;
    g_object_get(G_OBJECT(m_sink), "widget", &video_widget, nullptr);

    if (video_widget != nullptr) {
        gtk_widget_set_size_request(video_widget, w, h);
        gtk_widget_set_hexpand(video_widget, FALSE);
        gtk_widget_set_vexpand(video_widget, FALSE);
        gtk_container_add(GTK_CONTAINER(gobj()), video_widget);
        gtk_widget_show(video_widget);
        g_object_unref(video_widget);
    }

    GstBus *bus = gst_element_get_bus(GST_ELEMENT(playbin));
    m_bus_watch_id = gst_bus_add_watch(bus, &VideoWidget::OnGstMessage, this);
    gst_object_unref(bus);

    signal_button_release_event().connect([this](GdkEventButton *ev) -> bool {
        if (ev->type == GDK_BUTTON_RELEASE && ev->button == GDK_BUTTON_PRIMARY) {
            if (!m_click_url.empty())
                LaunchBrowser(m_click_url);
            return true;
        }
        return false;
    });

    // Pre-roll to READY only; PLAYING waits until we're in a toplevel window.
    gst_element_set_state(GST_ELEMENT(playbin), GST_STATE_READY);
    show_all();
}

VideoWidget::~VideoWidget() {
    m_focus_conn.disconnect();
    if (m_bus_watch_id != 0) {
        g_source_remove(m_bus_watch_id);
        m_bus_watch_id = 0;
    }
    if (m_pipeline != nullptr) {
        gst_element_set_state(GST_ELEMENT(m_pipeline), GST_STATE_NULL);
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
    }
    m_sink = nullptr; // owned by playbin
}

void VideoWidget::StartPlayback() {
    if (m_pipeline == nullptr || m_started)
        return;

    // Only start once we have a real toplevel; otherwise gtksink will
    // reparent us into its own "Gtk+ Cairo renderer" window.
    GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(gobj()));
    if (toplevel == nullptr || !gtk_widget_is_toplevel(toplevel))
        return;

    m_started = true;
    if (!Abaddon::Get().IsMainWindowActive()) {
        m_paused_for_focus = true;
        gst_element_set_state(GST_ELEMENT(m_pipeline), GST_STATE_PAUSED);
        return;
    }

    m_paused_for_focus = false;
    gst_element_set_state(GST_ELEMENT(m_pipeline), GST_STATE_PLAYING);
}

void VideoWidget::PausePlayback() {
    if (m_pipeline == nullptr || !m_started)
        return;
    gst_element_set_state(GST_ELEMENT(m_pipeline), GST_STATE_PAUSED);
}

void VideoWidget::ResumePlayback() {
    if (m_pipeline == nullptr || !m_started)
        return;
    if (!get_mapped())
        return;
    if (!Abaddon::Get().IsMainWindowActive())
        return;
    m_paused_for_focus = false;
    gst_element_set_state(GST_ELEMENT(m_pipeline), GST_STATE_PLAYING);
}

void VideoWidget::OnWindowFocusChange(bool focused) {
    if (!m_started || m_pipeline == nullptr)
        return;
    if (focused) {
        ResumePlayback();
    } else {
        m_paused_for_focus = true;
        PausePlayback();
    }
}

void VideoWidget::on_realize() {
    Gtk::EventBox::on_realize();
    StartPlayback();
}

void VideoWidget::on_map() {
    Gtk::EventBox::on_map();
    StartPlayback();
    ResumePlayback();
}

void VideoWidget::on_unmap() {
    PausePlayback();
    Gtk::EventBox::on_unmap();
}

gboolean VideoWidget::OnGstMessage(GstBus *bus, GstMessage *msg, gpointer user_data) {
    auto *self = static_cast<VideoWidget *>(user_data);

    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError *err = nullptr;
            gchar *debug = nullptr;
            gst_message_parse_error(msg, &err, &debug);
            g_printerr("VideoWidget error: %s\n", err->message);
            g_error_free(err);
            g_free(debug);
            if (self->m_pipeline != nullptr)
                gst_element_set_state(GST_ELEMENT(self->m_pipeline), GST_STATE_NULL);
        } break;
        case GST_MESSAGE_EOS:
            // Loop GIF-like videos (only while focused / playing)
            if (!self->m_paused_for_focus && Abaddon::Get().IsMainWindowActive()) {
                gst_element_seek_simple(self->m_pipeline, GST_FORMAT_TIME,
                                        static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                                        0);
            }
            break;
        default:
            break;
    }
    return TRUE;
}
