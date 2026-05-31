//apt install libtorrent-rasterbar-dev libfltk1.3-dev libboost-thread-dev
//g++ fltorrent.cxx -o fltorrent $(fltk-config --cxxflags --ldflags) -ltorrent-rasterbar -lboost_system -lboost_thread -lpthread

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Progress.H>
#include <FL/Fl_Box.H>
#include <FL/fl_draw.H>

#include <libtorrent/session.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/torrent_status.hpp>
#include <libtorrent/torrent_handle.hpp>

#include <thread>
#include <atomic>
#include <chrono>
#include <sstream>
#include <string>

using namespace libtorrent;

// ----------------------------------------------------
// Session / torrent
// ----------------------------------------------------

session* ses = nullptr;
torrent_handle th;

// ----------------------------------------------------
// State
// ----------------------------------------------------

std::atomic<float> progress_val(0.0f);

std::atomic<long long> downloaded_size(0);
std::atomic<long long> total_size(0);
std::atomic<long long> uploaded_size(0);

std::atomic<double> dl_speed(0);
std::atomic<double> ul_speed(0);

std::atomic<int> seeders(0);
std::atomic<int> peers(0);

std::atomic<bool> running(false);
std::atomic<bool> finished(false);
std::atomic<bool> seeding(false);

// ----------------------------------------------------
// UI
// ----------------------------------------------------

Fl_Input* magnet_input = nullptr;
Fl_Box* info_box = nullptr;
std::string info_text;

// ----------------------------------------------------
// Size formatter
// ----------------------------------------------------

std::string format_size(long long bytes)
{
    const double KB = 1024.0;
    const double MB = KB * 1024.0;
    const double GB = MB * 1024.0;
    const double TB = GB * 1024.0;

    char buf[64];

    if (bytes >= TB)
        snprintf(buf, sizeof(buf), "%.2f TB", bytes / TB);
    else if (bytes >= GB)
        snprintf(buf, sizeof(buf), "%.2f GB", bytes / GB);
    else if (bytes >= MB)
        snprintf(buf, sizeof(buf), "%.2f MB", bytes / MB);
    else if (bytes >= KB)
        snprintf(buf, sizeof(buf), "%.2f KB", bytes / KB);
    else
        snprintf(buf, sizeof(buf), "%lld B", bytes);

    return std::string(buf);
}

// ----------------------------------------------------
// Custom progress bar
// ----------------------------------------------------

class ProgressBar : public Fl_Progress
{
public:
    ProgressBar(int X,int Y,int W,int H)
        : Fl_Progress(X,Y,W,H) {}

    void draw() override
    {
        fl_draw_box(FL_FLAT_BOX,x(),y(),w(),h(),FL_DARK3);

        float v = value();
        if (v < 0) v = 0;
        if (v > 1) v = 1;

        Fl_Color fill;

        if (finished.load())
            fill = FL_GREEN;
        else if (seeding.load())
            fill = FL_DARK_GREEN;
        else
            fill = FL_BLUE;

        int bw = (int)(w() * v);

        fl_draw_box(FL_FLAT_BOX,x(),y(),bw,h(),fill);

        char txt[32];
        snprintf(txt,sizeof(txt),"%d%%",(int)(v*100));

        fl_color(FL_WHITE);
        fl_font(FL_HELVETICA_BOLD,12);
        fl_draw(txt,x(),y(),w(),h(),FL_ALIGN_CENTER);
    }
};

ProgressBar* bar = nullptr;

// ----------------------------------------------------
// Torrent thread (runs forever, updates state)
// ----------------------------------------------------

void torrent_thread(std::string magnet)
{
    try
    {
        add_torrent_params atp = parse_magnet_uri(magnet);
        atp.save_path = "./downloads";

        th = ses->add_torrent(atp);

        running = true;
        finished = false;
        seeding = false;

        while (true)
        {
            torrent_status st = th.status();

            progress_val = st.progress_ppm / 1000000.0f;

            downloaded_size = st.total_done;
            total_size = st.total_wanted;
            uploaded_size = st.total_upload;

            dl_speed = st.download_rate;
            ul_speed = st.upload_rate;

            seeders = st.num_seeds;
            peers = st.num_peers;

            seeding = st.is_seeding;
            finished = (st.progress_ppm >= 1000000);

            std::this_thread::sleep_for(
                std::chrono::milliseconds(500));
        }
    }
    catch (...)
    {
        running = false;
    }
}

// ----------------------------------------------------
// UI update
// ----------------------------------------------------

void ui_update(void*)
{
    float p = progress_val.load();

    bar->value(p);
    bar->redraw();

    double dl = downloaded_size.load();
    double ul = uploaded_size.load();

    std::ostringstream ss;

    ss << "Downloaded: "
       << format_size((long long)dl)
       << " / "
       << format_size(total_size.load())
       << "\n";

    ss << "Uploaded: "
       << format_size((long long)ul)
       << "\n";

    double ratio = (dl > 0) ? (ul / dl) : 0.0;

    ss << "Ratio: "
       << ratio
       << "\n";

    ss << "DL: "
       << dl_speed.load() / 1024.0
       << " KB/s";

    ss << "    UL: "
       << ul_speed.load() / 1024.0
       << " KB/s\n";

    ss << "Seeders: "
       << seeders.load();

    ss << "    Peers: "
       << peers.load()
       << "\n";

    ss << "Connections: "
       << peers.load()
       << "\n";

    if (seeding)
        ss << "Status: seeding";
    else if (finished)
        ss << "Status: completed";
    else if (running)
        ss << "Status: downloading";
    else
        ss << "Status: idle";

    info_text = ss.str();
    fl_font(FL_HELVETICA, 12);
    info_box->copy_label(info_text.c_str());

    Fl::repeat_timeout(0.3, ui_update);
}

// ----------------------------------------------------
// Start download
// ----------------------------------------------------

void start_download(Fl_Widget*, void*)
{
    std::string magnet = magnet_input->value();
    if (magnet.empty()) return;

    std::thread(torrent_thread, magnet).detach();
}

// ----------------------------------------------------
// Main
// ----------------------------------------------------

int main(int argc,char** argv)
{
    ses = new session();

    Fl_Window win(520,340,"FLTK Torrent Client");
    win.begin();

    Fl_Box label(-10,15,120,20,"Magnet:");

    magnet_input = new Fl_Input(20,40,480,30);

    Fl_Button btn(20,100,120,35,"Download");
    btn.callback(start_download);

    bar = new ProgressBar(160,100,340,35);
    bar->minimum(0);
    bar->maximum(1);
    bar->value(0);

    info_box = new Fl_Box(20,150,480,150);
    info_box->box(FL_DOWN_BOX);
    info_box->align(FL_ALIGN_LEFT|FL_ALIGN_TOP|FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
    info_box->copy_label("Idle");

    win.end();
    win.show(argc,argv);

    Fl::add_timeout(0.3,ui_update);

    return Fl::run();
}
