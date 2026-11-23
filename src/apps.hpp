#pragma once

// the mini strip: one per desktop, floating on every workspace
int run_strip(int argc, char** argv);
// a big viewer: a tiled window for one chat, or for the chats on its workspace
int run_view(int argc, char** argv);
// the phone page: no window, a small web server for Tailscale to publish
int run_serve(int argc, char** argv);
