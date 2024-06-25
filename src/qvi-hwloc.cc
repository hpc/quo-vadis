/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
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

#include "qvi-hwloc.h"
#include "qvi-utils.h"

#include "qvi-nvml.h"
#include "qvi-rsmi.h"

typedef enum qvi_hwloc_task_xop_obj_e {
    QVI_HWLOC_TASK_INTERSECTS_OBJ = 0,
    QVI_HWLOC_TASK_ISINCLUDED_IN_OBJ
} qvi_hwloc_task_xop_obj_t;

/** Maps a string identifier to a device. */
using qvi_hwloc_dev_map_t = std::unordered_map<
    std::string,
    std::shared_ptr<qvi_hwloc_device_t>
>;

/** Set of device identifiers. */
using qvi_hwloc_dev_id_set_t = std::unordered_set<std::string>;

typedef struct qvi_hwloc_s {
    /** The cached node topology. */
    hwloc_topology_t topo = nullptr;
    /** Path to exported hardware topology. */
    char *topo_file = nullptr;
    /** Cached set of PCI IDs discovered during topology load. */
    qvi_hwloc_dev_id_set_t device_ids;
    /** Cached devices discovered during topology load. */
    qvi_hwloc_dev_list_t devices;
    /** Cached GPUs discovered during topology load. */
    qvi_hwloc_dev_list_t gpus;
    /** Cached NICs discovered during topology load. */
    qvi_hwloc_dev_list_t nics;
    /** Constructor */
    qvi_hwloc_s(void) = default;
    /** Destructor */
    ~qvi_hwloc_s(void)
    {
        if (topo) hwloc_topology_destroy(topo);
        if (topo_file) free(topo_file);
    }
} qvi_hwloc_t;

/**
 *
 */
hwloc_topology_t
qvi_hwloc_get_topo_obj(
    qvi_hwloc_t *hwl
) {
    return hwl->topo;
}

/**
 *
 */
static bool
dev_list_compare_by_visdev_id(
    const std::shared_ptr<qvi_hwloc_device_t> &a,
    const std::shared_ptr<qvi_hwloc_device_t> &b
) {
    return a.get()->id < b.get()->id;
}

hwloc_obj_type_t
qvi_hwloc_get_obj_type(
    qv_hw_obj_type_t external
) {
    switch(external) {
        case(QV_HW_OBJ_MACHINE):
            return HWLOC_OBJ_MACHINE;
        case(QV_HW_OBJ_PACKAGE):
            return HWLOC_OBJ_PACKAGE;
        case(QV_HW_OBJ_CORE):
            return HWLOC_OBJ_CORE;
        case(QV_HW_OBJ_PU):
            return HWLOC_OBJ_PU;
        case(QV_HW_OBJ_L1CACHE):
            return HWLOC_OBJ_L1CACHE;
        case(QV_HW_OBJ_L2CACHE):
            return HWLOC_OBJ_L2CACHE;
        case(QV_HW_OBJ_L3CACHE):
            return HWLOC_OBJ_L3CACHE;
        case(QV_HW_OBJ_L4CACHE):
            return HWLOC_OBJ_L4CACHE;
        case(QV_HW_OBJ_L5CACHE):
            return HWLOC_OBJ_L5CACHE;
        case(QV_HW_OBJ_NUMANODE):
            return HWLOC_OBJ_NUMANODE;
        case(QV_HW_OBJ_GPU):
            return HWLOC_OBJ_OS_DEVICE;
        default:
            // This is an internal development error.
            qvi_abort();
    }
}

static qv_hw_obj_type_t
qvi_hwloc_convert_obj_type(
   hwloc_obj_type_t external
) {
    switch(external) {
        case(HWLOC_OBJ_MACHINE):
             return QV_HW_OBJ_MACHINE;
        case(HWLOC_OBJ_PACKAGE):
             return QV_HW_OBJ_PACKAGE;
        case(HWLOC_OBJ_CORE):
             return QV_HW_OBJ_CORE;
        case(HWLOC_OBJ_PU):
             return QV_HW_OBJ_PU;
        case(HWLOC_OBJ_L1CACHE):
             return QV_HW_OBJ_L1CACHE;
        case(HWLOC_OBJ_L2CACHE):
             return QV_HW_OBJ_L2CACHE;
        case(HWLOC_OBJ_L3CACHE):
             return QV_HW_OBJ_L3CACHE;
        case(HWLOC_OBJ_L4CACHE):
             return QV_HW_OBJ_L4CACHE; 
        case(HWLOC_OBJ_L5CACHE):
             return QV_HW_OBJ_L5CACHE; 
        case(HWLOC_OBJ_NUMANODE):
            return QV_HW_OBJ_NUMANODE;
        case(HWLOC_OBJ_OS_DEVICE):
            return QV_HW_OBJ_GPU;
        default:
            // This is an internal development error.
            qvi_abort();
    }
}

bool
qvi_hwloc_obj_type_is_host_resource(
    qv_hw_obj_type_t type
) {
    switch(type) {
        case(QV_HW_OBJ_MACHINE):
        case(QV_HW_OBJ_PACKAGE):
        case(QV_HW_OBJ_CORE):
        case(QV_HW_OBJ_PU):
        case(QV_HW_OBJ_L1CACHE):
        case(QV_HW_OBJ_L2CACHE):
        case(QV_HW_OBJ_L3CACHE):
        case(QV_HW_OBJ_L4CACHE):
        case(QV_HW_OBJ_L5CACHE):
        case(QV_HW_OBJ_NUMANODE):
            return true;
        case(QV_HW_OBJ_GPU):
        case(QV_HW_OBJ_LAST):
            return false;
        default:
            // This is an internal development error.
            qvi_abort();
    }
}

int
qvi_hwloc_obj_type_depth(
    qvi_hwloc_t *hwloc,
    qv_hw_obj_type_t type,
    int *depth
) {
    *depth = HWLOC_TYPE_DEPTH_UNKNOWN;

    const hwloc_obj_type_t obj_type = qvi_hwloc_get_obj_type(type);
    *depth = hwloc_get_type_depth(hwloc->topo, obj_type);
    return QV_SUCCESS;
}

static int
obj_get_by_type(
    qvi_hwloc_t *hwloc,
    qv_hw_obj_type_t type,
    int type_index,
    hwloc_obj_t *obj
) {
    const hwloc_obj_type_t obj_type = qvi_hwloc_get_obj_type(type);
    hwloc_obj_t iobj = hwloc_get_obj_by_type(
        hwloc->topo, obj_type, (uint_t)type_index
    );
    if (!iobj) {
        // There are a couple of reasons why target_obj may be NULL. If this
        // ever happens and the specified type and obj index are valid, then
        // improve this code.
        qvi_log_error(
            "hwloc_get_obj_by_type() failed. Please submit a bug report."
        );
        *obj = nullptr;
        return QV_ERR_INTERNAL;
    }
    *obj = iobj;
    return QV_SUCCESS;
}

int
qvi_hwloc_new(
    qvi_hwloc_t **hwl
) {
    return qvi_new_rc(hwl);
}

void
qvi_hwloc_free(
    qvi_hwloc_t **hwl
) {
    qvi_delete(hwl);
}

static int
topo_set_from_xml(
    qvi_hwloc_t *hwl,
    const char *path
) {
    const int rc = hwloc_topology_set_xml(hwl->topo, path);
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
    const int rc = hwloc_topology_init(&hwl->topo);
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
    std::string &busid
) {
    hwloc_obj_t pcidev = get_pci_obj(dev);
    if (!pcidev) return nullptr;

    static constexpr int PCI_BUS_ID_BUFF_SIZE = 32;
    char busid_buffer[PCI_BUS_ID_BUFF_SIZE] = {'\0'};
    const int nw = snprintf(
        busid_buffer, PCI_BUS_ID_BUFF_SIZE,
        "%04x:%02x:%02x.%01x",
        pcidev->attr->pcidev.domain,
        pcidev->attr->pcidev.bus,
        pcidev->attr->pcidev.dev,
        pcidev->attr->pcidev.func
    );
    if (nw >= PCI_BUS_ID_BUFF_SIZE) {
        qvi_abort();
    }
    busid = std::string(busid_buffer);
    return pcidev;
}

static int
set_visdev_id(
    qvi_hwloc_device_t *device
) {
    const qv_hw_obj_type_t type = device->type;
    // Consider only what is listed here.
    if (type != QV_HW_OBJ_GPU) {
        return QV_SUCCESS;
    }
    // Set visible devices.  Note that these IDs are relative to a
    // particular context, so we need to keep track of that. For example,
    // visdevs=2,3 could be 0,1.  The challenge is in supporting a user's
    // request via (e.g, CUDA_VISIBLE_DEVICES, ROCR_VISIBLE_DEVICES).
    int id = QVI_HWLOC_DEVICE_INVISIBLE_ID, id2 = id;
    if (sscanf(device->name.c_str(), "cuda%d", &id) == 1) {
        device->id = id;
        return QV_SUCCESS;
    }
    if (sscanf(device->name.c_str(), "rsmi%d", &id) == 1) {
        device->id = id;
        return QV_SUCCESS;
    }
    if (sscanf(device->name.c_str(), "opencl%dd%d", &id2, &id) == 2) {
        device->id = id;
        return QV_SUCCESS;
    }
    return QV_SUCCESS;
}

static int
set_general_device_info(
    qvi_hwloc_t *,
    hwloc_obj_t obj,
    hwloc_obj_t pci_obj,
    cstr_t pci_bus_id,
    qvi_hwloc_device_t *device
) {
    switch (obj->attr->osdev.type) {
        // For our purposes a HWLOC_OBJ_OSDEV_COPROC
        // is the same as a HWLOC_OBJ_OSDEV_GPU.
        case HWLOC_OBJ_OSDEV_GPU:
        case HWLOC_OBJ_OSDEV_COPROC:
            device->type = QV_HW_OBJ_GPU;
            break;
        // TODO(skg) Add other device types.
        default:
            return QV_SUCCESS;
    }
    // Set vendor ID.
    device->vendor_id = pci_obj->attr->pcidev.vendor_id;
    // Save device name.
    device->name = std::string(obj->name);
    // Set the PCI bus ID.
    device->pci_bus_id = std::string(pci_bus_id);
    // Set visible device ID, if applicable.
    return set_visdev_id(device);
}

static int
set_gpu_device_info(
    qvi_hwloc_t *hwl,
    hwloc_obj_t obj,
    qvi_hwloc_device_t *device
) {
    int id = QVI_HWLOC_DEVICE_INVISIBLE_ID;
    //
    if (sscanf(obj->name, "rsmi%d", &id) == 1) {
        device->smi = id;
        device->uuid = std::string(hwloc_obj_get_info_by_name(obj, "AMDUUID"));
        return qvi_hwloc_rsmi_get_device_cpuset_by_device_id(
            hwl, device->smi, device->affinity
        );
    }
    //
    if (sscanf(obj->name, "nvml%d", &id) == 1) {
        device->smi = id;
        device->uuid = std::string(hwloc_obj_get_info_by_name(obj, "NVIDIAUUID"));
        return qvi_hwloc_nvml_get_device_cpuset_by_pci_bus_id(
            hwl, device->pci_bus_id, device->affinity
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
    device->uuid = std::string(hwloc_obj_get_info_by_name(obj, "NodeGUID"));
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
        const hwloc_obj_osdev_type_t type = obj->attr->osdev.type;
        // Consider only what is listed here.
        if (type != HWLOC_OBJ_OSDEV_GPU &&
            type != HWLOC_OBJ_OSDEV_COPROC &&
            type != HWLOC_OBJ_OSDEV_OPENFABRICS) {
            continue;
        }
        // Try to get the PCI object.
        std::string busid;
        hwloc_obj_t pci_obj = get_pci_busid(obj, busid);
        if (!pci_obj) continue;
        // Have we seen this device already? For example, opencl0d0 and cuda0
        // may correspond to the same GPU hardware.
        // insert().second returns whether or not item insertion took place. If
        // true, we have not seen it.
        const bool seen = !hwl->device_ids.insert(busid).second;
        if (seen) continue;
        // Add a new device with a unique PCI busid.
        auto new_dev = std::make_shared<qvi_hwloc_device_t>();
        // Save general device info to new device instance.
        const int rc = set_general_device_info(
            hwl, obj, pci_obj, busid.c_str(), new_dev.get()
        );
        if (rc != QV_SUCCESS) return rc;
        // Add the new device to our list of available devices.
        hwl->devices.push_back(new_dev);
    }
    return QV_SUCCESS;
}

static int
discover_gpu_devices(
    qvi_hwloc_t *hwl
) {
#ifndef QV_GPU_SUPPORT
    return QV_SUCCESS;
#endif
    // This will maintain a mapping of PCI bus ID to device pointers.
    qvi_hwloc_dev_map_t devmap;

    hwloc_obj_t obj = nullptr;
    while ((obj = hwloc_get_next_osdev(hwl->topo, obj)) != nullptr) {
        const hwloc_obj_osdev_type_t type = obj->attr->osdev.type;
        // Consider only what is listed here.
        if (type != HWLOC_OBJ_OSDEV_GPU &&
            type != HWLOC_OBJ_OSDEV_COPROC) {
            continue;
        }
        // Try to get the PCI object.
        std::string busid;
        hwloc_obj_t pci_obj = get_pci_busid(obj, busid);
        if (!pci_obj) continue;

        for (auto &dev : hwl->devices) {
            // Skip invisible devices.
            if (dev->id == QVI_HWLOC_DEVICE_INVISIBLE_ID) continue;
            // Skip if this is not the PCI bus ID we are looking for.
            if (dev->pci_bus_id != busid) continue;
            // Set as much device info as we can.
            int rc = set_gpu_device_info(hwl, obj, dev.get());
            if (rc != QV_SUCCESS) goto out;
            // First, determine if this is a new device?
            auto got = devmap.find(busid);
            // New device (i.e., a new PCI bus ID)
            if (got == devmap.end()) {
                auto gpudev = std::make_shared<qvi_hwloc_device_t>();
                // Set base info from current device.
                rc = qvi_hwloc_device_copy(dev.get(), gpudev.get());
                if (rc != QV_SUCCESS) return rc;
                // Add it to our collection of 'seen' devices.
                devmap.insert({busid, gpudev});
            }
            // A device we have seen before. Try to set as much info as
            // possible. Note that we don't copy because we don't know if the
            // source device has a different amount of information.
            else {
                qvi_hwloc_device_t *gpudev = got->second.get();
                rc = set_gpu_device_info(hwl, obj, gpudev);
                if (rc != QV_SUCCESS) return rc;
            }
        }
    }
    // Now populate our GPU device list.
    for (const auto &mapi : devmap) {
        auto gpudev = std::make_shared<qvi_hwloc_device_t>();
        // Copy device info.
        const int rc = qvi_hwloc_device_copy(
            mapi.second.get(), gpudev.get()
        );
        if (rc != QV_SUCCESS) return rc;
        // Save copy.
        hwl->gpus.push_back(gpudev);
    }
    // Sort list based on device ID.
    std::sort(
        hwl->gpus.begin(),
        hwl->gpus.end(),
        dev_list_compare_by_visdev_id
    );
out:
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
        std::string busid;
        hwloc_obj_t pci_obj = get_pci_busid(obj, busid);
        if (!pci_obj) continue;

        for (auto &dev : hwl->devices) {
            // Skip if this is not the PCI bus ID we are looking for.
            if (dev->pci_bus_id != busid) continue;
            //
            int rc = set_of_device_info(hwl, obj, dev.get());
            if (rc != QV_SUCCESS) return rc;
            //
            auto nicdev = std::make_shared<qvi_hwloc_device_t>();

            rc = qvi_hwloc_device_copy(dev.get(), nicdev.get());
            if (rc != QV_SUCCESS) return rc;
            hwl->nics.push_back(nicdev);
        }
    }
    return QV_SUCCESS;
}

int
qvi_hwloc_topology_load(
    qvi_hwloc_t *hwl
) {
    cstr_t ers = nullptr;
    // Set flags that influence hwloc's behavior.
    static const uint_t flags = HWLOC_TOPOLOGY_FLAG_IS_THISSYSTEM;
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
out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        return QV_ERR_HWLOC;
    }
    return QV_SUCCESS;
}

int
qvi_hwloc_discover_devices(
    qvi_hwloc_t *hwl
) {
    int rc = discover_all_devices(hwl);
    if (rc != QV_SUCCESS) return rc;
    rc = discover_gpu_devices(hwl);
    if (rc != QV_SUCCESS) return rc;
    rc = discover_nic_devices(hwl);
    if (rc != QV_SUCCESS) return rc;
    return QV_SUCCESS;
}

hwloc_topology_t
qvi_hwloc_topo_get(
    qvi_hwloc_t *hwl
) {
    return hwl->topo;
}

hwloc_const_cpuset_t
qvi_hwloc_topo_get_cpuset(
    qvi_hwloc_t *hwl
) {
    return hwloc_topology_get_topology_cpuset(hwl->topo);
}

int
qvi_hwloc_topo_is_this_system(
    qvi_hwloc_t *hwl
) {
    return hwloc_topology_is_thissystem(qvi_hwloc_topo_get(hwl));
}

int
qvi_hwloc_bitmap_calloc(
    hwloc_cpuset_t *cpuset
) {
    *cpuset = hwloc_bitmap_alloc();
    if (!*cpuset) return QV_ERR_OOR;
    hwloc_bitmap_zero(*cpuset);
    return QV_SUCCESS;
}

void
qvi_hwloc_bitmap_free(
    hwloc_cpuset_t *cpuset
) {
    if (!cpuset) return;
    if (*cpuset) hwloc_bitmap_free(*cpuset);
    *cpuset = nullptr;
}

int
qvi_hwloc_bitmap_copy(
    hwloc_const_cpuset_t src,
    hwloc_cpuset_t dest
) {
    assert(src && dest);

    if (hwloc_bitmap_copy(dest, src) != 0) {
        return QV_ERR_HWLOC;
    }
    return QV_SUCCESS;
}

int
qvi_hwloc_bitmap_dup(
    hwloc_const_cpuset_t src,
    hwloc_cpuset_t *dest
) {
    hwloc_cpuset_t idest = nullptr;
    int rc = qvi_hwloc_bitmap_calloc(&idest);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_hwloc_bitmap_copy(src, idest);
out:
    if (rc != QV_SUCCESS) {
        qvi_hwloc_bitmap_free(&idest);
    }
    *dest = idest;
    return rc;
}

int
qvi_hwloc_bitmap_nbits(
    hwloc_const_cpuset_t cpuset,
    size_t *nbits
) {
    *nbits = 0;

    const int inbits = hwloc_bitmap_last(cpuset);
    if (inbits == -1) return QV_ERR_HWLOC;

    *nbits = size_t(inbits) + 1;
    return QV_SUCCESS;
}

static int
topo_fname(
    const char *base,
    char **name
) {
    const int pid = getpid();
    srand(time(nullptr));
    const int nw = asprintf(
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
    const int ifd = open(path, O_CREAT | O_RDWR, 0644);
    if (ifd == -1) {
        const int err = errno;
        cstr_t ers = "open() failed";
        qvi_log_error("{} {}", ers, qvi_strerr(err));
        return QV_ERR_FILE_IO;
    }
    // We need to publish this file to consumers that are potentially not part
    // of our group. We cannot assume the current umask, so set explicitly.
    const int rc = fchmod(ifd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (rc == -1) {
        const int err = errno;
        cstr_t ers = "fchmod() failed";
        qvi_log_error("{} {}", ers, qvi_strerr(err));
        return QV_ERR_FILE_IO;
    }
    *fd = ifd;
    return QV_SUCCESS;
}

int
qvi_hwloc_topology_export(
    qvi_hwloc_t *hwl,
    const char *base_path,
    char **path
) {
    int qvrc = QV_SUCCESS, rc = 0, fd = 0;
    cstr_t ers = nullptr;

    int err;
    bool usable = qvi_path_usable(base_path, &err);
    if (!usable) {
        ers = "Cannot export hardware topology to {} ({})";
        qvi_log_error(ers, base_path, qvi_strerr(err));
        return QV_ERR;
    }

    char *topo_xml = nullptr;
    int topo_xml_len = 0;
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
        qvrc = QV_ERR_OOR;
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
    int depth = HWLOC_TYPE_DEPTH_UNKNOWN;
    const int rc = qvi_hwloc_obj_type_depth(hwloc, target_type, &depth);
    if (rc != QV_SUCCESS) return rc;

    *out_nobjs = hwloc_get_nbobjs_by_depth(hwloc->topo, depth);
    return rc;
}

int
qvi_hwloc_emit_cpubind(
   qvi_hwloc_t  *hwl,
   qvi_task_id_t task_id
) {
    hwloc_cpuset_t cpuset = nullptr;
    int rc = qvi_hwloc_task_get_cpubind(hwl, task_id, &cpuset);
    if (rc != QV_SUCCESS) return rc;

    char *cpusets = nullptr;
    rc = qvi_hwloc_bitmap_asprintf(&cpusets, cpuset);
    if (rc != QV_SUCCESS) goto out;

    qvi_log_info(
        "[pid={} tid={}] cpubind={}",
        qvi_task_id_get_pid(task_id),
        qvi_gettid(),
        cpusets
    );
out:
    qvi_hwloc_bitmap_free(&cpuset);
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
qvi_hwloc_bitmap_list_asprintf(
    char **result,
    hwloc_const_cpuset_t cpuset
) {
    int rc = QV_SUCCESS;
    // Caller is responsible for freeing returned resources.
    char *iresult = nullptr;
    (void)hwloc_bitmap_list_asprintf(&iresult, cpuset);
    if (!iresult) {
        qvi_log_error("hwloc_bitmap_list_asprintf() failed");
        rc = QV_ERR_OOR;
    }
    *result = iresult;
    return rc;
}

void
qvi_hwloc_cpuset_debug(
    cstr_t msg,
    hwloc_const_cpuset_t cpuset
) {
#if QVI_DEBUG_MODE == 0
    QVI_UNUSED(msg);
#endif

    if (!cpuset) {
        qvi_abort();
    }

    char *cpusets = nullptr;
    int rc = qvi_hwloc_bitmap_asprintf(&cpusets, cpuset);
    if (rc != QV_SUCCESS) {
        qvi_abort();
    }

    qvi_log_debug("{} CPUSET={}", msg, cpusets);
    free(cpusets);
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

static int
get_task_cpubind_flags(
    qvi_task_type_t task_type
) {
#ifdef __linux__
    if (task_type == QV_TASK_TYPE_THREAD) return HWLOC_CPUBIND_THREAD;
    return HWLOC_CPUBIND_PROCESS;
#else
    return HWLOC_CPUBIND_PROCESS;
#endif
}

static int
get_proc_cpubind(
    qvi_hwloc_t *hwl,
    qvi_task_id_t task_id,
    hwloc_cpuset_t cpuset
) {
    int rc = hwloc_get_proc_cpubind(
        hwl->topo,
        task_id.who,
        cpuset,
        get_task_cpubind_flags(task_id.type)
    );
    if (rc != 0) return QV_ERR_HWLOC;
    // Note in some instances I've noticed that the system's topology cpuset is
    // a strict subset of a process' cpuset, so protect against that case by
    // setting the process' cpuset to the system topology's if the above case is
    // what we are dealing.
    hwloc_const_cpuset_t tcpuset = qvi_hwloc_topo_get_cpuset(hwl);
    // If the topology's cpuset is less than the process' cpuset, adjust.
    rc = hwloc_bitmap_compare(tcpuset, cpuset);
    if (rc == -1) {
        return qvi_hwloc_bitmap_copy(tcpuset, cpuset);
    }
    // Else just use the one that was gathered above.
    return QV_SUCCESS;
}

int
qvi_hwloc_task_get_cpubind(
    qvi_hwloc_t *hwl,
    qvi_task_id_t task_id,
    hwloc_cpuset_t *out_cpuset
) {
    hwloc_cpuset_t cur_bind = nullptr;
    int rc = qvi_hwloc_bitmap_calloc(&cur_bind);
    if (rc != QV_SUCCESS) goto out;

    rc = get_proc_cpubind(hwl, task_id, cur_bind);
out:
    if (rc != QV_SUCCESS) {
        qvi_hwloc_bitmap_free(&cur_bind);
    }
    *out_cpuset = cur_bind;
    return rc;
}

int
qvi_hwloc_task_set_cpubind_from_cpuset(
    qvi_hwloc_t *hwl,
    qvi_task_id_t task_id,
    hwloc_const_cpuset_t cpuset
) {
    const int rc = hwloc_set_proc_cpubind(
        hwl->topo,
        qvi_task_id_get_pid(task_id),
        cpuset,
        get_task_cpubind_flags(task_id.type)
    );
    if (rc == -1) {
        return QV_ERR_NOT_SUPPORTED;
    }
    return QV_SUCCESS;
}

int
qvi_hwloc_task_get_cpubind_as_string(
    qvi_hwloc_t *hwl,
    qvi_task_id_t task_id,
    char **cpusets
) {
    hwloc_cpuset_t cpuset = nullptr;
    int rc = qvi_hwloc_task_get_cpubind(hwl, task_id, &cpuset);
    if (rc != QV_SUCCESS) return rc;

    rc = qvi_hwloc_bitmap_asprintf(cpusets, cpuset);
    qvi_hwloc_bitmap_free(&cpuset);
    return rc;
}

/**
 *
 */
static inline int
task_obj_xop_by_type_id(
    qvi_hwloc_t *hwl,
    qv_hw_obj_type_t type,
    qvi_task_id_t task_id,
    int type_index,
    qvi_hwloc_task_xop_obj_t opid,
    int *result
) {
    hwloc_obj_t obj;
    int rc = obj_get_by_type(hwl, type, type_index, &obj);
    if (rc != QV_SUCCESS) return rc;

    hwloc_cpuset_t cur_bind = nullptr;
    rc = qvi_hwloc_task_get_cpubind(hwl, task_id, &cur_bind);
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
    qvi_hwloc_bitmap_free(&cur_bind);
    return QV_SUCCESS;
}

int
qvi_hwloc_task_intersects_obj_by_type_id(
    qvi_hwloc_t *hwl,
    qv_hw_obj_type_t type,
    qvi_task_id_t task_id,
    int type_index,
    int *result
) {
    return task_obj_xop_by_type_id(
        hwl,
        type,
        task_id,
        type_index,
        QVI_HWLOC_TASK_INTERSECTS_OBJ,
        result
    );
}

int
qvi_hwloc_task_isincluded_in_obj_by_type_id(
    qvi_hwloc_t *hwl,
    qv_hw_obj_type_t type,
    qvi_task_id_t task_id,
    int type_index,
    int *result
) {
    return task_obj_xop_by_type_id(
        hwl,
        type,
        task_id,
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
    const qvi_hwloc_dev_list_t &devs,
    hwloc_const_cpuset_t cpuset,
    int *nobjs
) {
    int ndevs = 0;
    for (auto &dev : devs) {
        if (hwloc_bitmap_isincluded(dev->affinity.data, cpuset)) ndevs++;
    }
    *nobjs = ndevs;

    return QV_SUCCESS;
}

static int
get_nobjs_in_cpuset(
    qvi_hwloc_t *hwl,
    qv_hw_obj_type_t target_obj,
    hwloc_const_cpuset_t cpuset,
    int *nobjs
) {
    int depth;
    int rc = qvi_hwloc_obj_type_depth(hwl, target_obj, &depth);
    if (rc != QV_SUCCESS) return rc;

    int n = 0;
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
    int *nobjs
) {
    switch (target_obj) {
        case(QV_HW_OBJ_GPU) :
            return get_nosdevs_in_cpuset(hwl, hwl->gpus, cpuset, nobjs);
        default:
            return get_nobjs_in_cpuset(hwl, target_obj, cpuset, nobjs);
    }
    return QV_ERR_INTERNAL;
}

int
qvi_hwloc_get_obj_type_in_cpuset(
    qvi_hwloc_t *hwl,
    int npieces,
    hwloc_const_cpuset_t cpuset,
    qv_hw_obj_type_t *target_obj
) {
    hwloc_topology_t topo = hwl->topo;
    hwloc_obj_t obj =  hwloc_get_first_largest_obj_inside_cpuset (topo, cpuset);
    int num = hwloc_get_nbobjs_inside_cpuset_by_type (topo, cpuset, obj->type);
    int depth = obj->depth;
    
    while(!((num > npieces) && (!hwloc_obj_type_is_cache(obj->type)))){
        depth++;
        obj = hwloc_get_obj_inside_cpuset_by_depth (topo, cpuset, depth, 0);
        num = hwloc_get_nbobjs_inside_cpuset_by_type (topo, cpuset, obj->type);
    }

    *target_obj = qvi_hwloc_convert_obj_type(obj->type);
    return QV_SUCCESS;
}

int
qvi_hwloc_get_obj_in_cpuset_by_depth(
    qvi_hwloc_t *hwl,
    hwloc_const_cpuset_t cpuset,
    int depth,
    int index,
    hwloc_obj_t *result_obj
) {
    const hwloc_topology_t topo = hwl->topo;

    bool found = false;
    int i = 0;
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

const std::vector<qv_hw_obj_type_t> &
qvi_hwloc_supported_devices(void) {
    static const std::vector<qv_hw_obj_type_t> supported_devices = {
        QV_HW_OBJ_GPU
    };
    return supported_devices;
}

int
qvi_hwloc_device_new(
    qvi_hwloc_device_t **dev
) {
    return qvi_new_rc(dev);
}

void
qvi_hwloc_device_free(
    qvi_hwloc_device_t **dev
) {
    qvi_delete(dev);
}

int
qvi_hwloc_device_copy(
    qvi_hwloc_device_t *src,
    qvi_hwloc_device_t *dest
) {
    *dest = *src;
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
            devlist = &hwl->gpus;
            break;
        default:
            return QV_ERR_NOT_SUPPORTED;
    }
    for (auto &dev : *devlist) {
        char *cpusets = nullptr;
        int rc = qvi_hwloc_bitmap_asprintf(&cpusets, dev->affinity.data);
        if (rc != QV_SUCCESS) return rc;

        qvi_log_info("  Device Name: {}", dev->name);
        qvi_log_info("  Device PCI Bus ID: {}", dev->pci_bus_id);
        qvi_log_info("  Device UUID: {}", dev->uuid);
        qvi_log_info("  Device affinity: {}", cpusets);
        qvi_log_info("  Device Vendor ID: {}", dev->vendor_id);
        qvi_log_info("  Device SMI: {}", dev->smi);
        qvi_log_info("  Device Visible Device ID: {}\n", dev->id);

        free(cpusets);
    }
    return QV_SUCCESS;
}

static int
get_devices_in_cpuset_from_dev_list(
    const qvi_hwloc_dev_list_t &devlist,
    hwloc_const_cpuset_t cpuset,
    qvi_hwloc_dev_list_t &devs
) {
    for (auto &dev : devlist) {
        if (!hwloc_bitmap_isincluded(dev->affinity.data, cpuset)) continue;

        auto devin = std::make_shared<qvi_hwloc_device_t>();
        const int rc = qvi_hwloc_device_copy(dev.get(), devin.get());
        if (rc != QV_SUCCESS) return rc;
        devs.push_back(devin);
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
    hwloc_const_cpuset_t cpuset,
    qvi_hwloc_dev_list_t &devs
) {
    qvi_hwloc_dev_list_t *devlist = nullptr;
    // Make sure that the user provided a valid, supported device type.
    switch (obj_type) {
        // TODO(skg) We can support more devices, so do that eventually.
        case QV_HW_OBJ_GPU:
            devlist = &hwl->gpus;
            break;
        default:
            return QV_ERR_NOT_SUPPORTED;
    }

    return get_devices_in_cpuset_from_dev_list(
        *devlist, cpuset, devs
    );
}

int
qvi_hwloc_get_devices_in_bitmap(
    qvi_hwloc_t *hwl,
    qv_hw_obj_type_t dev_type,
    const qvi_hwloc_bitmap_s &bitmap,
    qvi_hwloc_dev_list_t &devs
) {
    return get_devices_in_cpuset(hwl, dev_type, bitmap.data, devs);
}

int
qvi_hwloc_get_device_id_in_cpuset(
    qvi_hwloc_t *hwl,
    qv_hw_obj_type_t dev_obj,
    int i,
    hwloc_const_cpuset_t cpuset,
    qv_device_id_type_t dev_id_type,
    char **dev_id
) {
    int rc = QV_SUCCESS, nw = 0;

    qvi_hwloc_dev_list_t devs;
    rc = get_devices_in_cpuset(hwl, dev_obj, cpuset, devs);
    if (rc != QV_SUCCESS) {
        goto out;
    }

    switch (dev_id_type) {
        case (QV_DEVICE_ID_UUID):
            nw = asprintf(dev_id, "%s", devs.at(i)->uuid.c_str());
            break;
        case (QV_DEVICE_ID_PCI_BUS_ID):
            nw = asprintf(dev_id, "%s", devs.at(i)->pci_bus_id.c_str());
            break;
        case (QV_DEVICE_ID_ORDINAL):
            nw = asprintf(dev_id, "%d", devs.at(i)->id);
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
    return rc;
}

/**
 *
 */
static int
split_cpuset_chunk_size(
    qvi_hwloc_t *hwl,
    hwloc_const_cpuset_t cpuset,
    uint_t npieces,
    uint_t *chunk_size
) {
    int npus = 0;
    const int rc = qvi_hwloc_get_nobjs_in_cpuset(
        hwl, QV_HW_OBJ_PU, cpuset, &npus
    );
    if (rc != QV_SUCCESS || npieces == 0 || npus == 0 ) {
        *chunk_size = 0;
    }
    else {
        *chunk_size = npus / npieces;
    }
    return rc;
}

/**
 *
 */
static int
split_cpuset_by_range(
    qvi_hwloc_t *hwl,
    hwloc_const_cpuset_t cpuset,
    uint_t base,
    uint_t extent,
    hwloc_bitmap_t result
) {
    // Zero-out the result bitmap that will encode the split.
    hwloc_bitmap_zero(result);
    // We use PUs to split resources. Each set bit represents a PU. The number
    // of bits set represents the number of PUs present on the system. The
    // least-significant (right-most) bit represents logical ID 0.
    int pu_depth = 0;
    int rc = qvi_hwloc_obj_type_depth(
        hwl, QV_HW_OBJ_PU, &pu_depth
    );
    if (rc != QV_SUCCESS) return rc;
    // Calculate split based on given range.
    for (uint_t i = base; i < base + extent; ++i) {
        hwloc_obj_t dobj;
        rc = qvi_hwloc_get_obj_in_cpuset_by_depth(
            hwl, cpuset, pu_depth, i, &dobj
        );
        if (rc != QV_SUCCESS) break;

        const int orrc = hwloc_bitmap_or(result, result, dobj->cpuset);
        if (orrc != 0) {
            rc = QV_ERR_HWLOC;
            break;
        }
    }
    return rc;
}

int
qvi_hwloc_get_cpuset_for_nobjs(
    qvi_hwloc_t *hwl,
    hwloc_const_cpuset_t cpuset,
    qv_hw_obj_type_t obj_type,
    uint_t nobjs,
    hwloc_cpuset_t *result
) {
    hwloc_bitmap_t iresult = nullptr;
    int rc = qvi_hwloc_bitmap_calloc(&iresult);
    if (rc != QV_SUCCESS) goto out;
    // Get the target object's depth.
    int obj_depth;
    rc = qvi_hwloc_obj_type_depth(
        hwl, obj_type, &obj_depth
    );
    if (rc != QV_SUCCESS) goto out;
    // Calculate cpuset based on number of desired objects.
    for (uint_t i = 0; i < nobjs; ++i) {
        hwloc_obj_t dobj;
        rc = qvi_hwloc_get_obj_in_cpuset_by_depth(
            hwl, cpuset, obj_depth, i, &dobj
        );
        if (rc != QV_SUCCESS) break;

        const int orrc = hwloc_bitmap_or(
            iresult, iresult, dobj->cpuset
        );
        if (orrc != 0) {
            rc = QV_ERR_HWLOC;
            break;
        }
    }
out:
    if (rc != QV_SUCCESS) {
        qvi_hwloc_bitmap_free(&iresult);
    }
    *result = iresult;
    return rc;
}

int
qvi_hwloc_split_cpuset_by_chunk_id(
    qvi_hwloc_t *hwl,
    hwloc_const_cpuset_t cpuset,
    uint_t nchunks,
    uint_t chunk_id,
    hwloc_cpuset_t result
) {
    uint_t chunk_size = 0;
    int rc = split_cpuset_chunk_size(
        hwl, cpuset, nchunks, &chunk_size
    );
    if (rc != QV_SUCCESS) return rc;
    // 0 chunk_size likely caused by nonsensical split request.
    // Chunk IDs must be < nchunks: 0, 1, ... , nchunks-1.
    if (chunk_size == 0 || chunk_id >= nchunks) {
        return QV_ERR_SPLIT;
    }

    return split_cpuset_by_range(
        hwl, cpuset, chunk_size * chunk_id, chunk_size, result
    );
}

int
qvi_hwloc_get_device_affinity(
    qvi_hwloc_t *hwl,
    qv_hw_obj_type_t dev_obj,
    int device_id,
    hwloc_cpuset_t *cpuset
) {
    int rc = QV_SUCCESS;

    qvi_hwloc_dev_list_t *devlist = nullptr;
    hwloc_cpuset_t icpuset = nullptr;

    switch(dev_obj) {
        case(QV_HW_OBJ_GPU) :
            devlist = &hwl->gpus;
            break;
        default:
            rc = QV_ERR_NOT_SUPPORTED;
            break;
    }
    if (rc != QV_SUCCESS) goto out;
    // XXX(skg) This isn't the most efficient way of doing this, but the device
    // lists tend to be small, so just perform a linear search for the given ID.
    for (const auto &dev : *devlist) {
        if (dev->id != device_id) continue;
        rc = qvi_hwloc_bitmap_dup(dev->affinity.data, &icpuset);
        if (rc != QV_SUCCESS) goto out;
    }
    if (!icpuset) rc = QV_ERR_NOT_FOUND;
out:
    if (rc != QV_SUCCESS) {
        qvi_hwloc_bitmap_free(cpuset);
    }
    *cpuset = icpuset;
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
