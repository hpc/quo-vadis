/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */

/**
 * @file test-rmi.cc
 */

#include "quo-vadis.h"
#include "qvi-utils.h"
#include "qvi-hwloc.h"
#include "qvi-rmi.h"

static int
server(
    const char *url
) {
    printf("# [%d] Starting Server (%s)\n", getpid(), url);

    char const *ers = NULL;
    char *path = nullptr;
    qvi_rmi_config config;

    qvi_rmi_server *server = NULL;
    int rc = qvi_new(&server);
    if (rc != QV_SUCCESS) {
        ers = "qvi_new(&server) failed";
        goto out;
    }

    qvi_hwloc_t *hwloc;
    rc = qvi_hwloc_new(&hwloc);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_new() failed";
        goto out;
    }

    rc = qvi_hwloc_topology_init(hwloc, NULL);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_topology_init() failed";
        goto out;
    }

    rc = qvi_hwloc_topology_load(hwloc);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_topology_load() failed";
        goto out;
    }

    config.url = std::string(url);
    config.hwloc = hwloc;

    rc = qvi_hwloc_topology_export(
        hwloc, qvi_tmpdir().c_str(), &path
    );
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_topology_export() failed";
        goto out;
    }
    config.hwtopo_path = std::string(path);
    free(path);

    rc = server->configure(config);
    if (rc != QV_SUCCESS) {
        ers = "server->configure() failed";
        goto out;
    }

    rc = server->start();
    if (rc != QV_SUCCESS) {
        ers = "qvi_rmi_server_start() failed";
        goto out;
    }
    printf("# [%d] Server Started\n", getpid());
out:
    qvi_delete(&server);
    qvi_hwloc_delete(&hwloc);
    if (ers) {
        fprintf(stderr, "\n%s (rc=%d, %s)\n", ers, rc, qv_strerr(rc));
        return 1;
    }
    return 0;
}

static int
get_portno(
    char *url,
    int *portno
) {
    char *pos = strrchr(url, ':');
    if (!pos) return 1;

    char *ports = pos + 1;
    *portno = atoi(ports);
    return 0;
}

static int
client(
    char *url,
    bool send_shutdown_msg
) {
    printf("# [%d] Starting Client (%s)\n", getpid(), url);

    char const *ers = NULL;
    int portno = 0;
    pid_t who = qvi_gettid();
    hwloc_bitmap_t bitmap = NULL;

    qvi_rmi_client *client = NULL;
    int rc = qvi_new(&client);
    if (rc != QV_SUCCESS) {
        ers = "qvi_new(&client) failed";
        goto out;
    }

    rc = get_portno(url, &portno);
    if (rc != 0) {
        ers = "get_portno() failed";
        goto out;
    }

    rc = client->connect(url, portno);
    if (rc != QV_SUCCESS) {
        ers = "client->connect() failed";
        goto out;
    }

    rc = client->get_cpubind(who, &bitmap);
    if (rc != QV_SUCCESS) {
        ers = "client->cpubind() failed";
        goto out;
    }

    char *res;
    qvi_hwloc_bitmap_asprintf(bitmap, &res);
    printf("# [%d] cpubind = %s\n", who, res);
    hwloc_bitmap_free(bitmap);
    free(res);

    if (send_shutdown_msg) {
        rc = client->send_shutdown_message();
    }
out:
    qvi_delete(&client);
    if (ers) {
        fprintf(stderr, "\n%s (rc=%d, %s)\n", ers, rc, qv_strerr(rc));
        return 1;
    }
    return 0;
}

static void
usage(const char *appn)
{
    fprintf(stderr, "Usage: %s URL -s|-c\n", appn);
}

int
main(
    int argc,
    char **argv
) {
    int rc = 0;

    setbuf(stdout, NULL);

    if (argc != 3) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (strcmp(argv[2], "-s") == 0) {
        rc = server(argv[1]);
    }
    else if (strcmp(argv[2], "-c") == 0) {
        rc = client(argv[1], false);
    }
    else if (strcmp(argv[2], "-cc") == 0) {
        rc = client(argv[1], true);
    }
    else {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    return (rc == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
