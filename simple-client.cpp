
#include <iostream>
#include <string>
#include <memory>
#include <stdexcept>
#include <cstring>
#include <unistd.h>
#include <sys/mman.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include "os-compatibility.h"

struct simple_client {
    struct wl_display       *display;
    struct wl_registry      *registry;
    struct wl_compositor    *compositor;
    struct wl_buffer        *buffer;
    struct wl_surface       *surface;
    struct wl_shm           *shm;
    struct wl_shell         *shell;
    struct wl_shell_surface *shell_surface;
    void *data;
    int width, height;
};

template<> struct std::default_delete<struct simple_client> {
    void operator()(struct simple_client *client) {
        if (client->registry)
            wl_registry_destroy(client->registry);
        if (client->display)
            wl_display_disconnect(client->display);
        delete client;
    }
};

static void registry_handle_global(
    void *data, struct wl_registry *registry, uint32_t name,
    const char *interface_c, uint32_t version)
{
    struct simple_client *client = (struct simple_client *)data;
    std::string interface = interface_c;
    std::cout << "interface=" << interface << ": name="
        << std::hex << name << std::dec << " version=" << version << std::endl;
    if (interface == "wl_compositor") {
        client->compositor = (struct wl_compositor *)
            wl_registry_bind(registry, name, &wl_compositor_interface, 1);
    } else if (interface == "wl_shell") {
        client->shell = (struct wl_shell *)
            wl_registry_bind(registry, name, &wl_shell_interface, 1);
    } else if (interface == "wl_shm") {
        client->shm = (struct wl_shm *)
            wl_registry_bind(registry, name, &wl_shm_interface, 1);
    }
}

static void handle_ping(void *data, struct wl_shell_surface *shell_surface, uint32_t serial)
{
    wl_shell_surface_pong(shell_surface, serial);
}

static void create_shm_buffer(struct simple_client *client)
{
    struct wl_shm_pool *pool;
    int fd, size, stride;

    stride = client->width * 4;
    size = stride * client->height;

    fd = os_create_anonymous_file(size);
    if (fd < 0) {
        fprintf(stderr, "creating a buffer file for %d B failed: %m\n", size);
        exit(1);
    }

    client->data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (client->data == MAP_FAILED) {
        fprintf(stderr, "mmap failed: %m\n");
        close(fd);
        exit(1);
    }

    pool = wl_shm_create_pool(client->shm, fd, size);
    client->buffer =
        wl_shm_pool_create_buffer(pool, 0,
            client->width, client->height ,stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);

    close(fd);
}

std::unique_ptr<simple_client> simple_client_create()
{
    std::unique_ptr<simple_client> client(new simple_client());
    std::memset(client.get(), 0, sizeof *client);

    client->display = wl_display_connect(nullptr);
    if (!client->display)
        throw std::runtime_error("Cannot connect to Wayland display");

    client->registry = wl_display_get_registry(client->display);
    if (!client->registry)
        throw std::runtime_error("Cannot get registry from Wayland display");
    static struct wl_registry_listener registry_listener = {
        &registry_handle_global, nullptr
    };
    wl_registry_add_listener(client->registry, &registry_listener, client.get());
    wl_display_dispatch(client->display);   // Send all requests (blocking)
    wl_display_roundtrip(client->display);  // Wait until all requests are processed
    // Now registry_handle_global() called

    client->width = 600;
    client->height = 500;
    client->surface = wl_compositor_create_surface(client->compositor);
    client->shell_surface = wl_shell_get_shell_surface(client->shell, client->surface);

    create_shm_buffer(client.get());

    if (client->shell_surface) {
        static const struct wl_shell_surface_listener shell_surface_listener = {
            handle_ping, nullptr, nullptr
        };
        wl_shell_surface_add_listener(
            client->shell_surface, &shell_surface_listener, client.get());
        wl_shell_surface_set_toplevel(client->shell_surface);
    }

    wl_surface_set_user_data(client->surface, client.get());
    wl_shell_surface_set_title(client->shell_surface, "simple-client");

    memset(client->data, 64, client->width * client->height * 4);
    return client;
}

void simple_client_display_surface(struct simple_client *client)
{
    wl_surface_attach(client->surface, client->buffer, 0, 0);
    wl_surface_damage(client->surface, 0, 0, client->width, client->height);
    wl_surface_commit(client->surface);
}

int main(int argc, char **argv)
{
    int exit_code = EXIT_SUCCESS;
    try {
        int ret = 0;
        auto client = simple_client_create();
        simple_client_display_surface(client.get());
        while (ret != -1)
            ret = wl_display_dispatch(client->display);
    } catch (std::exception& e) {
        std::cerr << "[-] Exception: " << e.what() << std::endl;
        exit_code = EXIT_FAILURE;
    }
    std::exit(exit_code);
}
