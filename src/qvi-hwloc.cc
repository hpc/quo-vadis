/*
 * Copyright (c) 2020-2021 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-hwloc.cc
 */

#include "qvi-common.h"
#include "qvi-hwloc.h"
#include "qvi-utils.h"

#include "qvi-nvml.h"
#include <unordered_map>

using qvi_hwloc_dev_list_t = std::vector<qvi_hwloc_device_t *>;
using qvi_hwloc_dev_id_set_t = std::unordered_set<std::string>;

typedef enum qvi_hwloc_task_xop_obj_e {
    QVI_HWLOC_TASK_INTERSECTS_OBJ = 0,
    QVI_HWLOC_TASK_ISINCLUDED_IN_OBJ
} qvi_hwloc_task_xop_obj_t;

typedef struct qvi_hwloc_objx_s {
    hwloc_obj_type_t objtype;
    hwloc_obj_osdev_type_t osdev_type;
} qvi_hwloc_objx_t;

/** ID used for invisible devices. */
#define QVI_HWLOC_DEVICE_INVISIBLE_ID -1

typedef struct qvi_hwloc_device_s {
    /** Device cpuset */
    hwloc_cpuset_t cpuset = nullptr;
    /** Internal object type information */
    qvi_hwloc_objx_t objx;
    /** Vendor ID */
    int vendor_id = -1;
    /** */
    int smi = -1;
    /** CUDA/ROCm visible devices ID */
    int visdev_id = QVI_HWLOC_DEVICE_INVISIBLE_ID;
    /** Device name */
    char name[QVI_HWLOC_DEV_NAME_BUFF_SIZE] = {'\0'};
    /** PCI bus ID */
    char pci_bus_id[QVI_HWLOC_PCI_BUS_ID_BUFF_SIZE] = {'\0'};
    /** UUID */
    char uuid[QVI_HWLOC_UUID_BUFF_SIZE] = {'\0'};
} qvi_hwloc_device_t;

typedef struct qvi_hwloc_s {
    /** The cached node topology. */
    hwloc_topology_t topo = nullptr;
    /** Path to exported hardware topology. */
    char *topo_file = nullptr;
    /** Cached set of PCI IDs discovered during topology load. */
    qvi_hwloc_dev_id_set_t *device_ids = nullptr;
    /** Cached devices discovered during topology load. */
    qvi_hwloc_dev_list_t *devices = nullptr;
    /** Cached GPUs discovered during topology load. */
    qvi_hwloc_dev_list_t *gpus = nullptr;
    /** Cached NICs discovered during topology load. */
    qvi_hwloc_dev_list_t *nics = nullptr;
} qvi_hwloc_t;

/**
 *
 */
static inline int
obj_type_from_external(
    qv_hw_obj_type_t external,
    qvi_hwloc_objx_t *objx
) {
    switch(external) {
        case(QV_HW_OBJ_MACHINE):
            objx->objtype = HWLOC_OBJ_MACHINE;
            break;
        case(QV_HW_OBJ_PACKAGE):
            objx->objtype = HWLOC_OBJ_PACKAGE;
            break;
        case(QV_HW_OBJ_CORE):
            objx->objtype = HWLOC_OBJ_CORE;
            break;
        case(QV_HW_OBJ_PU):
            objx->objtype = HWLOC_OBJ_PU;
            break;
        case(QV_HW_OBJ_L1CACHE):
            objx->objtype = HWLOC_OBJ_L1CACHE;
            break;
        case(QV_HW_OBJ_L2CACHE):
            objx->objtype = HWLOC_OBJ_L2CACHE;
            break;
        case(QV_HW_OBJ_L3CACHE):
            objx->objtype = HWLOC_OBJ_L3CACHE;
            break;
        case(QV_HW_OBJ_L4CACHE):
            objx->objtype = HWLOC_OBJ_L4CACHE;
            break;
        case(QV_HW_OBJ_L5CACHE):
            objx->objtype = HWLOC_OBJ_L5CACHE;
            break;
        case(QV_HW_OBJ_NUMANODE):
            objx->objtype = HWLOC_OBJ_NUMANODE;
            break;
        case(QV_HW_OBJ_GPU):
            objx->objtype = HWLOC_OBJ_OS_DEVICE;
            objx->osdev_type = HWLOC_OBJ_OSDEV_GPU;
            break;
        default:
            return QV_ERR_INVLD_ARG;
    }
    return QV_SUCCESS;
}

int
qvi_hwloc_obj_type_depth(
    qvi_hwloc_t *hwloc,
    qv_hw_obj_type_t type,
    int *depth
) {
    qvi_hwloc_objx_t objx;
    int rc = obj_type_from_external(type, &objx);
    if (rc != QV_SUCCESS) return rc;

    int d = hwloc_get_type_depth(hwloc->topo, objx.objtype);
    if (d == HWLOC_TYPE_DEPTH_UNKNOWN) {
        *depth = 0;
    }
    *depth = d;
    return QV_SUCCESS;
}

static int
obj_get_by_type(
    qvi_hwloc_t *hwloc,
    qv_hw_obj_type_t type,
    unsigned type_index,
    hwloc_obj_t *out_obj
) {
    qvi_hwloc_objx_t objx;
    int rc = obj_type_from_external(type, &objx);
    if (rc != QV_SUCCESS) return rc;

    *out_obj = hwloc_get_obj_by_type(hwloc->topo, objx.objtype, type_index);
    if (!*out_obj) {
        // There are a couple of reasons why target_obj may be NULL. If this
        // ever happens and the specified type and obj index are valid, then
        // improve this code.
        qvi_log_error(
            "hwloc_get_obj_by_type() failed. Please submit a bug report."
        );
        return QV_ERR_INTERNAL;
    }
    return QV_SUCCESS;
}

int
qvi_hwloc_new(
    qvi_hwloc_t **hwl
) {
    int rc = QV_SUCCESS;

    qvi_hwloc_t *ihwl = qvi_new qvi_hwloc_t;
    if (!ihwl) {
        rc = QV_ERR_OOR;
        goto out;
    }

    ihwl->device_ids = qvi_new qvi_hwloc_dev_id_set_t;
    if (!ihwl->device_ids) {
        rc = QV_ERR_OOR;
        goto out;
    }

    ihwl->devices = qvi_new qvi_hwloc_dev_list_t;
    if (!ihwl->devices) {
        rc = QV_ERR_OOR;
        goto out;
    }

    ihwl->gpus = qvi_new qvi_hwloc_dev_list_t;
    if (!ihwl->gpus) {
        rc = QV_ERR_OOR;
        goto out;
    }

    ihwl->nics = qvi_new qvi_hwloc_dev_list_t;
    if (!ihwl->nics) {
        rc = QV_ERR_OOR;
        goto out;
    }
out:
    if (rc != QV_SUCCESS) qvi_hwloc_free(&ihwl);
    *hwl = ihwl;
    return QV_SUCCESS;
}

static void
qvi_hwloc_dev_list_free(
    qvi_hwloc_dev_list_t **devl
) {
    if (!devl) return;
    qvi_hwloc_dev_list_t *idevl = *devl;
    if (!idevl) return;
    for (auto &dev : *idevl) {
        qvi_hwloc_device_free(&dev);
    }
    delete idevl;
    *devl = nullptr;
}

static void
qvi_hwloc_dev_id_set_free(
    qvi_hwloc_dev_id_set_t **set
) {
    if (!set) return;
    qvi_hwloc_dev_id_set_t *iset = *set;
    if (!iset) return;
    delete iset;
    *set = nullptr;
}

void
qvi_hwloc_free(
    qvi_hwloc_t **hwl
) {
    if (!hwl) return;
    qvi_hwloc_t *ihwl = *hwl;
    if (!ihwl) return;
    qvi_hwloc_dev_id_set_free(&ihwl->device_ids);
    qvi_hwloc_dev_list_free(&ihwl->devices);
    qvi_hwloc_dev_list_free(&ihwl->gpus);
    qvi_hwloc_dev_list_free(&ihwl->nics);
    if (ihwl->topo) hwloc_topology_destroy(ihwl->topo);
    if (ihwl->topo_file) free(ihwl->topo_file);
    delete ihwl;
    *hwl = nullptr;
}

static int
topo_set_from_xml(
    qvi_hwloc_t *hwl,
    const char *path
) {
    int rc = hwloc_topology_set_xml(hwl->topo, path);
    if (rc == -1) {
        qvi_log_error("hwloc_topology_set_xml() failed");
        return QV_ERR_HWLOC;
    }
    return QV_SUCCESS;
}

int
qvi_hwloc_topology_init(
    qvi_hwloc_t *hwl,
    const char *xml
) {
    int rc = hwloc_topology_init(&hwl->topo);
    if (rc != 0) {
        qvi_log_error("hwloc_topology_init() failed");
        return QV_ERR_HWLOC;
    }
    if (xml) return topo_set_from_xml(hwl, xml);
    return QV_SUCCESS;
}

/**
 * Returns a pointer to the associated PCI object, nullptr otherwise.
 */
static hwloc_obj_t
get_pci_obj(
    hwloc_obj_t dev
) {
    if (dev->type == HWLOC_OBJ_PCI_DEVICE) return dev;
    if (dev->parent->type == HWLOC_OBJ_PCI_DEVICE) return dev->parent;
    return nullptr;
}

/**
 * Get the PCI bus ID of an input device; save it into input string.
 * Returns a pointer to the associated PCI object, nullptr otherwise.
 */
static hwloc_obj_t
get_pci_busid(
    hwloc_obj_t dev,
    char *busid,
    int idsize
) {
    hwloc_obj_t pcidev = get_pci_obj(dev);
    if (!pcidev) return nullptr;

    int nw = snprintf(
        busid, idsize, "%04x:%02x:%02x.%01x",
        pcidev->attr->pcidev.domain,
        pcidev->attr->pcidev.bus,
        pcidev->attr->pcidev.dev,
        pcidev->attr->pcidev.func
    );
    assert(nw < idsize);
    if (nw >= idsize) return nullptr;
    return pcidev;
}

static int
set_visdev_id(
    qvi_hwloc_device_t *device
) {
    hwloc_obj_osdev_type_t type = device->objx.osdev_type;
    // Consider only what is listed here.
    if (type != HWLOC_OBJ_OSDEV_GPU &&
        type != HWLOC_OBJ_OSDEV_COPROC) {
        return QV_SUCCESS;
    }
    // Set visible devices.  Note that these IDs are relative to a
    // particular context, so we need to keep track of that. For example,
    // visdevs=2,3 could be 0,1.  The challenge is in supporting a user's
    // request via (e.g, CUDA_VISIBLE_DEVICES, ROCR_VISIBLE_DEVICES).
    int id = QVI_HWLOC_DEVICE_INVISIBLE_ID, id2 = id;
    if (sscanf(device->name, "cuda%d", &id) == 1) {
        device->visdev_id = id;
        return QV_SUCCESS;
    }
    if (sscanf(device->name, "opencl%dd%d", &id2, &id) == 2) {
        device->visdev_id = id;
        return QV_SUCCESS;
    }
    return QV_SUCCESS;
}

static int
set_general_device_info(
    qvi_hwloc_t *,
    hwloc_obj_t obj,
    hwloc_obj_t pci_obj,
    cstr pci_bus_id,
    qvi_hwloc_device_t *device
) {
    // Set objx types.
    device->objx.objtype = HWLOC_OBJ_OS_DEVICE;
    device->objx.osdev_type = obj->attr->osdev.type;
    // Set vendor ID.
    device->vendor_id = pci_obj->attr->pcidev.vendor_id;
    // Save device name.
    int nw = snprintf(
        device->name, QVI_HWLOC_DEV_NAME_BUFF_SIZE, "%s", obj->name
    );
    if (nw >= QVI_HWLOC_DEV_NAME_BUFF_SIZE) return QV_ERR_INTERNAL;
    // Set the PCI bus ID.
    nw = snprintf(
        device->pci_bus_id, QVI_HWLOC_PCI_BUS_ID_BUFF_SIZE, "%s", pci_bus_id
    );
    if (nw >= QVI_HWLOC_PCI_BUS_ID_BUFF_SIZE) return QV_ERR_INTERNAL;
    // Set visible device ID, if applicable.
    return set_visdev_id(device);
}

static int
set_gpu_device_info(
    qvi_hwloc_t *hwl,
    hwloc_obj_t obj,
    qvi_hwloc_device_t *device
) {
    int id = -1;
    if (sscanf(device->name, "rsmi%d", &id) == 1) {
        device->smi = id;
        int nw = snprintf(
            device->uuid, QVI_HWLOC_UUID_BUFF_SIZE, "%s",
            hwloc_obj_get_info_by_name(obj, "AMDUUID")
        );
        if (nw >= QVI_HWLOC_UUID_BUFF_SIZE) return QV_ERR_INTERNAL;
#if 0
        int hrc = hwloc_rsmi_get_device_cpuset(
            hwl->topo,
            amd_idx++,
            dev->cpuset
        );
#endif
    }
    if (sscanf(obj->name, "nvml%d", &id) == 1) {
        device->smi = id;
        int nw = snprintf(
            device->uuid, QVI_HWLOC_UUID_BUFF_SIZE, "%s",
            hwloc_obj_get_info_by_name(obj, "NVIDIAUUID")
        );
        if (nw >= QVI_HWLOC_UUID_BUFF_SIZE) return QV_ERR_INTERNAL;
        return qvi_hwloc_nvml_get_device_cpuset_by_pci_bus_id(
                hwl,
                device->pci_bus_id,
                device->cpuset
        );
    }
    return QV_SUCCESS;
}

static int
set_of_device_info(
    qvi_hwloc_t *,
    hwloc_obj_t obj,
    qvi_hwloc_device_t *device
) {
    // TODO(skg) Get cpuset, if available.
    int nw = snprintf(
        device->uuid, QVI_HWLOC_UUID_BUFF_SIZE, "%s",
        hwloc_obj_get_info_by_name(obj, "NodeGUID")
    );
    // Internal error because our buffer is too small.
    if (nw >= QVI_HWLOC_UUID_BUFF_SIZE) return QV_ERR_INTERNAL;
    return QV_SUCCESS;
}

/**
 * First pass: discover devices that must be added to the list of devices.
 */
static int
discover_all_devices(
    qvi_hwloc_t *hwl
) {
    hwloc_obj_t obj = nullptr;
    while ((obj = hwloc_get_next_osdev(hwl->topo, obj)) != nullptr) {
        hwloc_obj_osdev_type_t type = obj->attr->osdev.type;
        // Consider only what is listed here.
        if (type != HWLOC_OBJ_OSDEV_GPU &&
            type != HWLOC_OBJ_OSDEV_COPROC &&
            type != HWLOC_OBJ_OSDEV_OPENFABRICS) {
            continue;
        }
        // Try to get the PCI object.
        char busid[QVI_HWLOC_PCI_BUS_ID_BUFF_SIZE] = {'\0'};
        hwloc_obj_t pci_obj = get_pci_busid(obj, busid, sizeof(busid));
        if (!pci_obj) continue;
        // Have we seen this device already? For example, opencl0d0 and cuda0
        // may correspond to the same GPU hardware.
        // insert().second returns whether or not item insertion took place. If
        // true, we have not seen it.
        bool seen = !hwl->device_ids->insert(busid).second;
        if (seen) continue;
        // Add a new device with a unique PCI busid.
        qvi_hwloc_device_t *new_dev;
        int rc = qvi_hwloc_device_new(&new_dev);
        if (rc != QV_SUCCESS) return rc;
        // Save general device info to new device instance.
        rc = set_general_device_info(hwl, obj, pci_obj, busid, new_dev);
        if (rc != QV_SUCCESS) return rc;
        // Add the new device to our list of available devices.
        hwl->devices->push_back(new_dev);
    }
    return QV_SUCCESS;
}

static int
discover_gpu_devices(
    qvi_hwloc_t *hwl
) {
    hwloc_obj_t obj = nullptr;
    while ((obj = hwloc_get_next_osdev(hwl->topo, obj)) != nullptr) {
        if (obj->attr->osdev.type != HWLOC_OBJ_OSDEV_GPU) continue;
        // Try to get the PCI object.
        char busid[QVI_HWLOC_PCI_BUS_ID_BUFF_SIZE] = {'\0'};
        hwloc_obj_t pci_obj = get_pci_busid(obj, busid, sizeof(busid));
        if (!pci_obj) continue;

        for (auto &dev : *hwl->devices) {
            // Skip invisible devices.
            if (dev->visdev_id == QVI_HWLOC_DEVICE_INVISIBLE_ID) continue;
            // Skip if this is not the PCI bus ID we are looking for.
            if (strcmp(dev->pci_bus_id, busid) != 0) continue;
            //
            int rc = set_gpu_device_info(hwl, obj, dev);
            if (rc != QV_SUCCESS) return rc;
            //
            qvi_hwloc_device_t *gpudev;
            rc = qvi_hwloc_device_new(&gpudev);
            if (rc != QV_SUCCESS) return rc;

            rc = qvi_hwloc_device_copy(dev, gpudev);
            if (rc != QV_SUCCESS) return rc;

            hwl->gpus->push_back(gpudev);
        }
    }
    return QV_SUCCESS;
}

static int
discover_nic_devices(
    qvi_hwloc_t *hwl
) {
    hwloc_obj_t obj = nullptr;
    while ((obj = hwloc_get_next_osdev(hwl->topo, obj)) != nullptr) {
        if (obj->attr->osdev.type != HWLOC_OBJ_OSDEV_OPENFABRICS) continue;
        // Try to get the PCI object.
        char busid[QVI_HWLOC_PCI_BUS_ID_BUFF_SIZE] = {'\0'};
        hwloc_obj_t pci_obj = get_pci_busid(obj, busid, sizeof(busid));
        if (!pci_obj) continue;

        for (auto &dev : *hwl->devices) {
            // Skip if this is not the PCI bus ID we are looking for.
            if (strcmp(dev->pci_bus_id, busid) != 0) continue;
            //
            int rc = set_of_device_info(hwl, obj, dev);
            if (rc != QV_SUCCESS) return rc;
            //
            qvi_hwloc_device_t *nicdev;
            rc = qvi_hwloc_device_new(&nicdev);
            if (rc != QV_SUCCESS) return rc;

            rc = qvi_hwloc_device_copy(dev, nicdev);
            if (rc != QV_SUCCESS) return rc;

            hwl->nics->push_back(nicdev);
        }
    }
    return QV_SUCCESS;
}

static int
discover_devices(
    qvi_hwloc_t *hwl
) {
    int rc = discover_all_devices(hwl);
    if (rc != QV_SUCCESS) return rc;
#if 0 // TODO(skg) FIXME
    rc = discover_gpu_devices(hwl);
    if (rc != QV_SUCCESS) return rc;
    rc = discover_nic_devices(hwl);
    if (rc != QV_SUCCESS) return rc;
#endif
    return QV_SUCCESS;
}

int
qvi_hwloc_topology_load(
    qvi_hwloc_t *hwl
) {
    cstr ers = nullptr;
    // Set flags that influence hwloc's behavior.
    static const unsigned int flags = HWLOC_TOPOLOGY_FLAG_IS_THISSYSTEM;
    int rc = hwloc_topology_set_flags(hwl->topo, flags);
    if (rc != 0) {
        ers = "hwloc_topology_set_flags() failed";
        goto out;
    }

    rc = hwloc_topology_set_all_types_filter(
        hwl->topo,
        HWLOC_TYPE_FILTER_KEEP_IMPORTANT
    );
    if (rc != 0) {
        ers = "hwloc_topology_set_all_types_filter() failed";
        goto out;
    }

    rc = hwloc_topology_set_type_filter(
        hwl->topo,
        HWLOC_OBJ_OS_DEVICE,
        HWLOC_TYPE_FILTER_KEEP_IMPORTANT
    );
    if (rc != 0) {
        ers = "hwloc_topology_set_type_filter() failed";
        goto out;
    }

    rc = hwloc_topology_load(hwl->topo);
    if (rc != 0) {
        ers = "hwloc_topology_load() failed";
        goto out;
    }

    rc = discover_devices(hwl);
    if (rc != 0) {
        ers = "discover_devices() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        return QV_ERR_HWLOC;
    }
    return QV_SUCCESS;
}

hwloc_topology_t
qvi_hwloc_topo_get(
    qvi_hwloc_t *hwl
) {
    return hwl->topo;
}

int
qvi_hwloc_topo_is_this_system(
    qvi_hwloc_t *hwl
) {
    return hwloc_topology_is_thissystem(qvi_hwloc_topo_get(hwl));
}

int
qvi_hwloc_bitmap_alloc(
    hwloc_cpuset_t *cpuset
) {
    int rc = QV_SUCCESS;

    *cpuset = hwloc_bitmap_alloc();
    if (!*cpuset) rc = QV_ERR_OOR;

    return rc;
}

int
qvi_hwloc_bitmap_copy(
    hwloc_const_cpuset_t src,
    hwloc_cpuset_t dest
) {
    if (!src || !dest) return QV_ERR_INVLD_ARG;

    if (hwloc_bitmap_copy(dest, src) != 0) {
        return QV_ERR_HWLOC;
    }
    return QV_SUCCESS;
}

static int
topo_fname(
    const char *base,
    char **name
) {
    const int pid = getpid();
    srand(time(nullptr));
    int nw = asprintf(
        name,
        "%s/%s-%s-%d-%d.xml",
        base,
        PACKAGE_NAME,
        "hwtopo",
        pid,
        rand() % 256
    );
    if (nw == -1) {
        *name = nullptr;
        return QV_ERR_OOR;
    }
    return QV_SUCCESS;
}

/**
 *
 */
static int
topo_fopen(
    qvi_hwloc_t *,
    const char *path,
    int *fd
) {
    *fd = open(path, O_CREAT | O_RDWR);
    if (*fd == -1) {
        int err = errno;
        cstr ers = "open() failed";
        qvi_log_error("{} {}", ers, qvi_strerr(err));
        return QV_ERR_FILE_IO;
    }
    // We need to publish this file to consumers that are potentially not part
    // of our group. We cannot assume the current umask, so set explicitly.
    int rc = fchmod(*fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (rc == -1) {
        int err = errno;
        cstr ers = "fchmod() failed";
        qvi_log_error("{} {}", ers, qvi_strerr(err));
        return QV_ERR_FILE_IO;
    }
    return QV_SUCCESS;
}

int
qvi_hwloc_topology_export(
    qvi_hwloc_t *hwl,
    const char *base_path,
    char **path
) {
    int qvrc = QV_SUCCESS, rc = 0, fd = 0;
    cstr ers = nullptr;

    int err;
    bool usable = qvi_path_usable(base_path, &err);
    if (!usable) {
        ers = "Cannot export hardware topology to {} ({})";
        qvi_log_error(ers, base_path, qvi_strerr(err));
        return QV_ERR;
    }

    char *topo_xml;
    int topo_xml_len;
    rc = hwloc_topology_export_xmlbuffer(
        hwl->topo,
        &topo_xml,
        &topo_xml_len,
        0 // We don't need 1.x compatible XML export.
    );
    if (rc == -1) {
        ers = "hwloc_topology_export_xmlbuffer() failed";
        qvrc = QV_ERR_HWLOC;
        goto out;
    }

    qvrc = topo_fname(base_path, &hwl->topo_file);
    if (qvrc != QV_SUCCESS) {
        ers = "topo_fname() failed";
        goto out;
    }

    rc = asprintf(path, "%s", hwl->topo_file);
    if (rc == -1) {
        ers = "asprintf() failed";
        rc = QV_ERR_OOR;
        goto out;
    }

    qvrc = topo_fopen(hwl, hwl->topo_file, &fd);
    if (qvrc != QV_SUCCESS) {
        ers = "topo_fopen() failed";
        goto out;
    }

    rc = write(fd, topo_xml, topo_xml_len);
    if (rc == -1 || rc != topo_xml_len) {
        int err = errno;
        ers = "write() failed";
        qvi_log_error("{} {}", ers, qvi_strerr(err));
        qvrc = QV_ERR_FILE_IO;
        goto out;
    }
out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, qvrc, qv_strerr(qvrc));
    }
    hwloc_free_xmlbuffer(hwl->topo, topo_xml);
    (void)close(fd);
    return qvrc;
}

int
qvi_hwloc_get_nobjs_by_type(
   qvi_hwloc_t *hwloc,
   qv_hw_obj_type_t target_type,
   int *out_nobjs
) {
    int depth;
    int rc = qvi_hwloc_obj_type_depth(hwloc, target_type, &depth);
    if (rc != QV_SUCCESS) return rc;

    *out_nobjs = hwloc_get_nbobjs_by_depth(hwloc->topo, depth);
    return QV_SUCCESS;
}

int
qvi_hwloc_emit_cpubind(
   qvi_hwloc_t *hwl,
   pid_t who
) {
    hwloc_cpuset_t cpuset = nullptr;
    int rc = qvi_hwloc_task_get_cpubind(hwl, who, &cpuset);
    if (rc != QV_SUCCESS) return rc;

    char *cpusets = nullptr;
    rc = qvi_hwloc_bitmap_asprintf(&cpusets, cpuset);
    if (rc != QV_SUCCESS) goto out;

    qvi_log_info("[pid={} tid={}] cpubind={}", who, qvi_gettid(), cpusets);
out:
    if (cpuset) hwloc_bitmap_free(cpuset);
    if (cpusets) free(cpusets);
    return rc;
}

int
qvi_hwloc_bitmap_asprintf(
    char **result,
    hwloc_const_cpuset_t cpuset
) {
    int rc = QV_SUCCESS;
    // Caller is responsible for freeing returned resources.
    char *iresult = nullptr;
    (void)hwloc_bitmap_asprintf(&iresult, cpuset);
    if (!iresult) {
        qvi_log_error("hwloc_bitmap_asprintf() failed");
        rc = QV_ERR_OOR;
    }
    *result = iresult;
    return rc;
}

int
qvi_hwloc_bitmap_sscanf(
    hwloc_cpuset_t cpuset,
    char *str
) {
    if (hwloc_bitmap_sscanf(cpuset, str) != 0) {
        return QV_ERR_HWLOC;
    }
    return QV_SUCCESS;
}

int
qvi_hwloc_task_get_cpubind(
    qvi_hwloc_t *hwl,
    pid_t who,
    hwloc_cpuset_t *out_cpuset
) {
    int qrc = QV_SUCCESS, rc = 0;

    hwloc_cpuset_t cur_bind = nullptr;
    qrc = qvi_hwloc_bitmap_alloc(&cur_bind);
    if (qrc != QV_SUCCESS) return qrc;

    // TODO(skg) Add another routine to also support getting TIDs.
    rc = hwloc_get_proc_cpubind(
        hwl->topo,
        who,
        cur_bind,
        HWLOC_CPUBIND_PROCESS
    );
    if (rc) {
        cstr ers = "hwloc_get_proc_cpubind() failed";
        qvi_log_error("{} with rc={}", ers, rc);
        qrc = QV_ERR_HWLOC;
        goto out;
    }
    *out_cpuset = cur_bind;
out:
    if (qrc != QV_SUCCESS) {
        if (cur_bind) hwloc_bitmap_free(cur_bind);
        *out_cpuset = nullptr;
    }
    return qrc;
}

// TODO(skg) Add support for binding threads, too.
int
qvi_hwloc_task_set_cpubind_from_cpuset(
    qvi_hwloc_t *hwl,
    pid_t who,
    hwloc_const_cpuset_t cpuset
) {
    int qvrc = QV_SUCCESS;

    int rc = hwloc_set_proc_cpubind(
        hwl->topo,
        who,
        cpuset,
        HWLOC_CPUBIND_PROCESS
    );
    if (rc == -1) {
        qvrc = QV_ERR_NOT_SUPPORTED;
    }
    return qvrc;
}

int
qvi_hwloc_task_get_cpubind_as_string(
    qvi_hwloc_t *hwl,
    pid_t who,
    char **cpusets
) {
    hwloc_cpuset_t cpuset;
    int rc = qvi_hwloc_task_get_cpubind(hwl, who, &cpuset);
    if (rc != QV_SUCCESS) return rc;

    rc = qvi_hwloc_bitmap_asprintf(cpusets, cpuset);
    hwloc_bitmap_free(cpuset);
    return rc;
}

/**
 *
 */
static inline int
task_obj_xop_by_type_id(
    qvi_hwloc_t *hwl,
    qv_hw_obj_type_t type,
    pid_t who,
    unsigned type_index,
    qvi_hwloc_task_xop_obj_t opid,
    int *result
) {
    hwloc_obj_t obj;
    int rc = obj_get_by_type(hwl, type, type_index, &obj);
    if (rc != QV_SUCCESS) return rc;

    hwloc_cpuset_t cur_bind;
    rc = qvi_hwloc_task_get_cpubind(hwl, who, &cur_bind);
    if (rc != QV_SUCCESS) return rc;

    switch (opid) {
        case QVI_HWLOC_TASK_INTERSECTS_OBJ: {
            *result = hwloc_bitmap_intersects(cur_bind, obj->cpuset);
            break;
        }
        case QVI_HWLOC_TASK_ISINCLUDED_IN_OBJ: {
            *result = hwloc_bitmap_isincluded(cur_bind, obj->cpuset);
            break;
        }
    }

    hwloc_bitmap_free(cur_bind);
    return QV_SUCCESS;
}

int
qvi_hwloc_task_intersects_obj_by_type_id(
    qvi_hwloc_t *hwl,
    qv_hw_obj_type_t type,
    pid_t who,
    unsigned type_index,
    int *result
) {
    return task_obj_xop_by_type_id(
        hwl,
        type,
        who,
        type_index,
        QVI_HWLOC_TASK_INTERSECTS_OBJ,
        result
    );
}

int
qvi_hwloc_task_isincluded_in_obj_by_type_id(
    qvi_hwloc_t *hwl,
    qv_hw_obj_type_t type,
    pid_t who,
    unsigned type_index,
    int *result
) {
    return task_obj_xop_by_type_id(
        hwl,
        type,
        who,
        type_index,
        QVI_HWLOC_TASK_ISINCLUDED_IN_OBJ,
        result
    );
}

/**
 *
 */
static int
get_nosdevs_in_cpuset(
    qvi_hwloc_t *,
    qvi_hwloc_dev_list_t *devs,
    hwloc_const_cpuset_t cpuset,
    unsigned *nobjs
) {
    unsigned ndevs = 0;
    for (auto &dev : *devs) {
        if (hwloc_bitmap_isincluded(dev->cpuset, cpuset)) ndevs++;
    }
    *nobjs = ndevs;

    return QV_SUCCESS;
}

static int
get_nobjs_in_cpuset(
    qvi_hwloc_t *hwl,
    qv_hw_obj_type_t target_obj,
    hwloc_const_cpuset_t cpuset,
    unsigned *nobjs
) {
    int depth;
    int rc = qvi_hwloc_obj_type_depth(hwl, target_obj, &depth);
    if (rc != QV_SUCCESS) return rc;

    unsigned n = 0;
    hwloc_topology_t topo = hwl->topo;
    hwloc_obj_t obj = nullptr;
    while ((obj = hwloc_get_next_obj_by_depth(topo, depth, obj))) {
        if (!hwloc_bitmap_isincluded(obj->cpuset, cpuset)) continue;
        // Ignore objects with empty sets (can happen when outside of cgroup).
        if (hwloc_bitmap_iszero(obj->cpuset)) continue;
        n++;
    }
    *nobjs = n;
    return QV_SUCCESS;
}

int
qvi_hwloc_get_nobjs_in_cpuset(
    qvi_hwloc_t *hwl,
    qv_hw_obj_type_t target_obj,
    hwloc_const_cpuset_t cpuset,
    unsigned *nobjs
) {
    switch(target_obj) {
        case(QV_HW_OBJ_GPU) :
            return get_nosdevs_in_cpuset(hwl, hwl->gpus, cpuset, nobjs);
        default:
            return get_nobjs_in_cpuset(hwl, target_obj, cpuset, nobjs);
    }
    return QV_ERR_INTERNAL;
}

int
qvi_hwloc_get_obj_in_cpuset_by_depth(
    qvi_hwloc_t *hwl,
    hwloc_const_cpuset_t cpuset,
    int depth,
    unsigned index,
    hwloc_obj_t *result_obj
) {
    hwloc_topology_t topo = hwl->topo;

    bool found = false;
    unsigned i = 0;
    hwloc_obj_t obj = nullptr;
    while ((obj = hwloc_get_next_obj_by_depth(topo, depth, obj))) {
        if (!hwloc_bitmap_isincluded(obj->cpuset, cpuset)) continue;
        // Ignore objects with empty sets (can happen when outside of cgroup).
        if (hwloc_bitmap_iszero(obj->cpuset)) continue;
        if (i == index) {
            *result_obj = obj;
            found = true;
            break;
        }
        i++;
    }
    return (found ? QV_SUCCESS : QV_ERR_HWLOC);
}

int
qvi_hwloc_device_new(
    qvi_hwloc_device_t **dev
) {
    int rc = QV_SUCCESS;

    qvi_hwloc_device_t *idev = qvi_new qvi_hwloc_device_t;
    if (!idev) {
        rc = QV_ERR_OOR;
        goto out;
    }

    rc = qvi_hwloc_bitmap_alloc(&idev->cpuset);
    if (rc != QV_SUCCESS) {
        goto out;
    }
out:
    if (rc != QV_SUCCESS) {
        qvi_hwloc_device_free(&idev);
    }
    *dev = idev;
    return rc;
}

void
qvi_hwloc_device_free(
    qvi_hwloc_device_t **dev
) {
    if (!dev) return;
    qvi_hwloc_device_t *idev = *dev;
    if (!idev) return;
    if (idev->cpuset) hwloc_bitmap_free(idev->cpuset);
    delete idev;
    *dev = nullptr;
}

int
qvi_hwloc_device_copy(
    qvi_hwloc_device_t *src,
    qvi_hwloc_device_t *dest
) {
    int rc = qvi_hwloc_bitmap_copy(src->cpuset, dest->cpuset);
    if (rc != QV_SUCCESS) return rc;

    dest->objx = src->objx;
    dest->vendor_id = src->vendor_id;
    dest->smi = src->smi;
    dest->visdev_id = src->visdev_id;
    memmove(dest->name, src->name, sizeof(src->name));
    memmove(dest->pci_bus_id, src->pci_bus_id, sizeof(src->pci_bus_id));
    memmove(dest->uuid, src->uuid, sizeof(src->uuid));

    return QV_SUCCESS;
}

int
qvi_hwloc_devices_emit(
    qvi_hwloc_t *hwl,
    qv_hw_obj_type_t obj_type
) {
    qvi_hwloc_dev_list_t *devlist = nullptr;
    switch (obj_type) {
        case QV_HW_OBJ_GPU:
            devlist = hwl->gpus;
            break;
        default:
            return QV_ERR_NOT_SUPPORTED;
    }
    for (auto &dev : *devlist) {
        char *cpusets = nullptr;
        int rc = qvi_hwloc_bitmap_asprintf(&cpusets, dev->cpuset);
        if (rc != QV_SUCCESS) return rc;

        qvi_log_info("  Device Name: {}", dev->name);
        qvi_log_info("  Device PCI Bus ID: {}", dev->pci_bus_id);
        qvi_log_info("  Device UUID: {}", dev->uuid);
        qvi_log_info("  Device cpuset: {}", cpusets);
        qvi_log_info("  Device Vendor ID: {}", dev->vendor_id);
        qvi_log_info("  Device SMI: {}", dev->smi);
        qvi_log_info("  Device Visible Device ID: {}\n", dev->visdev_id);

        free(cpusets);
    }
    return QV_SUCCESS;
}

static int
get_devices_in_cpuset_from_dev_list(
    qvi_hwloc_dev_list_t *devlist,
    hwloc_cpuset_t cpuset,
    qvi_hwloc_dev_list_t *devs
) {
    for (auto &dev : *devlist) {
        if (!hwloc_bitmap_isincluded(dev->cpuset, cpuset)) continue;

        qvi_hwloc_device_t *devin;
        int rc = qvi_hwloc_device_new(&devin);
        if (rc != QV_SUCCESS) return rc;

        rc = qvi_hwloc_device_copy(dev, devin);
        if (rc != QV_SUCCESS) {
            qvi_hwloc_device_free(&devin);
            return rc;
        }
        devs->push_back(devin);
    }
    return QV_SUCCESS;
}

/**
 *
 */
static int
get_devices_in_cpuset(
    qvi_hwloc_t *hwl,
    qv_hw_obj_type_t obj_type,
    hwloc_cpuset_t cpuset,
    qvi_hwloc_dev_list_t *devs
) {
    qvi_hwloc_dev_list_t *devlist = nullptr;
    // Make sure that the user provided a valid, supported device type.
    switch (obj_type) {
        // TODO(skg) We can support more devices, so do that eventually.
        case QV_HW_OBJ_GPU:
            devlist = hwl->gpus;
            break;
        default:
            return QV_ERR_NOT_SUPPORTED;
    }

    return get_devices_in_cpuset_from_dev_list(
        devlist,
        cpuset,
        devs
    );
}

int
qvi_hwloc_get_device_in_cpuset(
    qvi_hwloc_t *hwl,
    qv_hw_obj_type_t dev_obj,
    hwloc_cpuset_t cpuset,
    int i,
    qv_device_id_type_t dev_id_type,
    char **dev_id
) {
    int nw = 0;

    qvi_hwloc_dev_list_t *devs = qvi_new qvi_hwloc_dev_list_t;
    if (!devs) return QV_ERR_OOR;

    int rc = get_devices_in_cpuset(hwl, dev_obj, cpuset, devs);
    if (rc != QV_SUCCESS) {
        goto out;
    }

    switch (dev_id_type) {
        case (QV_DEVICE_ID_UUID):
            nw = asprintf(dev_id, "%s", devs->at(i)->uuid);
            break;
        case (QV_DEVICE_ID_PCI_BUS_ID):
            nw = asprintf(dev_id, "%s", devs->at(i)->pci_bus_id);
            break;
        case (QV_DEVICE_ID_ORDINAL):
            nw = asprintf(dev_id, "%d", devs->at(i)->visdev_id);
            break;
        default:
            rc = QV_ERR_INVLD_ARG;
            goto out;
    }
    if (nw == -1) rc = QV_ERR_OOR;
out:
    if (rc != QV_SUCCESS) {
        *dev_id = nullptr;
    }
    qvi_hwloc_dev_list_free(&devs);
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
