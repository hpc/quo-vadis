/*
 * Copyright (c) 2020-2022 Triad National Security, LLC
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
#include "qvi-rsmi.h"

/** Device list type. */
using qvi_hwloc_dev_list_t = std::vector<qvi_hwloc_device_t *>;
/** Maps a string identifier to a device. */
using qvi_hwloc_dev_map_t = std::unordered_map<
    std::string,
    qvi_hwloc_device_t *
>;
/** Set of device identifiers. */
using qvi_hwloc_dev_id_set_t = std::unordered_set<std::string>;

typedef enum qvi_hwloc_task_xop_obj_e {
    QVI_HWLOC_TASK_INTERSECTS_OBJ = 0,
    QVI_HWLOC_TASK_ISINCLUDED_IN_OBJ
} qvi_hwloc_task_xop_obj_t;

typedef struct qvi_hwloc_objx_s {
    hwloc_obj_type_t objtype;
    hwloc_obj_osdev_type_t osdev_type;
} qvi_hwloc_objx_t;

/** Array of supported device types. */
static const qv_hw_obj_type_t supported_devices[] = {
    QV_HW_OBJ_GPU,
    QV_HW_OBJ_LAST
};

/** ID used for invisible devices. */
static const int QVI_HWLOC_DEVICE_INVISIBLE_ID = -1;

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
    const qvi_hwloc_device_t *a,
    const qvi_hwloc_device_t *b
) {
    return a->visdev_id < b->visdev_id;
}

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
    *depth = HWLOC_TYPE_DEPTH_UNKNOWN;

    qvi_hwloc_objx_t objx;
    const int rc = obj_type_from_external(type, &objx);
    if (rc != QV_SUCCESS) return rc;

    *depth = hwloc_get_type_depth(hwloc->topo, objx.objtype);
    return QV_SUCCESS;
}

static int
obj_get_by_type(
    qvi_hwloc_t *hwloc,
    qv_hw_obj_type_t type,
    int type_index,
    hwloc_obj_t *out_obj
) {
    qvi_hwloc_objx_t objx;
    int rc = obj_type_from_external(type, &objx);
    if (rc != QV_SUCCESS) return rc;

    const uint_t tiau = (uint_t)type_index;
    *out_obj = hwloc_get_obj_by_type(hwloc->topo, objx.objtype, tiau);
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

    qvi_hwloc_t *ihwl = qvi_new qvi_hwloc_t();
    if (!ihwl) {
        rc = QV_ERR_OOR;
        goto out;
    }

    ihwl->device_ids = qvi_new qvi_hwloc_dev_id_set_t();
    if (!ihwl->device_ids) {
        rc = QV_ERR_OOR;
        goto out;
    }

    ihwl->devices = qvi_new qvi_hwloc_dev_list_t();
    if (!ihwl->devices) {
        rc = QV_ERR_OOR;
        goto out;
    }

    ihwl->gpus = qvi_new qvi_hwloc_dev_list_t();
    if (!ihwl->gpus) {
        rc = QV_ERR_OOR;
        goto out;
    }

    ihwl->nics = qvi_new qvi_hwloc_dev_list_t();
    if (!ihwl->nics) {
        rc = QV_ERR_OOR;
    }
out:
    if (rc != QV_SUCCESS) qvi_hwloc_free(&ihwl);
    *hwl = ihwl;
    return QV_SUCCESS;
}

static void
dev_list_free(
    qvi_hwloc_dev_list_t **devl
) {
    if (!devl) return;
    qvi_hwloc_dev_list_t *idevl = *devl;
    if (!idevl) goto out;
    for (auto &dev : *idevl) {
        qvi_hwloc_device_free(&dev);
    }
    delete idevl;
out:
    *devl = nullptr;
}

static void
dev_map_free(
    qvi_hwloc_dev_map_t **devm
) {
    if (!devm) return;
    qvi_hwloc_dev_map_t *idevm = *devm;
    if (!idevm) goto out;
    for (auto &mapi : *idevm) {
        qvi_hwloc_device_free(&mapi.second);
    }
    delete idevm;
out:
    *devm = nullptr;
}

static void
qvi_hwloc_dev_id_set_free(
    qvi_hwloc_dev_id_set_t **set
) {
    if (!set) return;
    qvi_hwloc_dev_id_set_t *iset = *set;
    if (!iset) goto out;
    delete iset;
out:
    *set = nullptr;
}

void
qvi_hwloc_free(
    qvi_hwloc_t **hwl
) {
    if (!hwl) return;
    qvi_hwloc_t *ihwl = *hwl;
    if (!ihwl) goto out;
    qvi_hwloc_dev_id_set_free(&ihwl->device_ids);
    dev_list_free(&ihwl->devices);
    dev_list_free(&ihwl->gpus);
    dev_list_free(&ihwl->nics);
    if (ihwl->topo) hwloc_topology_destroy(ihwl->topo);
    if (ihwl->topo_file) free(ihwl->topo_file);
    delete ihwl;
out:
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
    const hwloc_obj_osdev_type_t type = device->objx.osdev_type;
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
    if (sscanf(device->name, "rsmi%d", &id) == 1) {
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
    cstr_t pci_bus_id,
    qvi_hwloc_device_t *device
) {
    // Set objx types.
    device->objx.objtype = HWLOC_OBJ_OS_DEVICE;
    // For our purposes a HWLOC_OBJ_OSDEV_COPROC is the same as a
    // HWLOC_OBJ_OSDEV_GPU, so adjust accordingly.
    if (obj->attr->osdev.type == HWLOC_OBJ_OSDEV_COPROC) {
        device->objx.osdev_type = HWLOC_OBJ_OSDEV_GPU;
    }
    else {
        device->objx.osdev_type = obj->attr->osdev.type;
    }
    // Set vendor ID.
    device->vendor_id = pci_obj->attr->pcidev.vendor_id;
    // Save device name.
    int nw = snprintf(
        device->name,
        QVI_HWLOC_DEV_NAME_BUFF_SIZE,
        "%s", obj->name
    );
    if (nw >= QVI_HWLOC_DEV_NAME_BUFF_SIZE) {
        return QV_ERR_INTERNAL;
    }
    // Set the PCI bus ID.
    nw = snprintf(
        device->pci_bus_id,
        QVI_HWLOC_PCI_BUS_ID_BUFF_SIZE,
        "%s", pci_bus_id
    );
    if (nw >= QVI_HWLOC_PCI_BUS_ID_BUFF_SIZE) {
        return QV_ERR_INTERNAL;
    }
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
        int nw = snprintf(
            device->uuid, QVI_HWLOC_UUID_BUFF_SIZE, "%s",
            hwloc_obj_get_info_by_name(obj, "AMDUUID")
        );
        if (nw >= QVI_HWLOC_UUID_BUFF_SIZE) {
            return QV_ERR_INTERNAL;
        }
        return qvi_hwloc_rsmi_get_device_cpuset_by_device_id(
            hwl,
            device->smi,
            device->cpuset
        );
    }
    //
    if (sscanf(obj->name, "nvml%d", &id) == 1) {
        device->smi = id;
        int nw = snprintf(
            device->uuid, QVI_HWLOC_UUID_BUFF_SIZE, "%s",
            hwloc_obj_get_info_by_name(obj, "NVIDIAUUID")
        );
        if (nw >= QVI_HWLOC_UUID_BUFF_SIZE) {
            return QV_ERR_INTERNAL;
        }
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
        const hwloc_obj_osdev_type_t type = obj->attr->osdev.type;
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
        qvi_hwloc_device_t *new_dev = nullptr;
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
    // This will maintain a mapping of PCI bus ID to device pointers.
    qvi_hwloc_dev_map_t *devmap = nullptr;
    devmap = qvi_new qvi_hwloc_dev_map_t();
    if (!devmap) {
        return QV_ERR_OOR;
    }

    hwloc_obj_t obj = nullptr;
    while ((obj = hwloc_get_next_osdev(hwl->topo, obj)) != nullptr) {
        const hwloc_obj_osdev_type_t type = obj->attr->osdev.type;
        // Consider only what is listed here.
        if (type != HWLOC_OBJ_OSDEV_GPU &&
            type != HWLOC_OBJ_OSDEV_COPROC) {
            continue;
        }
        // Try to get the PCI object.
        char busid[QVI_HWLOC_PCI_BUS_ID_BUFF_SIZE] = {'\0'};
        hwloc_obj_t pci_obj = get_pci_busid(obj, busid, sizeof(busid));
        if (!pci_obj) continue;

        for (auto &dev : *hwl->devices) {
            // Skip invisible devices.
            if (dev->visdev_id == QVI_HWLOC_DEVICE_INVISIBLE_ID) continue;
            // Skip if this is not the PCI bus ID we are looking for.
            if (strcmp(dev->pci_bus_id, busid) != 0) continue;
            // Set as much device info as we can.
            int rc = set_gpu_device_info(hwl, obj, dev);
            if (rc != QV_SUCCESS) return rc;
            // First, determine if this is a new device?
            auto got = devmap->find(busid);
            // New device (i.e., a new PCI bus ID)
            if (got == devmap->end()) {
                qvi_hwloc_device_t *gpudev = nullptr;
                int rc = qvi_hwloc_device_new(&gpudev);
                if (rc != QV_SUCCESS) return rc;
                // Set base info from current device.
                rc = qvi_hwloc_device_copy(dev, gpudev);
                if (rc != QV_SUCCESS) return rc;
                // Add it to our collection of 'seen' devices.
                devmap->insert({busid, gpudev});
            }
            // A device we have seen before. Try to set as much info as
            // possible. Note that we don't copy because we don't know if the
            // source device has a different amount of information.
            else {
                qvi_hwloc_device_t *gpudev = got->second;
                rc = set_gpu_device_info(hwl, obj, gpudev);
                if (rc != QV_SUCCESS) return rc;
            }
        }
    }
    // Now populate our GPU device list.
    for (const auto &mapi : *devmap) {
        qvi_hwloc_device_t *gpudev = nullptr;
        int rc = qvi_hwloc_device_new(&gpudev);
        if (rc != QV_SUCCESS) return rc;
        // Copy device info.
        rc = qvi_hwloc_device_copy(mapi.second, gpudev);
        if (rc != QV_SUCCESS) return rc;
        // Save copy.
        hwl->gpus->push_back(gpudev);
    }
    // Sort list based on device ID.
    std::sort(
        hwl->gpus->begin(),
        hwl->gpus->end(),
        dev_list_compare_by_visdev_id
    );
    // Free devmap, as it is no longer needed.
    dev_map_free(&devmap);
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
    rc = discover_gpu_devices(hwl);
    if (rc != QV_SUCCESS) return rc;
    rc = discover_nic_devices(hwl);
    if (rc != QV_SUCCESS) return rc;
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
    return discover_devices(hwl);
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
    if (!src || !dest) {
        assert(false);
        return QV_ERR_INTERNAL;
    }

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
    *fd = open(path, O_CREAT | O_RDWR, 0644);
    if (*fd == -1) {
        int err = errno;
        cstr_t ers = "open() failed";
        qvi_log_error("{} {}", ers, qvi_strerr(err));
        return QV_ERR_FILE_IO;
    }
    // We need to publish this file to consumers that are potentially not part
    // of our group. We cannot assume the current umask, so set explicitly.
    int rc = fchmod(*fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (rc == -1) {
        int err = errno;
        cstr_t ers = "fchmod() failed";
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
    cstr_t ers = nullptr;

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
    assert(cpuset);
    char *cpusets = nullptr;
    int rc = qvi_hwloc_bitmap_asprintf(&cpusets, cpuset);
    assert(rc == QV_SUCCESS);
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
    // XXX(skg) In some instances I've noticed that the system's topology cpuset
    // is a strict subset of a process' cpuset, so protect against that case by
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
    int qvrc = QV_SUCCESS;
    int rc = hwloc_set_proc_cpubind(
        hwl->topo,
        qvi_task_id_get_pid(task_id),
        cpuset,
        get_task_cpubind_flags(task_id.type)
    );
    if (rc == -1) {
        qvrc = QV_ERR_NOT_SUPPORTED;
    }
    return qvrc;
}

int
qvi_hwloc_task_get_cpubind_as_string(
    qvi_hwloc_t *hwl,
    qvi_task_id_t task_id,
    char **cpusets
) {
    hwloc_cpuset_t cpuset;
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
    qvi_hwloc_dev_list_t *devs,
    hwloc_const_cpuset_t cpuset,
    int *nobjs
) {
    int ndevs = 0;
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

const qv_hw_obj_type_t *
qvi_hwloc_supported_devices(void) {
    return supported_devices;
}

int
qvi_hwloc_n_supported_devices(void) {
    size_t fsize = sizeof(supported_devices) / sizeof(supported_devices[0]);
    // Adjust to exclude the sentinel value.
    return fsize - 1;
}

int
qvi_hwloc_device_new(
    qvi_hwloc_device_t **dev
) {
    int rc = QV_SUCCESS;

    qvi_hwloc_device_t *idev = qvi_new qvi_hwloc_device_t();
    if (!idev) {
        rc = QV_ERR_OOR;
        goto out;
    }

    rc = qvi_hwloc_bitmap_calloc(&idev->cpuset);
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
    if (!idev) goto out;
    qvi_hwloc_bitmap_free(&idev->cpuset);
    delete idev;
out:
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
    hwloc_const_cpuset_t cpuset,
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
    hwloc_const_cpuset_t cpuset,
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
        devlist, cpuset, devs
    );
}

int
qvi_hwloc_get_device_in_cpuset(
    qvi_hwloc_t *hwl,
    qv_hw_obj_type_t dev_obj,
    int i,
    hwloc_const_cpuset_t cpuset,
    qv_device_id_type_t dev_id_type,
    char **dev_id
) {
    int rc = QV_SUCCESS, nw = 0;

    qvi_hwloc_dev_list_t *devs = qvi_new qvi_hwloc_dev_list_t();
    if (!devs) {
        rc = QV_ERR_OOR;
        goto out;
    }

    rc = get_devices_in_cpuset(hwl, dev_obj, cpuset, devs);
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
    dev_list_free(&devs);
    return rc;
}

/**
 *
 */
static int
split_cpuset_chunk_size(
    qvi_hwloc_t *hwl,
    hwloc_const_cpuset_t cpuset,
    int npieces,
    int *chunk
) {
    int rc = QV_SUCCESS;

    if (npieces <= 0) {
        rc = QV_ERR_INVLD_ARG;
        goto out;
    }

    int npus;
    rc = qvi_hwloc_get_nobjs_in_cpuset(
        hwl, QV_HW_OBJ_PU, cpuset, &npus
    );
out:
    if (rc != QV_SUCCESS) {
        *chunk = 0;
    }
    else {
        *chunk = npus / npieces;
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
    int base,
    int extent,
    hwloc_bitmap_t *result
) {
    // Allocate and zero-out a new bitmap that will encode the split.
    hwloc_bitmap_t iresult = nullptr;
    int rc = qvi_hwloc_bitmap_calloc(&iresult);
    if (rc != QV_SUCCESS) goto out;
    // We use PUs to split resources. Each set bit represents a PU. The number
    // of bits set represents the number of PUs present on the system. The
    // least-significant (right-most) bit represents logical ID 0.
    int pu_depth;
    rc = qvi_hwloc_obj_type_depth(
        hwl, QV_HW_OBJ_PU, &pu_depth
    );
    if (rc != QV_SUCCESS) goto out;
    // Calculate split based on given range.
    for (int i = base; i < base + extent; ++i) {
        hwloc_obj_t dobj;
        rc = qvi_hwloc_get_obj_in_cpuset_by_depth(
            hwl, cpuset, pu_depth, i, &dobj
        );
        if (rc != QV_SUCCESS) break;

        rc = hwloc_bitmap_or(iresult, iresult, dobj->cpuset);
        if (rc != 0) {
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

// TODO(skg) Merge with split_cpuset_by_range().
int
qvi_hwloc_get_cpuset_for_nobjs(
    qvi_hwloc_t *hwl,
    hwloc_const_cpuset_t cpuset,
    qv_hw_obj_type_t obj_type,
    int nobjs,
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
    for (int i = 0; i < nobjs; ++i) {
        hwloc_obj_t dobj;
        rc = qvi_hwloc_get_obj_in_cpuset_by_depth(
            hwl, cpuset, obj_depth, i, &dobj
        );
        if (rc != QV_SUCCESS) break;

        rc = hwloc_bitmap_or(iresult, iresult, dobj->cpuset);
        if (rc != 0) {
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
qvi_hwloc_split_cpuset_by_color(
    qvi_hwloc_t *hwl,
    hwloc_const_cpuset_t cpuset,
    int ncolors,
    int color,
    hwloc_cpuset_t *result
) {
    int chunk = 0;
    int rc = split_cpuset_chunk_size(
        hwl, cpuset, ncolors, &chunk
    );
    if (rc != QV_SUCCESS) goto out;
    // This happens when n > npus. We can't support that split.
    // TODO(skg) Perhaps we can create an empty cpuset that denotes no
    // resources?
    if (chunk == 0) {
        rc = QV_ERR_SPLIT;
        goto out;
    }
    // Group IDs must be < n: 0, 1, ... , ncolors-1.
    // TODO(skg) We could also allow this. That might mean that this processor
    // should not be considered in the split.
    if (color >= ncolors) {
        rc = QV_ERR_SPLIT;
        goto out;
    }

    rc = split_cpuset_by_range(
        hwl, cpuset, chunk * color, chunk, result
    );
out:
    if (rc != QV_SUCCESS) {
        qvi_hwloc_bitmap_free(result);
    }
    return rc;
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
            devlist = hwl->gpus;
            break;
        default:
            rc = QV_ERR_NOT_SUPPORTED;
            break;
    }
    if (rc != QV_SUCCESS) goto out;
    // XXX(skg) This isn't the most efficient way of doing this, but the device
    // lists tend to be small, so just perform a linear search for the given ID.
    for (const auto &dev : *devlist) {
        if (dev->visdev_id != device_id) continue;
        qvi_hwloc_bitmap_calloc(&icpuset);
        rc = qvi_hwloc_bitmap_copy(dev->cpuset, icpuset);
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
